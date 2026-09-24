#pragma once
#include <compare>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <variant>
#include <string>
#include <compare>

#include "Tools.hpp"
#include "Uuid.hpp"

struct ToolCallId {
    uint64_t tid;
    uint64_t cid;

    auto operator<=>(const ToolCallId&) const = default;
};

struct AssistantBlock {
    enum class Kind { OUTPUT, REASONING };
    Kind kind;
    std::string text;
};

struct UserTurn         { uint64_t tid; std::string text; };
struct AssistantTurn    { uint64_t tid; std::vector<AssistantBlock> blocks; ToolCallRequests tool_calls; };
struct ToolResultTurn   { uint64_t tid; std::string tool_call_id; bool ok; std::string content; };
struct SystemTurn       { uint64_t tid; std::string text; };

using Turn = std::variant<UserTurn, AssistantTurn, ToolResultTurn, SystemTurn>;
using TurnPtr = std::shared_ptr<const Turn>;
using TurnVec = std::vector<TurnPtr>;

struct PersistTurns         { Uuid sid; TurnVec turns; };

struct SendRequest          { Uuid sid; std::shared_ptr<TurnVec> snapshot;  SchemaMapPtr tools; };
struct OutputTokensDelta    { Uuid sid; std::string delta; };
struct ReasoningTokensDelta { Uuid sid; std::string delta; };
struct TurnFinished         { Uuid sid; };

struct PromptSubmission     { Uuid sid; std::string prompt; };
struct SessionCreation      { Uuid sid; };

struct ListSessionsRes      { std::vector<Uuid> uuids; };
struct ListSessionsReq      {};

struct LoadSessionReq       { Uuid sid; };
struct LoadSessionRes       { Uuid sid; TurnVec history; };

// Tools
struct ToolCallsMade        { Uuid sid; ToolCallRequests calls; };
struct ToolExecute          { Uuid sid; uint64_t tid; ResolvedCall call; };
struct EmitToolStarted      { Uuid sid; ToolCallId tcid; std::string name; };
struct EmitToolResult       { Uuid sid; ToolCallId tcid; bool ok; std::string content; };

// Errors
struct GlobalError          { std::string msg; };
struct SessionError         { Uuid sid; std::string msg; };

struct EffectNone { };

using Effect = std::variant<
    EffectNone,
    SendRequest, 
    OutputTokensDelta, 
    ReasoningTokensDelta, 
    TurnFinished,
    ToolExecute, 
    EmitToolStarted, 
    EmitToolResult, 
    SessionError, 
    PersistTurns
>;
using Effects = std::vector<Effect>;


using toUIMessage = std::variant<
    OutputTokensDelta, 
    ReasoningTokensDelta, 
    EmitToolStarted,
    EmitToolResult, 
    TurnFinished, 
    SessionError, 
    ListSessionsRes,
    LoadSessionRes
>;

using fromUIMessage = std::variant<
    SessionCreation, 
    PromptSubmission, 
    ListSessionsReq,
    LoadSessionReq
>;

using fromNetworkMessage = std::variant<
    OutputTokensDelta, 
    ReasoningTokensDelta, 
    TurnFinished, 
    ToolCallsMade, 
    SessionError
>;

using toIoMessage = std::variant<
    ListSessionsReq,
    PersistTurns,
    LoadSessionReq
>;

using fromIoMessage = std::variant<
    ListSessionsRes,
    LoadSessionRes,
    GlobalError,
    SessionError
>;

using toNetworkMessage = SendRequest;

struct fromToolMessage {
    Uuid       session_id;
    uint64_t   turn_id;
    size_t     call_id;
    ToolResult result;
};
