#pragma once

#include <thread>
#include <stop_token>

#include <curl/curl.h>

#include <ConcurrentQueue.hpp>
#include <InternalMessages.hpp>


class NetworkWorker {
public:
    NetworkWorker(ConcurrentQueue<toNetworkMessage>& in, ConcurrentQueue<fromNetworkMessage>& out);
    ~NetworkWorker();
private:
    void run(std::stop_token stop);

    ConcurrentQueue<toNetworkMessage>& in;
    ConcurrentQueue<fromNetworkMessage>& out;
    std::jthread thread;

    CURLM *multi;
};