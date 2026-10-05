#include "Session.hpp"
#include "Log.hpp"

#include <utility>
#include <algorithm>

Session::Session(Uuid id, SchemaMapPtr allowed_tools)
: session_id(id), tool_schemas(std::move(allowed_tools))
{

    TurnPtr sys = std::make_shared<Turn>(SystemTurn {
        .tid = 0,
        .text = "You are a helpful assistant the users answering questions"
    });

    history.push_back(std::move(sys));

}

Session::Session(Uuid id, SchemaMapPtr allowed_tools, TurnVec history)
: session_id(id), tool_schemas(std::move(allowed_tools)), history(std::move(history))
{
    persisted_upto = this->history.size();
}

void Session::setIdleState(){
    // Also resets the per-request state
    state = Idle{};
    retries_used = 0;
    current_model = "";
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

Effects Session::submitUserTurn(PromptSubmission ps) {
    if (!std::get_if<Idle>(&state)) return {};

    uint64_t turn_id = static_cast<uint64_t>(history.size());

    TurnPtr turn = std::make_shared<Turn>(UserTurn {
        .tid = turn_id,
        .text = std::move(ps.prompt)
    });
    history.push_back(std::move(turn));

    state = AwaitingModel{};
    current_model = std::move(ps.model);

    Effects effects = { SendRequest{
        .sid = session_id,
        .snapshot = snapshotHistory(),
        .tools = tool_schemas,
        .model = current_model
    } };

    persistPending(effects);
    return effects;
}

Effects Session::onTextDelta(std::string tokens, TokensType type){
    auto *aw = std::get_if<AwaitingModel>(&state);
    if (!aw) return {};
    if (tokens.empty()) return {};

    const auto kind = type == TokensType::OUTPUT ? AssistantBlock::Kind::OUTPUT : AssistantBlock::Kind::REASONING;
    if (aw->blocks.empty() || aw->blocks.back().kind != kind){
        aw->blocks.push_back(AssistantBlock{ .kind = kind, .text = tokens });
    } else {
        aw->blocks.back().text.append(tokens);
    }

    if (type == TokensType::OUTPUT){
        return { OutputTokensDelta{ .sid = session_id, .delta = std::move(tokens) } };
    } else {
        return { ReasoningTokensDelta{ .sid = session_id, .delta = std::move(tokens) } };
    }
}

Effects Session::onTurnComplete(){
    auto *aw = std::get_if<AwaitingModel>(&state);
    if (!aw) return {};

    TurnPtr turn = std::make_shared<Turn>(AssistantTurn {
        .tid = static_cast<uint64_t>(history.size()),
        .blocks = std::move(aw->blocks),
        .tool_calls = {}
    });

    history.push_back(std::move(turn));

    setIdleState();

    Effects effects = { TurnFinished{ .sid = session_id } };
    persistPending(effects);

    return effects;
}

Effects Session::onRequestFailed(RequestFailed failure){
    // Failure is only valid in AWAITING_MODEL, RETRY_WAIT means waiting on a retry
    auto *aw = std::get_if<AwaitingModel>(&state);
    if (!aw) return {};

    // Retry only retryable failures with no output ommited in the turn
    // and with available retry budget
    if (failure.retry && aw->blocks.empty() && retries_used < max_retries){
        state = RetryWait{};
        
        size_t delay = init_retry_delay_ms;
        for (size_t r = 0; r < retries_used; r++){
            delay *= 2;
            if (delay > max_retry_delay_ms) { delay = max_retry_delay_ms; break; }
        }
        retries_used += 1;

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay);
        return { 
            SessionRetry{ .sid = session_id, .msg = std::move(failure.msg), .deadline = deadline },
            ScheduleRetry{ 
                .sid = session_id,
                .deadline = deadline
            }
        };
    } else {
        // Terminal failure
        setIdleState();
        return { SessionError{ .sid = session_id, .msg = std::move(failure.msg) } };
    }

}

Effects Session::onRetryDue(){
    if (!std::get_if<RetryWait>(&state)) return {};
    
    state = AwaitingModel{};
    // Does not reset retries_used

    return { SendRequest{ 
        .sid = session_id, 
        .snapshot = snapshotHistory(), 
        .tools = tool_schemas,
        .model = current_model
    } };
}

Effects Session::onToolCallsRequest(ToolCallRequests tool_reqs){
    auto *aw = std::get_if<AwaitingModel>(&state);
    if (!aw) return {};

    uint64_t turn_id = static_cast<uint64_t>(history.size());

    TurnPtr turn = std::make_shared<Turn>(AssistantTurn {
        .tid = turn_id,
        .blocks = std::move(aw->blocks),
        .tool_calls = tool_reqs // Does a copy right now
    });
    history.push_back(std::move(turn));
    // The model finished generating the assistant turn successfully, reset retry counter
    retries_used = 0;

    std::vector<ToolCallExec::Slot> slots_;
    Effects effects;

    // call_id can be used as a unique id of a tool call in one batch - the model can't make another request
    // until all in this batch are evaluated (result or failed)
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
                .tcid = ToolCallId { .tid = turn_id, .cid = call_id },
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

    state = ToolCallExec {
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
    auto *ts = std::get_if<ToolCallExec>(&state);
    if (!ts) return;
    if (ts->remaining > 0) return;

    for (auto& slot : ts->slots_){
        TurnPtr turn = std::make_shared<Turn> (ToolResultTurn {
            .tid = static_cast<uint64_t>(history.size()),
            .tool_call_id = std::move(slot.id),
            .ok = slot.result.ok,
            .content = std::move(slot.result.content)
        });
        history.push_back(std::move(turn));
    }

    effects.push_back(SendRequest{
        .sid = session_id,
        .snapshot = snapshotHistory(),
        .tools = tool_schemas,
        .model = current_model
    });

    state = AwaitingModel{};
    persistPending(effects);
}

Effects Session::onToolCallResult(uint64_t turn_id, size_t call_id, ToolResult result){
    auto *ts = std::get_if<ToolCallExec>(&state);
    if (!ts) return {};
    if (ts->turn_id != turn_id) return {}; // stale turn_id
    if (ts->slots_.size() <= call_id) return {}; // invalid call_id

    Effects effects = {};
    effects.push_back(EmitToolResult {
        .sid = session_id,
        .tcid = ToolCallId { .tid = turn_id, .cid = call_id },
        .ok = result.ok,
        // copy here is expected
        .content = result.content
    });

    ts->slots_[call_id].result = std::move(result);
    ts->remaining--;

    if (ts->remaining == 0){
        finishToolCalls(effects);
    }

    return effects;
}
