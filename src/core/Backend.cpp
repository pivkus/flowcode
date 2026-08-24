#include "Backend.hpp"
#include "SessionHandle.hpp"
#include "Uuid.hpp"

// TODO: is there an easier way to include all of them?
#include "tools/BashTool.hpp"

Backend::Backend(){

    curl_global_init(CURL_GLOBAL_ALL);
    multi = curl_multi_init();

    auto wakeCoordinator = [this]{
        {
            // hold the lock while setting coord_notified
            std::lock_guard<std::mutex> lock(coord_mutex);
            coord_notified = true;
        }
        coord_cv.notify_one();
    };

    fromUIQueue.setCallback(wakeCoordinator);
    fromToolQueue.setCallback(wakeCoordinator);

    // TODO: Backend and Bridge have a circular reference with these callbacks, make sure they get deleted in a way
    // so this will no be called on a destroyed object
    toNetworkQueue.setCallback([this]{
        // this wakes up the network thread, will be called from main thread, should be thread-safe
        curl_multi_wakeup(multi);
    });

    // Register built-in tools in the global tool registry
    // TODO: factor this out into a function
    tool_registry.add(std::make_unique<BashTool>());

    // jthread prepends stop_token meanining it would call networkWorker(stop_token, this) like this
    // lambda to fix argument order so "this" is first
    network_thread = std::jthread([this](std::stop_token stop){ networkWorker(stop); });
    coordinator_thread = std::jthread([this](std::stop_token stop){ coordinatorWorker(stop); });

    executor_threads.reserve(executor_pool_size);
    for (int i = 0; i < executor_pool_size; ++i){
        executor_threads.emplace_back([this](std::stop_token stop){ executorWorker(stop); });
    }
}

Backend::~Backend(){
    network_thread.request_stop();
    coordinator_thread.request_stop();

    for (auto& t : executor_threads) t.request_stop();
    for (auto& t : executor_threads) t.join();

    curl_multi_wakeup(multi);
    // jthread would only join in its own destructor, after multi is already cleaned up below
    network_thread.join();
    coordinator_thread.join();

    curl_multi_cleanup(multi);
    curl_global_cleanup();
}


void Backend::networkWorker(std::stop_token stop){
    // TODO: this will keep accummulating connections, manage this once I add persistant session storage
    std::unordered_map<Uuid, std::unique_ptr<SessionHandle>> se_cache;

    int still_running = 0;
    while (!stop.stop_requested()){

        while (auto req = toNetworkQueue.dequeue()){
            // Will insert a null unique_ptr if id not in map
            std::unique_ptr<SessionHandle>& slot = se_cache[req->session_id];
            if (!slot){
                try {
                    slot = std::make_unique<SessionHandle>(req->session_id, &fromNetworkQueue);
                } catch (const std::exception &e){
                    // e.g. missing API key; report to the UI instead of crashing the worker
                    using enum fromNetworkMessage::Kind;

                    fromNetworkQueue.enqueue({
                        .session_id = req->session_id,
                        .kind = ERROR,
                        .content = e.what()
                    });
                    se_cache.erase(req->session_id);
                    continue;
                }
            }

            SessionHandle& connection = *slot;
            connection.prepareMessage(*req);
            curl_multi_add_handle(multi, connection.raw());
        }

        curl_multi_perform(multi, &still_running);

        CURLMsg *m;
        int left;
        while ((m = curl_multi_info_read(multi, &left))){
            void *s_ptr;
            curl_easy_getinfo(m->easy_handle, CURLINFO_PRIVATE, &s_ptr);
            SessionHandle& session = *static_cast<SessionHandle* >(s_ptr); // make sure this will always be valid

            // data.result is only valid when msg == CURLMSG_DONE
            if (m->msg == CURLMSG_DONE){
                if (m->data.result != CURLE_OK){
                    session.sendError(curl_easy_strerror(m->data.result));
                } else {
                    session.completeMessage();
                }
                curl_multi_remove_handle(multi, session.raw());
            }
        }

        // will wait if there are no active handles or until wakeup
        int numfds = 0;
        curl_multi_poll(multi, nullptr, 0, 1000, &numfds);
    }
    
}

void Backend::executorWorker(std::stop_token stop){

    while (!stop.stop_requested()){
        auto task = toToolQueue.wait_dequeue(stop);
        if (!task) continue; // woken by a stop request with no work queued

        Tool* tool = tool_registry.get_tool(task->call.name);
        ToolResult result = tool->execute(task->call.args, stop);

        fromToolQueue.enqueue(fromToolMessage{
            .session_id = task->sid,
            .turn_id    = task->tid,
            .call_id    = task->call.call_id,
            .result     = std::move(result)
        });
    }
}

