#pragma once
#include <chrono>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <flowcode/History.hpp>
#include <flowcode/Uuid.hpp>

struct ModelInfo {
    std::string id; // "provider/name"
    bool supports_reasoning;
    std::vector<std::string> supported_efforts; // "low", "medium", "xhigh" ...
};

struct OutputTokensDelta    { Uuid sid; std::string delta; };
struct ReasoningTokensDelta { Uuid sid; std::string delta; };
struct TurnFinished         { Uuid sid; };
struct SessionNamed         { Uuid sid; std::string name; };

struct PromptSubmission     { Uuid sid; std::string prompt; std::string model; };
struct SessionCreation      { Uuid sid; };

struct ListSessionsRes      { std::vector<Uuid> uuids; std::vector<std::string> names; };
struct ListSessionsReq      {};

struct ListModelsReq        {};
struct ListModelsRes        { std::shared_ptr<std::vector<ModelInfo>> models; };

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
    SessionNamed,
    SessionError, 
    SessionRetry,
    ListSessionsRes,
    ListModelsRes,
    LoadSessionRes
>;

using fromUIMessage = std::variant<
    SessionCreation, 
    PromptSubmission, 
    ListSessionsReq,
    ListModelsReq,
    LoadSessionReq
>;

