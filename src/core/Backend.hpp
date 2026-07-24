#pragma once
#include <curl/curl.h>

#include <map>
#include <optional>
#include <condition_variable>
#include <thread>
#include <print>
#include <stop_token>

#include "ConcurrentQueue.hpp"
#include "Messages.hpp"
#include "Session.hpp"

// json only for caching the tool call schemas, not touched after registration 
#include <json.hpp>
using json = nlohmann::json;

enum class ToolParamType { String, Integer, Number, Boolean, StringArray };

struct ToolParam {
    std::string name;
    std::string description;
    ToolParamType type;
    bool required;

    std::vector<std::string> allowed_vals; // String only, empty is unconstrained
};

struct ToolSchema {
    std::string name;
    std::string description;
    std::vector<ToolParam> params;

    json cached;
};

class ToolRegistry {
    public:
        void registerTool(ToolSchema schema);
        SchemaPtr get(std::string_view key);
    private:
        static std::string_view toJSONType(ToolParamType type);
        json buildJSONSchema(const ToolSchema& schema);
        std::map<std::string, SchemaPtr, std::less<>> registry;

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

        // TODO: concider turning coordinator into a separate class
        std::jthread coordinator_thread;
        std::mutex coord_mutex;
        bool coord_notified = false;
        std::condition_variable_any coord_cv;
        void executeEffect(Effect&& e);

        ConcurrentQueue<toNetworkMessage> toNetworkQueue;
        ConcurrentQueue<fromNetworkMessage> fromNetworkQueue;

        CURLM *multi;

};