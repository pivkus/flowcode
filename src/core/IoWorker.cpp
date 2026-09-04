#include <algorithm>
#include <filesystem>
#include <fstream>
#include <print>
#include <ranges>
#include <vector>

#include "IoWorker.hpp"
#include "Log.hpp"
#include "Paths.hpp"

#include <json.hpp>

namespace fs = std::filesystem;
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

IoWorker::IoWorker(ConcurrentQueue<toIoMessage>& in, ConcurrentQueue<fromIoMessage>& out)
    : in(in), out(out) {
    thread = std::jthread([this](std::stop_token stop){ run(stop); });
}

IoWorker::~IoWorker(){
    thread.request_stop();
    thread.join();
}

void IoWorker::run(std::stop_token stop){

    while (!stop.stop_requested()){
        auto msg = in.wait_dequeue(stop);
        if (!msg) continue;

        std::visit(overloaded{
            [&](ListSessionsReq& ls){ listSessions(); },
            [&](PersistTurns& pt){ persistTurns(pt.sid, std::move(pt.turns)); },
            [&](LoadSessionReq& ls){ loadSession(ls.sid); }
        }, *msg);

    }
}

void IoWorker::listSessions(){
    const auto& path = paths::sessionsDir();
    std::vector<Uuid> res;

    // Filesystem operations might throw
    try {
        // Creates the flowcode/sessions directories if they don't exist yet
        fs::create_directories(path);

        auto uuids = fs::directory_iterator(path)
            | std::views::transform([](const auto& d){ return Uuid::from_string(d.path().filename().string()); })
            | std::views::filter([](const auto& u){ return u.has_value(); })
            | std::views::transform([](const auto& u){ return *u; });

        res = uuids | std::ranges::to<std::vector>();

    } catch (const std::exception& e){
        debug_print("Exception in io thread: {}", e.what());
        out.enqueue(GlobalError { .msg = "Filesystem error, fail to load directories" });
        return;
    }

    // directory_iterator doesn't guarantee sorting, do it manually (newest session first)
    // TODO: sort by latest activity not by just creation date
    std::sort(res.begin(), res.end(), std::greater<>());

    out.enqueue(ListSessionsRes { .uuids = std::move(res) });
}

void IoWorker::persistTurns(Uuid sid, TurnVec turns){
    // Get directory of the session
    fs::path dir_path = paths::sessionsDir() / sid.to_string();

    std::ofstream file;
    // Make file operation throw instead of returning errors for easier handleing
    file.exceptions(std::ios::failbit | std::ios::badbit);

    try {
        fs::create_directories(dir_path); // Create the path if it doesn't exist yet
        fs::path history_path = dir_path / "history.jsonl";

        file.open(history_path , std::ios::app); // Open history file for appending

        for (const auto& turn : turns){
            std::string turn_line = getJSONLine(turn);
            std::println(file, "{}", turn_line);
        }

        file.flush();

    } catch (const std::exception& e){
        // TODO: what can be done in case of error?
        debug_print("Exception when persisiting turns: {}", e.what());
        out.enqueue(SessionError { .sid = sid, .msg = "Unable to persist turns" });
        return;
    }
}

std::string IoWorker::getJSONLine(TurnPtr turn){
    json turn_obj;

    turn_obj["turn_id"] = turn->turn_id;
    
    switch (turn->role) {
            using enum Turn::Role;
            case USER:      turn_obj["role"] = "user"; break;
            case ASSISTANT: turn_obj["role"] = "assistant"; break;
            case SYSTEM:    turn_obj["role"] = "system"; break;
            case TOOL:      turn_obj["role"] = "tool"; break;
    }

    std::visit(overloaded{
        [&](const std::string& text){
            turn_obj["content"] = text;
        },
        [&](const AssistantContent& acnt){
            json calls = json::array();
            for (const auto& call : acnt.tool_calls){
                calls.push_back({
                    {"error", call.error},
                    {"id", call.id},
                    {"name", call.name},
                    {"args", call.args}
                });
            }
            turn_obj["content"] = {{"text", acnt.text}, {"tool_calls", std::move(calls)}};
        },
        [&](const ToolResultContent& tcnt){
            turn_obj["content"] = {
                {"tool_call_id", tcnt.tool_call_id},
                {"ok", tcnt.ok},
                {"output", tcnt.content}
            };
        }
    }, turn->content);

    return turn_obj.dump();
}

void IoWorker::loadSession(Uuid sid){
    debug_print("loadSession in ioWorker {}", sid);
}