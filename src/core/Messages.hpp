#pragma once
#include <cstdint>
#include <memory>
#include <variant>
#include <string>

#include "Session.hpp"
#include "Tools.hpp"

struct toUIMessage {
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

struct fromUIMessage {
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
    std::shared_ptr<TurnVec> turns;
    std::vector<SchemaPtr> tools;
};

struct fromNetworkMessage {
    enum class Kind {
        OUTPUT_TOKENS,
        REASONING_TOKENS,
        TURN_FINISHED,
        ERROR
    };

    uint64_t session_id;
    Kind kind;
    std::variant<std::string, ToolCallRequest> content;
};
