#pragma once
#include <curl/curl.h>

#include <optional>
#include <condition_variable>
#include <thread>
#include <stop_token>

#include "ConcurrentQueue.hpp"
#include "SessionHandle.hpp"
#include "Session.hpp"

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

struct toNetworkMessage {

    uint64_t session_id;
    // shared_ptr is not really needed yet but will allow multiple consumers of the snapshot in the future
    std::shared_ptr<Session::TurnVec> turns;
};

// TODO: this is simmilar to toUIMessage, maybe consider merging the types
struct fromNetworkMessage {
    enum class Kind {
        OUTPUT_TOKENS,
        REASONING_TOKENS,
        TURN_FINISHED,
        ERROR
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


    private:
        void networkWorker(std::stop_token stop);
        void coordinatorWorker(std::stop_token stop);

        std::jthread network_thread;

        std::jthread coordinator_thread;
        std::mutex coord_mutex;
        bool coord_notified = false;
        std::condition_variable_any coord_cv;

        ConcurrentQueue<toNetworkMessage> toNetworkQueue;
        ConcurrentQueue<fromNetworkMessage> fromNetworkQueue;

        CURLM *multi;

};