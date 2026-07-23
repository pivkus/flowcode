#include "Backend.hpp"
#include "SessionHandle.hpp"

Backend::Backend(){

    curl_global_init(CURL_GLOBAL_ALL);
    multi = curl_multi_init();

    fromUIQueue.setCallback([this]{
        {
            // hold the lock while setting coord_notified
            std::lock_guard<std::mutex> lock(coord_mutex);
            coord_notified = true;
        }
        coord_cv.notify_one();
    });

    // TODO: Backend and Bridge have a circular reference with these callbacks, make sure they get deleted in a way
    // so this will no be called on a destroyed object
    toNetworkQueue.setCallback([this]{
        // this wakes up the network thread, will be called from main thread, should be thread-safe
        curl_multi_wakeup(multi);
    });


    // jthread prepends stop_token meanining it would call networkWorker(stop_token, this) like this
    // lambda to fix argument order so "this" is first
    network_thread = std::jthread([this](std::stop_token stop){ networkWorker(stop); });
    coordinator_thread = std::jthread([this](std::stop_token stop){ coordinatorWorker(stop); });
}

Backend::~Backend(){
    network_thread.request_stop();
    coordinator_thread.request_stop();

    curl_multi_wakeup(multi);
    // jthread would only join in its own destructor, after multi is already cleaned up below
    network_thread.join();
    coordinator_thread.join();

    curl_multi_cleanup(multi);
    curl_global_cleanup();
}


void Backend::networkWorker(std::stop_token stop){
    // TODO: this will keep accummulating connections, manage this once I add persistant session storage
    std::unordered_map<uint64_t, std::unique_ptr<SessionHandle>> se_cache;

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

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
void Backend::executeEffect(Effect&& e){
    std::visit(overloaded{
        [&](SendRequest& r){
            toNetworkQueue.enqueue({
                .session_id = r.sid, 
                .turns = std::move(r.snapshot) 
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
        [&](TurnFinished& t){
            toUIQueue.enqueue({
                .session_id = t.sid,
                .kind = toUIMessage::Kind::TURN_FINISHED,
                .content = ""
            });
        },
        [&](EffectNone& e){},
    }, e);
}

void Backend::coordinatorWorker(std::stop_token stop){
    std::unordered_map<uint64_t, Session> sessions;

    while (!stop.stop_requested()){


        while (auto msg = fromUIQueue.dequeue()){
            switch (msg->kind){
                using enum fromUIMessage::Kind;
                
                case CREATE_SESSION:
                    sessions.try_emplace(msg->session_id, msg->session_id);
                    break;
                case PROMPT_SUBMITED: {
                    Session& s = sessions.at(msg->session_id);
                    Effect effect = s.submitUserTurn(std::move(msg->content));
                    executeEffect(std::move(effect));
                    break;
                }
            }
        }

        while (auto msg = fromNetworkQueue.dequeue()){
            Session& s = sessions.at(msg->session_id);
            Effect effect;

            switch (msg->kind){
    
                case fromNetworkMessage::Kind::OUTPUT_TOKENS: 
                    effect = s.onTextDelta(std::move(msg->content), TokensType::OUTPUT);
                    break;
                
                case fromNetworkMessage::Kind::REASONING_TOKENS: 
                    effect = s.onTextDelta(std::move(msg->content), TokensType::REASONING);
                    break;
                
                case fromNetworkMessage::Kind::TURN_FINISHED: 
                    effect = s.onTurnComplete();
                    break;
                
                case fromNetworkMessage::Kind::ERROR: 
                    effect = s.onRequestFailed();
                    break;
                
            }

            executeEffect(std::move(effect));
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