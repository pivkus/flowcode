#pragma once
#include <string>
#include <vector>
#include <memory>
#include <print>
#include <variant>

struct Turn {
    enum class Role {USER, ASSISTANT, SYSTEM, TOOL};
    uint64_t turn_id;
    Role role;
    std::string content;
};

using TurnPtr = std::shared_ptr<const Turn>;
using TurnVec = std::vector<TurnPtr>;

struct ToolSchema; // defined in Backend.hpp
using SchemaPtr = std::shared_ptr<const ToolSchema>;

enum class TokensType{OUTPUT, REASONING};



// Effects
struct SendRequest { uint64_t sid; std::shared_ptr<TurnVec> snapshot; };
struct EmitOutput { uint64_t sid; std::string content; };
struct EmitReasoning { uint64_t sid; std::string content; };
struct TurnFinished { uint64_t sid; };
struct EffectNone { }; 

using Effect = std::variant<EffectNone, SendRequest, EmitOutput, EmitReasoning, TurnFinished>;

class Session {

    public:

        Session(uint64_t id, std::vector<SchemaPtr> allowed_tools);
        // Session(uint64_t id);

        // Commands from the UI
        Effect submitUserTurn(std::string content);
    
        // Events from the network
        Effect onTextDelta(std::string tokens, TokensType type);
        Effect onTurnComplete();
        Effect onRequestFailed();

        std::shared_ptr<TurnVec> snapshotHistory() const;

        uint64_t session_id;

    private:

        struct AwaitingModelData {
            uint64_t turn_id;
            std::string incoming;
        };

        enum class State {IDLE, AWAITING_MODEL};
        State state = State::IDLE;
        std::variant<std::monostate, AwaitingModelData> state_data;
        TurnVec history;
        std::vector<SchemaPtr> tools;
};
