#pragma once
#include <string>
#include <vector>
#include <memory>
#include <variant>

#include "Tools.hpp"
#include "Uuid.hpp"
#include "Messages.hpp"

enum class TokensType{OUTPUT, REASONING};

class Session {

    public:
        Session(Uuid id, SchemaMapPtr allowed_tools);
        Session(Uuid id, SchemaMapPtr allowed_tools, TurnVec history);
        Uuid session_id;

        // Commands from the UI
        Effects submitUserTurn(PromptSubmission ps);

        // Events from the network
        Effects onTextDelta(std::string tokens, TokensType type);
        Effects onTurnComplete();
        Effects onRequestFailed(RequestFailed failure);
        Effects onToolCallsRequest(ToolCallRequests tool_reqs);

        // Events from the tool pool
        Effects onToolCallResult(uint64_t turn_id, size_t call_id, ToolResult result);

        Effects onRetryDue();

        std::shared_ptr<TurnVec> snapshotHistory() const;
    private:

        struct AwaitingModelData {
            std::vector<AssistantBlock> blocks;
        };
        struct ToolCallExecData {

            struct Slot {
                std::string id; 
                std::string name;
                ToolResult result;
            };

            uint64_t turn_id;
            std::vector<Slot> slots_;
            size_t remaining;
        };


        // Appends the tool turns and goes back to AWAITING_MODEL
        void finishToolCalls(Effects& effects);

        void setIdleState();

        // Tracks how many turns have been persisted (saved) to disc
        size_t persisted_upto = 0;
        void persistPending(Effects& effects);

        size_t retries_used = 0;
        static constexpr size_t init_retry_delay_ms = 500;
        static constexpr size_t max_retry_delay_ms  = 30000;
        static constexpr size_t max_retries = 8;

        enum class State {IDLE, AWAITING_MODEL, TOOL_CALL_EXEC, RETRY_WAIT};
        State state = State::IDLE;
        std::variant<std::monostate, AwaitingModelData, ToolCallExecData> state_data;
        TurnVec history;
        SchemaMapPtr tool_schemas;

        std::string current_model;
};
