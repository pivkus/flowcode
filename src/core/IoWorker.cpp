#include <algorithm>
#include <filesystem>
#include <fstream>
#include <print>
#include <ranges>
#include <vector>

#include "IoWorker.hpp"
#include "Log.hpp"
#include "Paths.hpp"

namespace fs = std::filesystem;

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

        switch (msg->kind){
            using enum toIoMessage::Kind;
            case LIST_SESSIONS: listSessions(); break;
            case PERSIST_TURNS: persistTurns(msg->session_id, std::move(msg->content)); break;
        }

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
        out.enqueue(fromIoMessage{
            .kind = fromIoMessage::Kind::ERROR,
            .content = "Filesystem error, fail to load directories"
        });
        return;
    }

    // directory_iterator doesn't guarantee sorting, do it manually (newest session first)
    // TODO: sort by latest activity not by just creation date
    std::sort(res.begin(), res.end(), std::greater<>());

    out.enqueue(fromIoMessage {
        .kind = fromIoMessage::Kind::LIST_SESSIONS_RES,
        .content = std::move(res)
    });
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
        std::println(file, "persisted: {}", sid);

        file.flush();

    } catch (const std::exception& e){
        // TODO: what can be done in case of error?
        debug_print("Issue persisting turns: {}", e.what());
        return;
    }
}

void IoWorker::reportError(Uuid sid, std::string content){
}
