#pragma once
#include <compare>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <json.hpp>

struct ToolCallRequest {
    std::string error; // Empty means the call is valid

    std::string id;
    std::string name;
    nlohmann::json args;
};
using ToolCallRequests = std::vector<ToolCallRequest>;


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

