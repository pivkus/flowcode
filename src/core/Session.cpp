#include "Session.hpp"

Session::Session(uint64_t id)
: session_id(id)
{
    using enum Turn::Role;

    TurnPtr sys = std::make_shared<Turn>(Turn{
        .turn_id = 0, 
        .role = SYSTEM,
        .content = "You are a helpful assistant the users answering questions"
    });

    history.push_back(std::move(sys));
}

void Session::appendUserTurn(std::string content) {
    using enum Turn::Role;

    TurnPtr turn = std::make_shared<Turn>(Turn{
        .turn_id = static_cast<uint64_t>(history.size()),
        .role = USER,
        .content = std::move(content)
    });
    history.push_back(std::move(turn));
}