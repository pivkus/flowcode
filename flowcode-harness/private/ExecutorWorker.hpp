#pragma once

#include <thread>
#include <stop_token>
#include <vector>

#include <ConcurrentQueue.hpp>
#include <InternalMessages.hpp>
#include <Tools.hpp>

class ExecutorWorker {
public:
    ExecutorWorker(ConcurrentQueue<ToolExecute>& in,
                   ConcurrentQueue<fromToolMessage>& out, 
                   ToolRegistry& registry, int pool_size);
    ~ExecutorWorker();
private:
    void run(std::stop_token stop);

    int pool_size;
    ToolRegistry& registry;

    ConcurrentQueue<ToolExecute>& in;
    ConcurrentQueue<fromToolMessage>& out;
    std::vector<std::jthread> threads;
};