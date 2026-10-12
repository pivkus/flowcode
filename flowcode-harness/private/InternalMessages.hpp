#pragma once
#include <cstddef>

#include <flowcode/Messages.hpp>
#include <Tools.hpp>

struct PersistTurns         { Uuid sid; TurnVec turns; };
struct ScheduleRetry        { Uuid sid; std::chrono::steady_clock::time_point deadline; };

struct SendRequest          { Uuid sid; std::shared_ptr<TurnVec> snapshot;
                              SchemaMapPtr tools; std::string model; };
// Tools
struct ToolCallsMade        { Uuid sid; ToolCallRequests calls; };
struct ToolExecute          { Uuid sid; uint64_t tid; ResolvedCall call; };
// Errors
struct GlobalError          { std::string msg; };
struct RequestFailed        { Uuid sid; std::string msg; bool retry; };

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
    SessionRetry,
    PersistTurns,
    ScheduleRetry
>;
using Effects = std::vector<Effect>;


using fromNetworkMessage = std::variant<
    OutputTokensDelta, 
    ReasoningTokensDelta, 
    TurnFinished, 
    ToolCallsMade, 
    RequestFailed
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
