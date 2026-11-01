#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <curl/curl.h>
#include <flowcode/FlowcodeHarness.hpp>
#include <ConcurrentQueue.hpp>
#include <InternalMessages.hpp>

struct HttpFailure {
    CURLcode code = CURLE_OK;
    long status = 0; // HTTP status, or zero if no response was received.
    std::string message;
};

class ChatHandler {
public:
    ChatHandler(Uuid sid, SchemaMapPtr tools,
                ConcurrentQueue<fromNetworkMessage>& out);
    bool onBytes(std::string_view bytes);
    void onComplete();
    void onFailure(const HttpFailure& failure);
private:
    Uuid sid;
    SchemaMapPtr tools;
    ConcurrentQueue<fromNetworkMessage>& out;

    void handleEvent(std::string_view event);
    std::string chunks_buffer;
    struct ToolSlot {
        std::string id;
        std::string name;
        std::string args;
    };
    std::map<int, ToolSlot> tc_incomming;
    std::string finish_reason;
    std::string native_finish_reason;
    bool saw_done = false;
    std::optional<RequestFailed> pending_failure;
    void sendError(std::string message, bool retry);
    std::string validateToolCall(std::string& name, json& args);
};

class NamingHandler {
public:
    NamingHandler(Uuid sid, ConcurrentQueue<fromNetworkMessage>& out);
    bool onBytes(std::string_view bytes);
    void onComplete();
    void onFailure(const HttpFailure& failure);
private:
    Uuid sid;
    std::string response_body;
    ConcurrentQueue<fromNetworkMessage>& out;
};

using ResponseHandler = std::variant<ChatHandler, NamingHandler>;

std::string buildChatPayload(const SendRequest& request);
std::string buildSessionNamingPayload(const NameSessionReq& request,
                                      const HarnessConfig& config);
