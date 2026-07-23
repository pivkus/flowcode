#pragma once
#include <curl/curl.h>

#include <optional>
#include <condition_variable>
#include <thread>
#include <stop_token>

#include "ConcurrentQueue.hpp"
#include "Messages.hpp"

class Backend {
    public:
        Backend();
        ~Backend();
    
        // Right now the cleanest way seems to be having the queues as public
        // They need to be accessed from the Bridge and from backend itself
        // TODO: investigate
        ConcurrentQueue<fromUIMessage> fromUIQueue;
        ConcurrentQueue<toUIMessage> toUIQueue;


    private:
        void networkWorker(std::stop_token stop);
        void coordinatorWorker(std::stop_token stop);

        std::jthread network_thread;

        std::jthread coordinator_thread;
        std::mutex coord_mutex;
        bool coord_notified = false;
        std::condition_variable_any coord_cv;
        void executeEffect(Effect&& e);

        ConcurrentQueue<toNetworkMessage> toNetworkQueue;
        ConcurrentQueue<fromNetworkMessage> fromNetworkQueue;

        CURLM *multi;

};