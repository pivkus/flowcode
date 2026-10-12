#pragma once
#include <chrono>
#include <string>
#include <variant>
#include <vector>
#include <flowcode/History.hpp>
#include <flowcode/Uuid.hpp>

struct OutputTokensDelta    { Uuid sid; std::string delta; };
struct ReasoningTokensDelta { Uuid sid; std::string delta; };
struct TurnFinished         { Uuid sid; };

struct PromptSubmission     { Uuid sid; std::string prompt; std::string model; };
struct SessionCreation      { Uuid sid; };

struct ListSessionsRes      { std::vector<Uuid> uuids; };
struct ListSessionsReq      {};

struct LoadSessionReq       { Uuid sid; };
struct LoadSessionRes       { Uuid sid; TurnVec history; };

struct EmitToolStarted      { Uuid sid; ToolCallId tcid; std::string name; };
struct EmitToolResult       { Uuid sid; ToolCallId tcid; bool ok; std::string content; };

struct SessionError         { Uuid sid; std::string msg; };
struct SessionRetry         { Uuid sid; std::string msg; std::chrono::steady_clock::time_point deadline; };

using toUIMessage = std::variant<
    OutputTokensDelta, 
    ReasoningTokensDelta, 
    EmitToolStarted,
    EmitToolResult, 
    TurnFinished, 
    SessionError, 
    SessionRetry,
    ListSessionsRes,
    LoadSessionRes
>;

using fromUIMessage = std::variant<
    SessionCreation, 
    PromptSubmission, 
    ListSessionsReq,
    LoadSessionReq
>;

