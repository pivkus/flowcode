#include <fstream>
#include <sstream>
#include <print>
#include <stdexcept>

#include <HarnessImpl.hpp>
#include <Log.hpp>

// TODO: is there an easier way to include all of them?
#include <tools/BashTool.hpp>


template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

Harness::Impl::CurlRuntime::CurlRuntime(){
    const auto result = curl_global_init(CURL_GLOBAL_ALL);
    if (result != CURLE_OK){
        throw std::runtime_error(curl_easy_strerror(result));
    }
}

Harness::Impl::CurlRuntime::~CurlRuntime(){
    curl_global_cleanup();
}

Harness::Impl::Impl(std::string system_prompt)
    : system_prompt(std::move(system_prompt))
{

    auto wakeCoordinator = [this]{
        {
            // Hold the lock while setting coord_notified
            std::lock_guard<std::mutex> lock(coord_mutex);
            coord_notified = true;
        }
        coord_cv.notify_one();
    };

    fromUIQueue.setCallback(wakeCoordinator);
    fromToolQueue.setCallback(wakeCoordinator);
    fromIoQueue.setCallback(wakeCoordinator);
    fromNetworkQueue.setCallback(wakeCoordinator);

    // Register built-in tools in the global tool registry
    // TODO: factor this out into a function
    tool_registry.add(std::make_unique<BashTool>());

    coordinator_thread = std::jthread([this](std::stop_token stop){ coordinatorWorker(stop); });
}

Harness::Impl::~Impl(){
    coordinator_thread.request_stop();
    coordinator_thread.join();
}

void Harness::Impl::setMessageCallback(MessageCallback cb){ toUIQueue.setCallback(cb); }
void Harness::Impl::sendMessage(fromUIMessage msg){ fromUIQueue.enqueue(std::move(msg)); }
std::optional<toUIMessage> Harness::Impl::receiveMessage(){ return toUIQueue.dequeue(); }

void Harness::Impl::executeEffects(Effects&& effects){
    for (auto& e : effects){
        std::visit(overloaded{
            [&](SendRequest& r){ toNetworkQueue.enqueue(std::move(r)); },
            [&](OutputTokensDelta& o){ toUIQueue.enqueue(std::move(o)); },
            [&](ReasoningTokensDelta& r){ toUIQueue.enqueue(std::move(r)); },
            [&](EmitToolStarted& t){ toUIQueue.enqueue(std::move(t)); },
            [&](EmitToolResult& t){ toUIQueue.enqueue(std::move(t)); },
            [&](TurnFinished& t){ toUIQueue.enqueue(std::move(t)); },
            [&](ToolExecute& t){ toToolQueue.enqueue(std::move(t)); },
            [&](SessionError& e){ toUIQueue.enqueue(std::move(e)); },
            [&](SessionRetry& r){ toUIQueue.enqueue(std::move(r)); },
            [&](PersistTurns& t){ toIoQueue.enqueue(std::move(t)); },
            [&](ScheduleRetry& r){ network_retries.insert_or_assign(r.sid, std::move(r)); },
            [&](EffectNone& e){},
        }, e);
    }
}

