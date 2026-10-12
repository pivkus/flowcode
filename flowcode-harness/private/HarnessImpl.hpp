#pragma once

#include <condition_variable>
#include <map>
#include <mutex>
#include <stop_token>
#include <thread>

#include <curl/curl.h>

#include <flowcode/FlowcodeHarness.hpp>
#include <InternalMessages.hpp>
#include <ConcurrentQueue.hpp>
#include <Tools.hpp>
#include <Session.hpp>
#include <NetworkWorker.hpp>
#include <IoWorker.hpp>
#include <ExecutorWorker.hpp>

class Harness::Impl {
public:
    Impl();
    ~Impl();
    void setMessageCallback(MessageCallback cb);
    void pushMessage(fromUIMessage msg);
    std::optional<toUIMessage> popMessage();
private:
    void executeEffects(Effects&& effects);
    void coordinatorWorker(std::stop_token stop);

    class CurlRuntime {
    public:
        CurlRuntime();
        ~CurlRuntime();
        CurlRuntime(const CurlRuntime&) = delete;
        CurlRuntime& operator=(const CurlRuntime&) = delete;
    };

    // Initialized before curl users; cleaned up after all workers have joined.
    CurlRuntime curl_runtime;

    // Global tool registry
    ToolRegistry tool_registry;

    std::map<Uuid, ScheduleRetry> network_retries;

    // Worker output callbacks use this state, so it must outlive every worker.
    std::mutex coord_mutex;
    bool coord_notified = false;
    std::condition_variable_any coord_cv;

    ConcurrentQueue<fromUIMessage> fromUIQueue;
    ConcurrentQueue<toUIMessage> toUIQueue;

    ConcurrentQueue<toNetworkMessage> toNetworkQueue;
    ConcurrentQueue<fromNetworkMessage> fromNetworkQueue;

    static constexpr int executor_pool_size = 4;
    ConcurrentQueue<ToolExecute> toToolQueue;
    ConcurrentQueue<fromToolMessage> fromToolQueue;

    ConcurrentQueue<toIoMessage> toIoQueue;
    ConcurrentQueue<fromIoMessage> fromIoQueue;

    // Workers are destroyed before their queues, registry, and callback state.
    NetworkWorker network_worker{toNetworkQueue, fromNetworkQueue};
    ExecutorWorker executor_worker{toToolQueue, fromToolQueue, tool_registry, executor_pool_size};
    IoWorker io_worker{toIoQueue, fromIoQueue};

    // Stop the producer of worker tasks before destroying the workers.
    std::jthread coordinator_thread;
};
