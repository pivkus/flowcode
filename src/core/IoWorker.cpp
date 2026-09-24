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

    std::visit(overloaded{
        [&](const UserTurn& t){
            turn_obj["turn_id"] = t.tid;
            turn_obj["role"] = "user";
            turn_obj["content"] = t.text;
        },
        [&](const AssistantTurn& t){
            turn_obj["turn_id"] = t.tid;
            turn_obj["role"] = "assistant";

            json calls = json::array();
            for (const auto& call : t.tool_calls){
                calls.push_back({
                    {"error", call.error},
                    {"id", call.id},
                    {"name", call.name},
                    {"args", call.args}
                });
            }
            turn_obj["content"] = {{"text", t.text}, {"reasoning", t.reasoning }, {"tool_calls", std::move(calls)}};
        },
        [&](const ToolResultTurn& t){
            turn_obj["turn_id"] = t.tid;
            turn_obj["role"] = "tool";
            turn_obj["content"] = {
                {"tool_call_id", t.tool_call_id},
                {"ok", t.ok},
                {"output", t.content}
            };
        },
        [&](const SystemTurn& t){
            turn_obj["turn_id"] = t.tid;
            turn_obj["role"] = "system";
            turn_obj["content"] = t.text;
        }
    }, *turn);

    return turn_obj.dump();
}

TurnPtr IoWorker::tryParseLine(std::string& line){

    Turn turn;

    json parsed = json::parse(line, nullptr, false);
    if (parsed.is_discarded()) return nullptr; // Parse error

    auto t_it = parsed.find("turn_id");
    auto r_it = parsed.find("role");
    auto c_it = parsed.find("content");

    if (t_it == parsed.end() || !t_it->is_number_unsigned()) return nullptr;
    if (r_it == parsed.end() || !r_it->is_string()) return nullptr;
    if (c_it == parsed.end()) return nullptr;

    // turn.turn_id = t_it->get<uint64_t>();
    
    auto role_str = r_it->get<std::string>();
    if (role_str == "user") {
        if (!c_it->is_string()) return nullptr;
        turn = UserTurn {
            .tid = t_it->get<uint64_t>(),
            .text = c_it->get<std::string>()
        };
    }
    else if (role_str == "assistant") {
        if (!c_it->is_object()) return nullptr;

        auto tx_it = c_it->find("text");
        auto calls_it = c_it->find("tool_calls");
        auto reason_it = c_it->find("reasoning");
        if (tx_it == c_it->end() || !tx_it->is_string()) return nullptr;
        if (calls_it == c_it->end() || !calls_it->is_array()) return nullptr;
        if (reason_it == c_it->end() || !reason_it->is_string()) return nullptr;

        ToolCallRequests calls;
        calls.reserve(calls_it->size());
        for (const auto& jcall : *calls_it) {
            if (!jcall.is_object()) return nullptr;
            auto er_it = jcall.find("error");
            auto id_it = jcall.find("id");
            auto nm_it = jcall.find("name");
            auto ag_it = jcall.find("args");
            if (er_it == jcall.end() || !er_it->is_string()) return nullptr;
            if (id_it == jcall.end() || !id_it->is_string()) return nullptr;
            if (nm_it == jcall.end() || !nm_it->is_string()) return nullptr;
            if (ag_it == jcall.end()) return nullptr;

            calls.push_back(ToolCallRequest {
                .error = er_it->get<std::string>(),
                .id = id_it->get<std::string>(),
                .name = nm_it->get<std::string>(),
                .args = *ag_it
            });
        }

        turn = AssistantTurn {
            .tid = t_it->get<uint64_t>(),
            .text = tx_it->get<std::string>(),
            .reasoning= reason_it->get<std::string>(),
            .tool_calls = std::move(calls)
        };
    }
    else if (role_str == "system") {
        if (!c_it->is_string()) return nullptr;
        turn = SystemTurn {
            .tid = t_it->get<uint64_t>(),
            .text = c_it->get<std::string>()
        };
    }
    else if (role_str == "tool") {
        if (!c_it->is_object()) return nullptr;

        auto tc_it = c_it->find("tool_call_id");
        auto ok_it = c_it->find("ok");
        auto ou_it = c_it->find("output");
        if (tc_it == c_it->end() || !tc_it->is_string()) return nullptr;
        if (ok_it == c_it->end() || !ok_it->is_boolean()) return nullptr;
        if (ou_it == c_it->end() || !ou_it->is_string()) return nullptr;

        turn = ToolResultTurn {
            .tid = t_it->get<uint64_t>(),
            .content = ou_it->get<std::string>(),
            .tool_call_id = tc_it->get<std::string>(),
            .ok = ok_it->get<bool>()
        };

    }
    else {
        // Role string is not valid
        return nullptr;
    }

    return std::make_shared<Turn>(turn);
}

void IoWorker::loadSession(Uuid sid){
    
    fs::path path = paths::sessionsDir() / sid.to_string() / "history.jsonl";
    std::ifstream history_file(path);

    if (!history_file.is_open() || history_file.fail()){
        debug_print("Failed to open the history file {}", path.string());
        out.enqueue(SessionError { .sid = sid, .msg = "Failed to open Session" });
        return;
    }

    std::string line;
    TurnVec turns;
    while (std::getline(history_file, line)){

        TurnPtr turn = tryParseLine(line);
        if (!turn){
            debug_print("Parsing Error in loadSession {}", sid);
            out.enqueue(SessionError { .sid = sid, .msg = "Corrupted Session"});
            return;
        }
        turns.push_back(turn);

    }

    if (history_file.bad()){
        // Covers disk IO errors that might happen in getline loop
        debug_print("I/O error when parsing file {}", path.string());
        out.enqueue(SessionError { .sid = sid, .msg = "Error while opening Session" });
        return;
    }

    out.enqueue(LoadSessionRes { .sid = sid, .history = std::move(turns) });

}