void Harness::Impl::coordinatorWorker(std::stop_token stop){

    SchemaMap tool_map;
    tool_map["bash"] = tool_registry.get_schema("bash");
    SchemaMapPtr allowed_tools = std::make_shared<const SchemaMap>(std::move(tool_map));

    std::unordered_map<Uuid, Session> sessions;
    while (!stop.stop_requested()){

        while (auto msg = fromUIQueue.dequeue()){

            std::visit(overloaded{
                [&](SessionCreation& sc){
                    sessions.try_emplace(sc.sid, sc.sid, allowed_tools, system_prompt);
                },
                [&](PromptSubmission& ps){
                    Session& s = sessions.at(ps.sid);
                    Effects effects = s.submitUserTurn(std::move(ps));
                    executeEffects(std::move(effects));
                },
                [&](ListSessionsReq& ls){
                    toIoQueue.enqueue(std::move(ls));
                },
                [&](LoadSessionReq& ls){
                    toIoQueue.enqueue(std::move(ls));
                }
            }, *msg);
        }

        while (auto msg = fromNetworkQueue.dequeue()){

            Effects effects;
            std::visit(overloaded{
                [&](OutputTokensDelta& ot){
                    Session& s = sessions.at(ot.sid);
                    effects = s.onTextDelta(std::move(ot.delta), TokensType::OUTPUT);
                },
                [&](ReasoningTokensDelta& rt){
                    Session& s = sessions.at(rt.sid);
                    effects = s.onTextDelta(std::move(rt.delta), TokensType::REASONING);
                },
                [&](TurnFinished& tf){
                    Session& s = sessions.at(tf.sid);
                    effects = s.onTurnComplete();
                },
                [&](RequestFailed& e){
                    Session& s = sessions.at(e.sid);
                    effects = s.onRequestFailed(std::move(e));
                },
                [&](ToolCallsMade& tcs){
                    Session& s = sessions.at(tcs.sid);
                    effects = s.onToolCallsRequest(std::move(tcs.calls));
                }
            }, *msg);

            executeEffects(std::move(effects));
        }

        while (auto msg = fromToolQueue.dequeue()){
            Session& s = sessions.at(msg->session_id);
            Effects effects = s.onToolCallResult(msg->turn_id, msg->call_id, std::move(msg->result));
            executeEffects(std::move(effects));
        }

        while (auto msg = fromIoQueue.dequeue()){
            std::visit(overloaded {
                [&](ListSessionsRes& ls){ toUIQueue.enqueue(std::move(ls)); },
                [&](LoadSessionRes& ls){
                    sessions.try_emplace(ls.sid, ls.sid, allowed_tools, ls.history); 
                    toUIQueue.enqueue(std::move(ls));
                },
                [&](GlobalError& e){
                },
                [&](SessionError& e){ toUIQueue.enqueue(std::move(e)); }
            }, *msg);
        }

        std::erase_if(network_retries, [&](const auto& retry){
            if (std::chrono::steady_clock::now() >= retry.second.deadline){
                Session& s = sessions.at(retry.first);
                Effects effects = s.onRetryDue();
                executeEffects(std::move(effects));
                return true;
            }
            return false;
        });

        // periodically sleep, if there is work or stop wakeup
        std::unique_lock<std::mutex> lock(coord_mutex);
        // coord_notified predicate to prevent lost wakeups
        // wait_for unlocks mutex during sleep, re-acquires when awoken
        coord_cv.wait_for(lock, stop, std::chrono::milliseconds(500), [this]{ return coord_notified; });
        coord_notified = false;
        // lock released here
    }
}


Harness::Harness() = default;
Harness::~Harness() = default;

bool Harness::init(){
    if (init_attempted) return impl != nullptr;
    init_attempted = true;

    try {
        std::ifstream file(FLOWCODE_SYSTEM_PROMPT_PATH, std::ios::binary);
        if (!file.is_open()) throw std::runtime_error("Cannot open system prompt");

        std::ostringstream contents;
        char buffer[4096];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0){
            contents.write(buffer, file.gcount());
        }

        if (file.bad() || !file.eof() || !contents || contents.str().find_first_not_of(" \t\r\n") == std::string::npos){
            throw std::runtime_error("Cannot read system prompt or prompt is empty");
        }

        impl = std::make_unique<Impl>(contents.str());
        return true;
    } catch (const std::exception& e){
        debug_print("Harness initialization failed ({}): {}", FLOWCODE_SYSTEM_PROMPT_PATH, e.what());
        return false;
    }
}

Harness::Impl& Harness::initializedImpl(){
    if (!impl) throw std::logic_error("Harness::init() must succeed before using the harness");
    return *impl;
}

void Harness::setMessageCallback(MessageCallback cb){
    initializedImpl().setMessageCallback(std::move(cb));
}
void Harness::pushMessage(fromUIMessage msg){
    initializedImpl().sendMessage(std::move(msg));
}
std::optional<toUIMessage> Harness::popMessage(){
    return initializedImpl().receiveMessage();
}