ToolResult bash_tool_testing(const json& args, std::stop_token stop){
    return ToolResult {
        .ok = false,
        .content = "tool is not implemented yet, abort this task and alert user"
    };
}

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
void Backend::executeEffects(Effects&& effects){
    for (auto& e : effects){
        std::visit(overloaded{
            [&](SendRequest& r){
                toNetworkQueue.enqueue({
                    .session_id = r.sid, 
                    .turns = std::move(r.snapshot),
                    .tools = std::move(r.tools) 
                });
            },
            [&](EmitOutput& o){
                toUIQueue.enqueue({
                    .session_id = o.sid,
                    .kind = toUIMessage::Kind::OUTPUT_TOKENS,
                    .content = std::move(o.content)
                });
            },
            [&](EmitReasoning& r){
                toUIQueue.enqueue({
                    .session_id = r.sid,
                    .kind = toUIMessage::Kind::REASONING_TOKENS,
                    .content = std::move(r.content)
                });
            },
            [&](EmitToolStarted& t){
                toUIQueue.enqueue({
                    .session_id = t.sid,
                    .kind = toUIMessage::Kind::TOOL_CALL_STARTED,
                    .content = std::move(t.name)
                });
            },
            [&](EmitToolResult& t){
                toUIQueue.enqueue({
                    .session_id = t.sid,
                    .kind = toUIMessage::Kind::TOOL_CALL_RESULT,
                    .content = t.ok ? "ok" : "failed"
                });
            },
            [&](TurnFinished& t){
                toUIQueue.enqueue({
                    .session_id = t.sid,
                    .kind = toUIMessage::Kind::TURN_FINISHED,
                    .content = ""
                });
            },
            [&](ToolExecute& t){
                toToolQueue.enqueue(std::move(t));
            },
            [&](EmitError& e){
                toUIQueue.enqueue({
                    .session_id = e.sid,
                    .kind = toUIMessage::Kind::ERROR,
                    .content = std::move(e.content)
                });
            },
            [&](EffectNone& e){},
        }, e);
    }
}
void Backend::coordinatorWorker(std::stop_token stop){

    // TODO: this is just a temporary for testing, each session should have its own way to configure allowed tools
    SchemaMap tool_map;
    tool_map["bash"] = tool_registry.get_schema("bash");
    SchemaMapPtr allowed_tools = std::make_shared<const SchemaMap>(std::move(tool_map));

    std::unordered_map<Uuid, Session> sessions;
    while (!stop.stop_requested()){


        while (auto msg = fromUIQueue.dequeue()){
            switch (msg->kind){
                using enum fromUIMessage::Kind;
                
                case CREATE_SESSION:
                    sessions.try_emplace(msg->session_id, msg->session_id, allowed_tools);
                    break;
                case PROMPT_SUBMITED: {
                    Session& s = sessions.at(msg->session_id);
                    Effects effects = s.submitUserTurn(std::move(msg->content));
                    executeEffects(std::move(effects));
                    break;
                }
            }
        }

        while (auto msg = fromNetworkQueue.dequeue()){
            Session& s = sessions.at(msg->session_id);
            Effects effects;

            switch (msg->kind){
    
                case fromNetworkMessage::Kind::OUTPUT_TOKENS: {
                    auto content = std::get<std::string>(msg->content);
                    effects = s.onTextDelta(std::move(content), TokensType::OUTPUT);
                    break;
                }
                case fromNetworkMessage::Kind::REASONING_TOKENS: {
                    auto content = std::get<std::string>(msg->content); 
                    effects = s.onTextDelta(std::move(content), TokensType::REASONING);
                    break;
                }
                case fromNetworkMessage::Kind::TURN_FINISHED: {
                    effects = s.onTurnComplete();
                    break;
                }
                case fromNetworkMessage::Kind::ERROR: {
                    auto content = std::get<std::string>(msg->content);
                    effects = s.onRequestFailed(std::move(content));
                    break;
                }
                case fromNetworkMessage::Kind::TOOL_CALL: {
                    auto content = std::get<ToolCallRequests>(msg->content);
                    effects = s.onToolCallsRequest(std::move(content));
                }
            }

            executeEffects(std::move(effects));
        }

        while (auto msg = fromToolQueue.dequeue()){
            Session& s = sessions.at(msg->session_id);
            Effects effects = s.onToolCallResult(msg->turn_id, msg->call_id, std::move(msg->result));
            executeEffects(std::move(effects));
        }



        // periodically sleep, if there is work or stop wakeup
        std::unique_lock<std::mutex> lock(coord_mutex);
        // coord_notified predicate to prevent lost wakeups
        // wait_for unlocks mutex during sleep, re-acquires when awoken
        coord_cv.wait_for(lock, stop, std::chrono::milliseconds(500), [this]{ return coord_notified; });
        coord_notified = false;
        // lock released here
    }
}