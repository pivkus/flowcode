#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <curl/curl.h>

#include <json.hpp>

#include "ConcurrentQueue.hpp"
#include "Messages.hpp"


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
        using json = nlohmann::json;
        using enum fromNetworkMessage::Kind;

        static size_t writeTrampoline(char* p, size_t sz, size_t n, void* userdata);
        size_t writeback(const char* data, size_t len);
        void handleEvent(std::string_view event);
        void serializeTurnsToJSON(const Session::TurnVec& turns);



        CURL *handle;
        struct curl_slist *headers;

        std::string chunks_buffer;

        json json_payload;
        std::string str_payload;

        uint64_t session_id;
        ConcurrentQueue<fromNetworkMessage> *out_queue;
};
