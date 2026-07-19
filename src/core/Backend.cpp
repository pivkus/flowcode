#include "Backend.hpp"

Backend::Backend(){

    curl_global_init(CURL_GLOBAL_ALL);
    multi = curl_multi_init();

    // TODO: Backend and Bridge have a circular reference with these callbacks, make sure they get deleted in a way
    // so this will no be called on a destroyed object
    request_queue.setCallback([this]{
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
    // TODO: this will keep accummulating handles, manage this once I add persistant session storage
    std::unordered_map<uint64_t, std::unique_ptr<SessionHandle>> se_map;

    int still_running = 0;
    while (!stop.stop_requested()){

        while (auto req = request_queue.dequeue()){
            // Will insert a null unique_ptr if id not in map
            std::unique_ptr<SessionHandle>& slot = se_map[req->id];
            if (!slot){
                try {
                    slot = std::make_unique<SessionHandle>(req->id, &response_queue);
                } catch (const std::exception &e){
                    // e.g. missing API key; report to the UI instead of crashing the worker
                    response_queue.enqueue({
                        .id = req->id,
                        .kind = ResponseMessage::Kind::RESPONSE_ERROR,
                        .content = e.what()
                    });
                    se_map.erase(req->id);
                    continue;
                }
            }

            SessionHandle& session = *slot;
            session.prepareMessage(std::move(req->content));
            curl_multi_add_handle(multi, session.raw());
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

void Backend::coordinatorWorker(std::stop_token stop){
    while (!stop.stop_requested()){
        std::println("coordinator");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}