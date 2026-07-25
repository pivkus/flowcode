#include "Session.hpp"

Session::Session(uint64_t id, std::vector<SchemaPtr> allowed_tools)
: session_id(id), tools(allowed_tools)
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

Effect Session::submitUserTurn(std::string content) {
    if (state != State::IDLE) return EffectNone{};

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

    return SendRequest{ 
        .sid = session_id, 
        .snapshot = snapshotHistory(),
        .tools = tools
    };
}

Effect Session::onTextDelta(std::string tokens, TokensType type){
    if (state != State::AWAITING_MODEL) return EffectNone{};

    auto* aw = std::get_if<AwaitingModelData>(&state_data);
    if (!aw) return EffectNone{};

    if (type == TokensType::OUTPUT){
        aw->incoming.append(tokens);
        return EmitOutput{ .sid = session_id, .content = std::move(tokens) };
    } else {
        // Reasoning tokens are not stored in session history
        return EmitReasoning{ .sid = session_id, .content = std::move(tokens) } ;
    }

}

Effect Session::onTurnComplete(){
    if (state != State::AWAITING_MODEL) return EffectNone{};

    auto* aw = std::get_if<AwaitingModelData>(&state_data);
    if (!aw) return EffectNone{};

    using enum Turn::Role;
    TurnPtr turn = std::make_shared<Turn>(Turn{
        .turn_id = static_cast<uint64_t>(history.size()),
        .role = ASSISTANT,
        .content = std::move(aw->incoming)
    });

    history.push_back(std::move(turn));
    state = State::IDLE;
    state_data = std::monostate{};

    return TurnFinished{ .sid = session_id };
}

// TODO: distinquish different errors and support re-trying
Effect Session::onRequestFailed(){
    if (state != State::AWAITING_MODEL) return EffectNone{};

    auto* aw = std::get_if<AwaitingModelData>(&state_data);
    if (aw) aw->incoming.clear();

    state = State::IDLE;
    state_data = std::monostate{};

    return EffectNone{};
}
