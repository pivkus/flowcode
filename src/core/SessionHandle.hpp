#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <curl/curl.h>
#include <map>

#include <json.hpp>

#include "ConcurrentQueue.hpp"
#include "Messages.hpp"

using json = nlohmann::json;

class SessionHandle {
    public:
        SessionHandle(uint64_t id, ConcurrentQueue<fromNetworkMessage> *queue);
        ~SessionHandle();

        // Delete copy/move operations so the object cant accidentaly change address in memory
        // This is needed because we are passing "this" pointer to libcurl
        SessionHandle(const SessionHandle&)            = delete;
        SessionHandle& operator=(const SessionHandle&) = delete;
        SessionHandle(SessionHandle&&)                 = delete;
        SessionHandle& operator=(SessionHandle&&)      = delete;

        void prepareMessage(toNetworkMessage& request);
        void completeMessage();
        void sendError(std::string errmsg);

        CURL *raw();
        uint64_t id();

    private:
        using enum fromNetworkMessage::Kind;

        static size_t writeTrampoline(char* p, size_t sz, size_t n, void* userdata);
        size_t writeback(const char* data, size_t len);
        void handleEvent(std::string_view event);
    
        void serializeTurnsToJSON(const TurnVec& turns);
        static std::string_view toJSONType(ToolParamType type);
        json buildJSONSchema(const ToolSchema& schema);


        CURL *handle;
        struct curl_slist *headers;

        std::string chunks_buffer;

        json json_payload;
        std::string str_payload;

        uint64_t session_id;
        ConcurrentQueue<fromNetworkMessage> *out_queue;

        // State for collecting incomming tool_call chunks
        struct ToolSlot {
            std::string id;
            std::string name;
            std::string args;
        };
        std::map<int, ToolSlot, std::less<>> tc_incomming;
        SchemaMap tool_schemas;

        // State for response termination
        std::string finish_reason;
        std::string native_finish_reason;
        bool saw_done; // "data: [DONE]"" was emitted

        
};
