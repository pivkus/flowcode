#pragma once
#include <string>
#include <vector>
#include <memory>

struct Turn {
    enum class Role {USER, ASSISTANT, SYSTEM, TOOL};
    uint64_t turn_id;
    Role role;
    std::string content;
};

class Session{
    
    public:
        using TurnPtr = std::shared_ptr<const Turn>;
        using TurnVec = std::vector<TurnPtr>;
    
        Session(uint64_t id);

        void appendUserTurn(std::string content);

        uint64_t session_id;
        // TODO: I dont like history being public, right now its just simpler
        // investigate moving to private
        TurnVec history;
};