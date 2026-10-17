#pragma once

#include <thread>
#include <stop_token>

#include <curl/curl.h>

#include <flowcode/FlowcodeHarness.hpp>
#include <ConcurrentQueue.hpp>
#include <InternalMessages.hpp>


class NetworkWorker {
public:
    NetworkWorker(
        const HarnessConfig& config, 
        ConcurrentQueue<toNetworkMessage>& in, 
        ConcurrentQueue<fromNetworkMessage>& out
    );
    ~NetworkWorker();
private:
    void run(std::stop_token stop);

    ConcurrentQueue<toNetworkMessage>& in;
    ConcurrentQueue<fromNetworkMessage>& out;
    const HarnessConfig& config;

    std::jthread thread;

    CURLM *multi;
};