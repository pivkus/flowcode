#pragma once
#include <filesystem>
#include <stop_token>
#include <string>
#include <thread>

#include <ConcurrentQueue.hpp>
#include <InternalMessages.hpp>
#include <flowcode/Uuid.hpp>
#include <flowcode/FlowcodeHarness.hpp>

// Owns the io thread: all blocking filesystem work for session persistence.
class IoWorker {
    public:
        IoWorker(
            const HarnessConfig& config,
            ConcurrentQueue<toIoMessage>& in, 
            ConcurrentQueue<fromIoMessage>& out
        );
        ~IoWorker();

    private:
        void run(std::stop_token stop);

        void listSessions();
    
        void loadSession(Uuid sid);
        TurnPtr tryParseLine(std::string& line);


        void persistTurns(Uuid sid, TurnVec turns);
        std::string getJSONLine(TurnPtr turn);

        ConcurrentQueue<toIoMessage>& in;
        ConcurrentQueue<fromIoMessage>& out;
        const HarnessConfig& config;

        std::jthread thread;
};
