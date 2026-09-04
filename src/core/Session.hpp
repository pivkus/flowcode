#pragma once
#include <string>
#include <vector>
#include <memory>
#include <print>
#include <variant>

#include "Tools.hpp"
#include "Uuid.hpp"
#include "Messages.hpp"

enum class TokensType{OUTPUT, REASONING};

class Session {

    public:

        Session(Uuid id, SchemaMapPtr allowed_tools);

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

        Uuid session_id;

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
            std::vector<Slot> slots_;
            size_t remaining;
        };

        // Appends the tool turns and goes back to AWAITING_MODEL
        void finishToolCalls(Effects& effects);

        // Tracks how many turns have been persisted (saved) to disc
        size_t persisted_upto = 0;
        void persistPending(Effects& effects);


        enum class State {IDLE, AWAITING_MODEL, TOOL_CALL_EXEC};
        State state = State::IDLE;
        std::variant<std::monostate, AwaitingModelData, ToolCallExecData> state_data;
        TurnVec history;
        SchemaMapPtr tool_schemas;
};
