#pragma once
#include <curl/curl.h>

#include <optional>
#include <condition_variable>
#include <thread>
#include <stop_token>

#include "ConcurrentQueue.hpp"
#include "SessionHandle.hpp"

struct toUIMessage{
    enum class Kind {
        OUTPUT_TOKENS,
        REASONING_TOKENS,
        TOOL_CALL_STARTED,
        TOOL_CALL_RESULT,
        TURN_FINISHED,
        ERROR
    };

    uint64_t session_id;
    Kind kind;
    std::string content;
};

struct fromUIMessage{
    enum class Kind {
        CREATE_SESSION,
        PROMPT_SUBMITED
    };

    uint64_t session_id;
    Kind kind;
    std::string content;
};

class Backend {
    public:
        Backend();
        ~Backend();
    
        // Right now the cleanest way seems to be having the queues as public
        // They need to be accessed from the Bridge and from backend itself
        // TODO: investigate
        ConcurrentQueue<fromUIMessage> fromUIQueue;
        ConcurrentQueue<toUIMessage> toUIQueue;

        ConcurrentQueue<RequestMessage> request_queue;
        ConcurrentQueue<ResponseMessage> response_queue;

    private:
        void networkWorker(std::stop_token stop);
        void coordinatorWorker(std::stop_token stop);

        std::jthread network_thread;

        std::jthread coordinator_thread;
        std::mutex coord_mutex;
        bool coord_notified = false;
        std::condition_variable_any coord_cv;


        CURLM *multi;

};