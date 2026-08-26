#pragma once
#include <cstdint>
#include <memory>
#include <variant>
#include <string>

#include "Session.hpp"
#include "Tools.hpp"
#include "Uuid.hpp"

using toUIContent = std::variant<
    std::string,
    std::vector<Uuid> 
>;
struct toUIMessage {
    enum class Kind {
        OUTPUT_TOKENS,
        REASONING_TOKENS,
        TOOL_CALL_STARTED,
        TOOL_CALL_RESULT,
        TURN_FINISHED,
        ERROR,
        LIST_SESSIONS_RES
    };

    Uuid session_id;
    Kind kind;
    toUIContent content;
};


struct fromUIMessage {
    enum class Kind {
        CREATE_SESSION,
        PROMPT_SUBMITED,
        LIST_SESSIONS
    };

    Uuid session_id;
    Kind kind;
    std::string content;
};

struct toNetworkMessage {
    Uuid session_id;
    // shared_ptr is not really needed yet but will allow multiple consumers of the snapshot in the future
    std::shared_ptr<TurnVec> turns;
    SchemaMapPtr tools;
};

struct fromNetworkMessage {
    enum class Kind {
        OUTPUT_TOKENS,
        REASONING_TOKENS,
        TURN_FINISHED,
        TOOL_CALL,
        ERROR
    };

    Uuid session_id;
    Kind kind;
    std::variant<std::string, ToolCallRequests> content;
};

struct toIoMessage {
    enum class Kind {
        LIST_SESSIONS
    };

    Kind kind;
    Uuid session_id;
};

using fromIoContent = std::variant<
    std::vector<Uuid> 
>;
struct fromIoMessage {
    enum class Kind {
        LIST_SESSIONS_RES
    };

    Kind kind;
    Uuid session_id;
    fromIoContent content;
};

struct fromToolMessage {
    Uuid       session_id;
    uint64_t   turn_id;
    size_t     call_id;
    ToolResult result;
};
