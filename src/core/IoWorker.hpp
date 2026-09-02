#pragma once
#include <filesystem>
#include <stop_token>
#include <string>
#include <thread>

#include "ConcurrentQueue.hpp"
#include "Messages.hpp"
#include "Uuid.hpp"

// Owns the io thread: all blocking filesystem work for session persistence.
class IoWorker {
    public:
        IoWorker(ConcurrentQueue<toIoMessage>& in, ConcurrentQueue<fromIoMessage>& out);
        ~IoWorker();

    private:
        void run(std::stop_token stop);

        void listSessions();
        void persistTurns(Uuid sid, TurnVec turns);
        void reportError(Uuid sid, std::string content);

        ConcurrentQueue<toIoMessage>& in;
        ConcurrentQueue<fromIoMessage>& out;
        std::jthread thread;
};
