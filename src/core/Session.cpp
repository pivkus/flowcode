#include "Session.hpp"

Session::Session(Uuid id, SchemaMapPtr allowed_tools)
: session_id(id), tool_schemas(std::move(allowed_tools))
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

void Session::persistPending(Effects& effects){
    if (persisted_upto >= history.size()) return;

    effects.push_back(PersistTurns{
        .sid = session_id,
        .turns = TurnVec(history.begin() + persisted_upto, history.end())
    });
    persisted_upto = history.size();
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
        .incoming = ""
    };

    Effects effects = { SendRequest{
        .sid = session_id,
        .snapshot = snapshotHistory(),
        .tools = tool_schemas
    } };

    persistPending(effects);
    return effects;
}

Effects Session::onTextDelta(std::string tokens, TokensType type){
    if (state != State::AWAITING_MODEL) return {};

    auto* aw = std::get_if<AwaitingModelData>(&state_data);
    if (!aw) return {};

    if (type == TokensType::OUTPUT){
        aw->incoming.append(tokens);
        return { OutputTokensDelta{ .sid = session_id, .delta = std::move(tokens) } };
    } else {
        // Reasoning tokens are not stored in session history
        return { ReasoningTokensDelta{ .sid = session_id, .delta = std::move(tokens) } };
    }

}

Effects Session::onTurnComplete(){
    if (state != State::AWAITING_MODEL) return {};

    auto aw = std::get<AwaitingModelData>(state_data);

    using enum Turn::Role;
    TurnPtr turn = std::make_shared<Turn>(Turn{
        .turn_id = static_cast<uint64_t>(history.size()),
        .role = ASSISTANT,
        .content = AssistantContent{ 
            .text = std::move(aw.incoming), 
            .tool_calls = {}
        }
    });

    history.push_back(std::move(turn));
    state = State::IDLE;
    state_data = std::monostate{};

    Effects effects = { TurnFinished{ .sid = session_id } };
    persistPending(effects);

    return effects;
}

// TODO: distinquish different errors and support re-trying
Effects Session::onRequestFailed(std::string errmsg){
    if (state != State::AWAITING_MODEL) return {};

    auto aw = std::get<AwaitingModelData>(state_data);
    aw.incoming.clear();

    state = State::IDLE;
    state_data = std::monostate{};

    return { SessionError{ .sid = session_id, .msg = std::move(errmsg) } };
}

Effects Session::onToolCallsRequest(ToolCallRequests tool_reqs){
    if (state != State::AWAITING_MODEL) return {};

    auto aw = std::get<AwaitingModelData>(state_data);
    uint64_t turn_id = static_cast<uint64_t>(history.size());

    using enum Turn::Role;
    TurnPtr turn = std::make_shared<Turn>(Turn{
        .turn_id = turn_id,
        .role = ASSISTANT,
        .content = AssistantContent {
            .text = std::move(aw.incoming),
            .tool_calls = tool_reqs // Does a copy right now
        }
    });
    history.push_back(std::move(turn));

    std::vector<ToolCallExecData::Slot> slots_;
    Effects effects;

    uint64_t call_id = 0;
    size_t dispatched = 0;
    for (ToolCallRequest& req : tool_reqs){
        slots_.push_back({
            .id = std::move(req.id),
            .name = req.name
        });

        if (req.error.empty()){
            // no error means the tool is verified to exist (and is allowed)
            ResolvedCall call = {
                .call_id = call_id,
                .name = req.name,
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
            slots_[call_id].result = {
                .ok = false,
                .content = "Error: " + req.error
            };
        }
        call_id++;
    }

    state = State::TOOL_CALL_EXEC;
    state_data = ToolCallExecData {
        .turn_id = turn_id,
        .slots_ = std::move(slots_),
        .remaining = dispatched
    };


    if (dispatched == 0){
        // Case: All emmited tool call requests were invalid, nothing to dispatch
        finishToolCalls(effects);
    } 

    // finishToolCalls might call persistPending but it is idempotent, so its fine to call twice
    persistPending(effects);
    return effects;
}

void Session::finishToolCalls(Effects& effects){
    if (state != State::TOOL_CALL_EXEC) return;

    auto& ts = std::get<ToolCallExecData>(state_data);
    if (ts.remaining > 0) return;

    using enum Turn::Role;
    for (auto& slot : ts.slots_){
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
        .tools = tool_schemas
    });

    state = State::AWAITING_MODEL;
    state_data = AwaitingModelData {
        .incoming = ""
    };

    persistPending(effects);
}

Effects Session::onToolCallResult(uint64_t turn_id, size_t call_id, ToolResult result){
    if (state != State::TOOL_CALL_EXEC) return {};

    auto& ts = std::get<ToolCallExecData>(state_data);
    if (ts.turn_id != turn_id) return {}; // stale turn_id

    if (ts.slots_.size() <= call_id) return {}; // invalid call_id

    Effects effects = {};
    effects.push_back(EmitToolResult {
        .sid = session_id,
        .cid = call_id,
        .ok = result.ok,
        // copy here is expected
        .content = result.content
    });

    ts.slots_[call_id].result = std::move(result);
    ts.remaining--;

    if (ts.remaining == 0){
        finishToolCalls(effects);
    }

    return effects;
}