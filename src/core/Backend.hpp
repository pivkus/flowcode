#pragma once
#include <curl/curl.h>

#include <map>
#include <optional>
#include <condition_variable>
#include <thread>
#include <print>
#include <stop_token>

#include "ConcurrentQueue.hpp"
#include "IoWorker.hpp"
#include "Messages.hpp"
#include "Session.hpp"
#include "Tools.hpp"

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
        void executorWorker(std::stop_token stop);

        std::jthread network_thread;

        // TODO: concider turning coordinator into a separate class
        std::jthread coordinator_thread;
        std::mutex coord_mutex;
        bool coord_notified = false;
        std::condition_variable_any coord_cv;
        void executeEffects(Effects&& effects);

        ConcurrentQueue<toNetworkMessage> toNetworkQueue;
        ConcurrentQueue<fromNetworkMessage> fromNetworkQueue; 

        static constexpr int executor_pool_size = 4;
        ConcurrentQueue<ToolExecute> toToolQueue;
        ConcurrentQueue<fromToolMessage> fromToolQueue;
        std::vector<std::jthread> executor_threads;

        ConcurrentQueue<toIoMessage> toIoQueue;
        ConcurrentQueue<fromIoMessage> fromIoQueue;
        // declared after its queues so it is destroyed (and joined) before them
        IoWorker io_worker{toIoQueue, fromIoQueue};

        // Global tool registry
        ToolRegistry tool_registry;

        CURLM *multi;

};