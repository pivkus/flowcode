#include "Session.hpp"

Session::Session(uint64_t id, SchemaMap allowed_tools)
: session_id(id), tool_schemas(allowed_tools)
{
    using enum Turn::Role;

    TurnPtr sys = std::make_shared<Turn>(Turn{
        .turn_id = 0,
        .role = SYSTEM,
        .content = "You are a helpful assistant the users answering questions"
    });

    history.push_back(std::move(sys));

}

std::shared_ptr<TurnVec> Session::snapshotHistory() const {
    return std::make_shared<TurnVec>(history);
}

Effects Session::submitUserTurn(std::string content) {
    if (state != State::IDLE) return {};

    uint64_t turn_id = static_cast<uint64_t>(history.size());

    using enum Turn::Role;
    TurnPtr turn = std::make_shared<Turn>(Turn{
        .turn_id = turn_id,
        .role = USER,
        .content = std::move(content)
    });

    history.push_back(std::move(turn));

    state = State::AWAITING_MODEL;
    state_data = AwaitingModelData{
        .turn_id = turn_id,
        .incoming = ""
    };

    return { SendRequest{
        .sid = session_id,
        .snapshot = snapshotHistory(),
        // TODO this is creating a copy of the tool map, turn to shared_ptr i think
        .tools = tool_schemas
    }};
}

Effects Session::onTextDelta(std::string tokens, TokensType type){
    if (state != State::AWAITING_MODEL) return {};

    auto* aw = std::get_if<AwaitingModelData>(&state_data);
    if (!aw) return {};

    if (type == TokensType::OUTPUT){
        aw->incoming.append(tokens);
        return { EmitOutput{ .sid = session_id, .content = std::move(tokens) } };
    } else {
        // Reasoning tokens are not stored in session history
        return { EmitReasoning{ .sid = session_id, .content = std::move(tokens) } };
    }

}

Effects Session::onTurnComplete(){
    if (state != State::AWAITING_MODEL) return {};

    auto aw = std::get<AwaitingModelData>(state_data);

    using enum Turn::Role;
    TurnPtr turn = std::make_shared<Turn>(Turn{
        .turn_id = static_cast<uint64_t>(history.size()),
        .role = ASSISTANT,
        .content = std::move(aw.incoming)
    });

    history.push_back(std::move(turn));
    state = State::IDLE;
    state_data = std::monostate{};

    return { TurnFinished{ .sid = session_id } };
}

// TODO: distinquish different errors and support re-trying
Effects Session::onRequestFailed(){
    if (state != State::AWAITING_MODEL) return {};

    auto aw = std::get<AwaitingModelData>(state_data);
    aw.incoming.clear();

    state = State::IDLE;
    state_data = std::monostate{};

    return {};
}

Effects Session::onToolCallsRequest(ToolCallRequests tool_reqs){
    if (state != State::AWAITING_MODEL) return {};

    auto aw = std::get<AwaitingModelData>(state_data);
    uint64_t turn_id = static_cast<uint64_t>(history.size());

    using enum Turn::Role;
    TurnPtr turn = std::make_shared<Turn>(Turn{
        .turn_id = turn_id,
        .role = ASSISTANT,
        .content = std::move(aw.incoming)
    });

    history.push_back(std::move(turn));

    std::vector<ToolCallExecData::Slot> slots;
    Effects effects;

    uint64_t call_id = 0;
    size_t dispatched = 0;
    for (ToolCallRequest& req : tool_reqs){
        slots.push_back({
            .id = std::move(req.id),
            .name = req.name
        });

        if (req.correct){
            // correct means that the tool is verified to exist (and is allowed)
            ResolvedCall call = {
                .call_id = call_id,
                .fn = tool_schemas[req.name]->execute,
                .args = std::move(req.args)
            };
            effects.push_back(ToolExecute {
                .sid = session_id, 
                .tid = turn_id, 
                .call = std::move(call) 
            });
            effects.push_back(EmitToolStarted {
                .sid = session_id,
                .cid = call_id,
                .name = req.name
            });
            dispatched++;
        } else {
            slots[call_id].result = { 
                .ok = false, 
                .content = "Error: Invalid tool call request"
            };
        }
        call_id++;
    }

    state = State::TOOL_CALL_EXEC;
    state_data = ToolCallExecData {
        .slots = std::move(slots),
        .turn_id = turn_id,
        .remaining = tool_reqs.size()
    };

    if (dispatched > 0){
        // Case: All emmited tool call requests were invalid, nothing to dispatch
        finishToolCalls(effects);
    } 

    return effects;
}

void Session::finishToolCalls(Effects& effects){
    if (state != State::TOOL_CALL_EXEC) return;

    auto ts = std::get<ToolCallExecData>(state_data);
    if (ts.remaining > 0) return;

    using enum Turn::Role;
    for (auto& slot : ts.slots){
        TurnPtr turn = std::make_shared<Turn> (Turn {
            .turn_id = static_cast<uint64_t>(history.size()),
            .role = TOOL,
            .content = ToolResultContent {
                .tool_call_id = std::move(slot.id),
                .ok = slot.result.ok,
                .content = std::move(slot.result.content)
            }
        });
        history.push_back(std::move(turn));
    }

    effects.push_back(SendRequest{
        .sid = session_id,
        .snapshot = snapshotHistory(),
        // TODO this is creating a copy of the tool map, turn to shared_ptr i think
        .tools = tool_schemas
    });

    state = State::AWAITING_MODEL;
    state_data = AwaitingModelData {
        // TODO: fix this -1 hack
        .turn_id = static_cast<uint64_t>(history.size()) - 1,
        .incoming = ""
    };
}

Effects Session::onToolCallResult(uint64_t turn_id, size_t call_id, ToolResult result){
    if (state != State::TOOL_CALL_EXEC) return {};

    auto ts = std::get<ToolCallExecData>(state_data);
    if (ts.turn_id != turn_id) return {}; // stale turn_id

    if (ts.slots.size() <= call_id) return {}; // invalid call_id

    Effects effects = {};
    effects.push_back(EmitToolResult {
        .sid = session_id,
        .cid = call_id,
        .ok = result.ok,
        // copy here is expected
        .content = result.content
    });

    ts.slots[call_id].result = std::move(result);
    ts.remaining--;

    if (ts.remaining == 0){
        finishToolCalls(effects);
    }

    return effects;
}