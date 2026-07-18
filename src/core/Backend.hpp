#pragma once
#include <curl/curl.h>

#include <optional>
#include <thread>
#include <stop_token>

#include "ConcurrentQueue.hpp"
#include "SessionHandle.hpp"



class Backend {
    public:
        Backend();
        ~Backend();
    
        // Right now the cleanest way seems to be having the queues as public
        // They need to be accessed from the Bridge and from backend itself
        // TODO: investigate
        ConcurrentQueue<RequestMessage> request_queue;
        ConcurrentQueue<ResponseMessage> response_queue;
    private:
        void networkWorker(std::stop_token stop);
        std::jthread network_thread;

        CURLM *multi;

};