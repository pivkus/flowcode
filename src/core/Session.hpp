#pragma once
#include <string>
#include <vector>
#include <memory>
#include <print>
#include <variant>

#include "Tools.hpp"



struct AssistantContent { std::string text; ToolCallRequests tool_calls; };
struct ToolResultContent { std::string tool_call_id; bool ok; std::string content; };

using TurnContent = std::variant<std::string, AssistantContent, ToolResultContent>;

struct Turn {
    enum class Role {USER, ASSISTANT, SYSTEM, TOOL};
    uint64_t turn_id;
    Role role;
    TurnContent content;
};

using TurnPtr = std::shared_ptr<const Turn>;
using TurnVec = std::vector<TurnPtr>;



// Effects 

// Effects to the Network
struct SendRequest { uint64_t sid; std::shared_ptr<TurnVec> snapshot;  SchemaMapPtr tools; };
// Effects to the UI
struct TurnFinished { uint64_t sid; };
struct EmitOutput { uint64_t sid; std::string content; };
struct EmitReasoning { uint64_t sid; std::string content; };
struct EmitToolStarted { uint64_t sid; size_t cid; std::string name; };
struct EmitToolResult { uint64_t sid; size_t cid; bool ok; std::string content; };
struct EmitError{ uint64_t sid; std::string content; };
// Effects to the Executor pool
struct ToolExecute { uint64_t sid; uint64_t tid; ResolvedCall call; };

struct EffectNone { };

using Effect = std::variant<EffectNone, SendRequest, EmitOutput, EmitReasoning, TurnFinished,
                            ToolExecute, EmitToolStarted, EmitToolResult, EmitError>;

using Effects = std::vector<Effect>;


enum class TokensType{OUTPUT, REASONING};

class Session {

    public:

        Session(uint64_t id, SchemaMapPtr allowed_tools);

        // Commands from the UI
        Effects submitUserTurn(std::string content);

        // Events from the network
        Effects onTextDelta(std::string tokens, TokensType type);
        Effects onTurnComplete();
        Effects onRequestFailed(std::string errmsg);
        Effects onToolCallsRequest(ToolCallRequests tool_reqs);

        // Events from the tool pool
        Effects onToolCallResult(uint64_t turn_id, size_t call_id, ToolResult result);

        std::shared_ptr<TurnVec> snapshotHistory() const;

        uint64_t session_id;

    private:

        struct AwaitingModelData {
            std::string incoming;
        };
        struct ToolCallExecData {

            struct Slot {
                std::string id; 
                std::string name;
                ToolResult result;
            };

            uint64_t turn_id;
            std::vector<Slot> slots;
            size_t remaining;
        };

        // Appends the tool turns and goes back to AWAITING_MODEL
        void finishToolCalls(Effects& effects);

        enum class State {IDLE, AWAITING_MODEL, TOOL_CALL_EXEC};
        State state = State::IDLE;
        std::variant<std::monostate, AwaitingModelData, ToolCallExecData> state_data;
        TurnVec history;
        SchemaMapPtr tool_schemas;
};
