#include <NetworkWorker.hpp>
#include <Log.hpp>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
template<class... Ts> struct NetworkOverloaded : Ts... { using Ts::operator()...; };
}

HttpRequest::HttpRequest(std::string url, std::vector<std::string> headers,
                         std::string payload, ResponseHandler handler)
: url(std::move(url)), payload(std::move(payload)), handler(std::move(handler)) 
{
    handle = curl_easy_init();
    if (!handle) throw std::runtime_error("Failed to init a CURL handle");

    try {
        const auto check = [](CURLcode code) {
            if (code != CURLE_OK) throw std::runtime_error(curl_easy_strerror(code));
        };
    
        for (const auto& header : headers) {
            auto* next = curl_slist_append(headers_list, header.c_str());
            if (!next) throw std::bad_alloc{};
            headers_list = next;
        }

        check(curl_easy_setopt(handle, CURLOPT_URL, this->url.c_str()));
        check(curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers_list));
        check(curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &HttpRequest::writeCallback));
        check(curl_easy_setopt(handle, CURLOPT_WRITEDATA, this));
        check(curl_easy_setopt(handle, CURLOPT_PRIVATE, this));
        check(curl_easy_setopt(handle, CURLOPT_POSTFIELDS, this->payload.c_str()));
        check(curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE,
                              static_cast<curl_off_t>(this->payload.size())));
        check(curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L));
    } catch (...) {
        curl_easy_cleanup(handle);
        curl_slist_free_all(headers_list);
        throw;
    }
}

HttpRequest::~HttpRequest(){
    curl_easy_cleanup(handle);
    curl_slist_free_all(headers_list);
}

CURL* HttpRequest::raw() const noexcept { return handle; }

size_t HttpRequest::writeCallback(char* data, size_t size, size_t count,
                                 void* userdata) noexcept {
    auto& request = *static_cast<HttpRequest*>(userdata);
    try {

        const size_t length = size * count;
        const bool accepted = std::visit([&](auto& handler) {
            return handler.onBytes(std::string_view(data, length));
        }, request.handler);
        
        if (!accepted) {
            request.handler_aborted = true;
            return CURL_WRITEFUNC_ERROR;
        }
        return length;

    } catch (...) {
        request.callback_error = std::current_exception();
        return CURL_WRITEFUNC_ERROR;
    }
}

void HttpRequest::finish(CURLcode result){
    long status = 0;
    const CURLcode info_result = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
    HttpFailure failure{ .code = result, .status = status };

    if (callback_error) {
        // WriteCallback threw
        try { std::rethrow_exception(callback_error); }
        catch (const std::exception& e) { failure.message = e.what(); }
    } else if (handler_aborted) {
        // Handler rejected the response
        failure.message = "Response handler rejected the response";
    } else if (result != CURLE_OK) {
        // Transfer failures
        failure.message = curl_easy_strerror(result);
    } else if (info_result != CURLE_OK) {
        // Failed to query http status
        failure.code = info_result;
        failure.message = curl_easy_strerror(info_result);
    } else if (status < 200 || status >= 300) {
        failure.message = "HTTP request failed with status " + std::to_string(status);
    } else {
        // No Error paths reached 
        std::visit([](auto& handler) { handler.onComplete(); }, handler);
        return;
    }

    std::visit([&](auto& handler) { handler.onFailure(failure); }, handler);
}

NetworkWorker::NetworkWorker(
    const HarnessConfig& config, 
    ConcurrentQueue<toNetworkMessage>& in, 
    ConcurrentQueue<fromNetworkMessage>& out)
: in(in), out(out), config(config)
{
    multi = curl_multi_init();

    in.setCallback([this]{
        // This wakes up the network thread, will be called from main thread
        curl_multi_wakeup(multi);
    });

    thread = std::jthread([this](std::stop_token stop){ run(stop); });
}

NetworkWorker::~NetworkWorker(){
    thread.request_stop();
    thread.join();
    active_requests.clear();
    curl_multi_cleanup(multi);
}

void NetworkWorker::run(std::stop_token stop){
    int still_running = 0;
    while (!stop.stop_requested()){

        while (auto req = in.dequeue()){
            handleRequest(*req);
        }

        curl_multi_perform(multi, &still_running);

        CURLMsg *m;
        int left;
        while ((m = curl_multi_info_read(multi, &left))){
            void *ptr = nullptr;
            curl_easy_getinfo(m->easy_handle, CURLINFO_PRIVATE, &ptr);
            HttpRequest& request = *static_cast<HttpRequest* >(ptr);

            const CURLcode result = m->data.result;
            curl_multi_remove_handle(multi, request.raw());
            request.finish(result);

            active_requests.erase(m->easy_handle);
        }

        // will wait if there are no active handles or until wakeup
        int numfds = 0;
        curl_multi_poll(multi, nullptr, 0, 1000, &numfds);
    }
}

std::unique_ptr<HttpRequest> NetworkWorker::createChatRequest(const SendRequest& req, const std::string& key){
    return std::make_unique<HttpRequest>(
        "https://openrouter.ai/api/v1/chat/completions",
        std::vector<std::string>{
            "Content-Type: application/json",
            "Authorization: Bearer " + key
        },
        buildChatPayload(req),
        ChatHandler{req.sid, req.tools, out}
    );
}

std::unique_ptr<HttpRequest> NetworkWorker::createNameSessionRequest(const NameSessionReq& req, const std::string& key){
    return std::make_unique<HttpRequest>(
        "https://openrouter.ai/api/v1/chat/completions",
        std::vector<std::string>{
            "Content-Type: application/json",
            "Authorization: Bearer " + key
        },
        buildSessionNamingPayload(req, config),
        NamingHandler{req.sid, out}
    );
}

void NetworkWorker::handleRequest(const toNetworkMessage& message){
    try {
        const auto key = config.getOpenrouterKey();
        if (!key || key->empty()) {
            throw std::runtime_error("OpenRouter API key is not configured");
        }
        auto request = std::visit(NetworkOverloaded{
            [this, &key](const SendRequest& req) { return createChatRequest(req, *key); },
            [this, &key](const NameSessionReq& req) { return createNameSessionRequest(req, *key); }
        }, message);

        CURL* handle = request->raw();
        active_requests.emplace(handle, std::move(request));
    
        const CURLMcode result = curl_multi_add_handle(multi, handle);
        if (result != CURLM_OK) {
            active_requests.erase(handle);
            throw std::runtime_error(curl_multi_strerror(result));
        }
    } catch (const std::exception& e) {
        std::visit(NetworkOverloaded{
            [&](const SendRequest& req) {
                out.enqueue(RequestFailed{ .sid = req.sid, .msg = e.what(), .retry = false });
            },
            [&](const NameSessionReq&) {
                // Naming failure must not fail the concurrent chat request.
                debug_print("Session naming failed: {}", e.what());
            }
        }, message);
    }
}
