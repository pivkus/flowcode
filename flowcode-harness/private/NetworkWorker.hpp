#pragma once

#include "flowcode/Messages.hpp"
#include <exception>
#include <map>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include <ResponseHandlers.hpp>

class HttpRequest {
    public:
        // GET request; owns the URL and headers for the entire transfer.
        HttpRequest(std::string url, std::vector<std::string> headers,
                    ResponseHandler handler);

        // POST request; also owns the payload for the entire transfer.
        HttpRequest(std::string url, std::vector<std::string> headers,
                    std::string payload, ResponseHandler handler);
        ~HttpRequest();

        // CURL callbacks retain this object's address.
        HttpRequest(const HttpRequest&) = delete;
        HttpRequest& operator=(const HttpRequest&) = delete;
        HttpRequest(HttpRequest&&) = delete;
        HttpRequest& operator=(HttpRequest&&) = delete;

        CURL* raw() const noexcept;

        // Called once, after the worker removes the handle from the multi handle.
        // Checks transport/HTTP errors and notifies the response handler.
        void finish(CURLcode result);
    private:
        static size_t writeCallback(char* data, size_t size, size_t count,
                                    void* userdata) noexcept;

        CURL *handle = nullptr;
        curl_slist* headers_list = nullptr;

        std::string url;
        std::string payload;
        std::exception_ptr callback_error;
        bool handler_aborted = false;
        ResponseHandler handler;
};

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
    void handleRequest(const toNetworkMessage& message);
    std::unique_ptr<HttpRequest> createChatRequest(const SendRequest& req, const std::string& key);
    std::unique_ptr<HttpRequest> createNameSessionRequest(const NameSessionReq& req, const std::string& key);
    std::unique_ptr<HttpRequest> createModelsListRequest(const ListModelsReq& req);

    ConcurrentQueue<toNetworkMessage>& in;
    ConcurrentQueue<fromNetworkMessage>& out;
    const HarnessConfig& config;

    std::jthread thread;

    CURLM *multi;
    std::map<CURL*, std::unique_ptr<HttpRequest>> active_requests;
};
