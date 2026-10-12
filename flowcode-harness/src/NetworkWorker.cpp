#include <NetworkWorker.hpp>
#include <SessionHandle.hpp>

NetworkWorker::NetworkWorker(ConcurrentQueue<toNetworkMessage>& in, ConcurrentQueue<fromNetworkMessage>& out)
: in(in), out(out)
{
    multi = curl_multi_init();

    in.setCallback([this]{
        // This wakes up the network thread, will be called from main thread
        curl_multi_wakeup(multi);
    });

    thread = std::jthread([this](std::stop_token stop){ run(stop); });
}

NetworkWorker::~NetworkWorker(){
    thread.request_stop();
    thread.join();
    curl_multi_cleanup(multi);
}

void NetworkWorker::run(std::stop_token stop){
    std::unordered_map<Uuid, std::unique_ptr<SessionHandle>> se_cache;

    int still_running = 0;
    while (!stop.stop_requested()){

        while (auto req = in.dequeue()){
            // Will insert a null unique_ptr if id not in map
            std::unique_ptr<SessionHandle>& slot = se_cache[req->sid];
            if (!slot){
                try {
                    slot = std::make_unique<SessionHandle>(req->sid, &out);
                } catch (const std::exception &e){
                    // e.g. missing API key; report to the UI instead of crashing the worker

                    out.enqueue(RequestFailed {
                        .sid = req->sid,
                        .msg = e.what(),
                        .retry = false
                    });
                    se_cache.erase(req->sid);
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
                const CURLcode result = m->data.result;
                curl_multi_remove_handle(multi, session.raw());

                if (!session.pending_failure){
                    if (result != CURLE_OK){
                        // Curl errors not triggered by us
                        out.enqueue(RequestFailed {
                            .sid = session.id(),
                            .msg = curl_easy_strerror(result),
                            .retry = true
                        });
                    } else {
                        session.completeMessage();
                    }
                }

                // completeMessage can set pending_failure
                if (session.pending_failure){
                    // Termination caused by sendError, send the pending error
                    out.enqueue(std::move(*session.pending_failure));
                }
                session.resetRequestState();
            }
        }

        // will wait if there are no active handles or until wakeup
        int numfds = 0;
        curl_multi_poll(multi, nullptr, 0, 1000, &numfds);
    }
}