#include <ExecutorWorker.hpp>

ExecutorWorker::ExecutorWorker(ConcurrentQueue<ToolExecute>& in,
                   ConcurrentQueue<fromToolMessage>& out, 
                   ToolRegistry& registry, int pool_size)
 : in(in), out(out), registry(registry), pool_size(pool_size)
{
    threads.reserve(pool_size);
    for (int i = 0; i < pool_size; ++i){
        threads.emplace_back([this](std::stop_token stop){ run(stop); });
    }
}

ExecutorWorker::~ExecutorWorker(){
    for (auto& t : threads) t.request_stop();
    for (auto& t : threads) t.join();
}

void ExecutorWorker::run(std::stop_token stop){
    while (!stop.stop_requested()){
        auto task = in.wait_dequeue(stop);
        if (!task) continue; // woken by a stop request with no work queued

        Tool* tool = registry.get_tool(task->call.name);
        ToolResult result = tool->execute(task->call.args, stop);

        out.enqueue(fromToolMessage{
            .session_id = task->sid,
            .turn_id    = task->tid,
            .call_id    = task->call.call_id,
            .result     = std::move(result)
        });
    }
}