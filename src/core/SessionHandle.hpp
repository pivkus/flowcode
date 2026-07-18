#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <curl/curl.h>

#include <json.hpp>

#include "ConcurrentQueue.hpp"

struct ResponseMessage {
    enum class Kind {
        OUTPUT_TOKENS,
        REASONING_TOKENS,
        RESPONSE_ERROR,
        RESPONSE_END
    };

    uint64_t id;
    Kind kind;
    std::string content;
};

struct RequestMessage {
    enum class Kind {
        USER_PROMPT
    };

    uint64_t id;
    Kind kind;
    std::string content;
};

class SessionHandle {
    public:
        SessionHandle(uint64_t id, ConcurrentQueue<ResponseMessage> *queue);
        ~SessionHandle();

        // Delete copy/move operations so the object cant accidentaly change address in memory
        // This is needed because we are passing "this" pointer to libcurl
        SessionHandle(const SessionHandle&)            = delete;
        SessionHandle& operator=(const SessionHandle&) = delete;
        SessionHandle(SessionHandle&&)                 = delete;
        SessionHandle& operator=(SessionHandle&&)      = delete;

        void prepareMessage(std::string userprompt);
        void completeMessage();
        void sendError(std::string errmsg);

        CURL *raw();
        uint64_t id();

    private:
        using json = nlohmann::json;

        static size_t writeTrampoline(char* p, size_t sz, size_t n, void* userdata);
        size_t writeback(const char* data, size_t len);
        void handleEvent(std::string_view event);

        CURL *handle;
        std::string chunks_buffer;
        std::string incomming_response;
        std::string incomming_reasoning;
        struct curl_slist *headers;
        json json_payload;
        std::string str_payload;
        uint64_t session_id;
        ConcurrentQueue<ResponseMessage> *out_queue;
};
