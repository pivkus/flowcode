#include "SessionHandle.hpp"


#include <cassert>
#include <cstdlib>
#include <stdexcept>


SessionHandle::SessionHandle(uint64_t id, ConcurrentQueue<fromNetworkMessage> *queue)
: session_id(id), out_queue(queue)
{

    const char *api_key = std::getenv("OPENROUTER_API_KEY");
    if (!api_key) throw std::runtime_error("OPENROUTER_API_KEY environment variable not set");

    handle = curl_easy_init();

    if (!handle) throw std::runtime_error("Failed to init a curl handle");

    curl_easy_setopt(handle, CURLOPT_URL, "https://openrouter.ai/api/v1/chat/completions");
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &SessionHandle::writeTrampoline);
    // This is safe becuase this object cant move in memory
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(handle, CURLOPT_PRIVATE, this);

    headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    headers = curl_slist_append(headers, (std::string("Authorization: Bearer ") + api_key).c_str());
    
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

    json_payload["model"] = "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning:free";
    json_payload["stream"] = true;
    json_payload["messages"] = json::array();
    json_payload["reasoning"] = { {"enabled", true} };
}

SessionHandle::~SessionHandle(){
    curl_slist_free_all(headers);
    curl_easy_cleanup(handle);
}

void SessionHandle::sendError(std::string errmsg){
    fromNetworkMessage resp {
        .session_id = session_id,
        .kind = ERROR,
        .content = std::move(errmsg)
    };

    out_queue->enqueue(std::move(resp));
}


void SessionHandle::prepareMessage(toNetworkMessage& request){


    // TODO: these thing should be passed in the request
    json_payload["model"] = "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning:free";
    json_payload["stream"] = true;
    json_payload["messages"] = json::array();
    json_payload["reasoning"] = { {"enabled", true} };

    serializeTurnsToJSON(*request.turns);

    str_payload = json_payload.dump();
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, str_payload.c_str()); // str_payload needs to live until request finishes, doesnt copy
}

void SessionHandle::completeMessage(){

    // handle remaining chunk that might be present
    if (chunks_buffer.length() > 0){
        handleEvent(chunks_buffer);
    }


    fromNetworkMessage done_msg {
        .session_id = session_id,
        .kind = TURN_FINISHED,
        .content = ""
    };

    out_queue->enqueue(std::move(done_msg));
}

CURL *SessionHandle::raw(){ return handle; }
uint64_t SessionHandle::id(){ return session_id; }

size_t SessionHandle::writeTrampoline(char* p, size_t sz, size_t n, void* userdata){
    // userdata is a pointer to self, transfer writeback to a method
    return static_cast<SessionHandle *>(userdata)->writeback(p, sz * n);
}

size_t SessionHandle::writeback(const char* data, size_t len){
    chunks_buffer.append(data, len);

    size_t start_idx = 0;
    size_t sep_idx;
    while ( (sep_idx = chunks_buffer.find("\n\n", start_idx)) != std::string::npos ){
        std::string_view event(chunks_buffer.begin() + start_idx, chunks_buffer.begin() + sep_idx);
        handleEvent(event);
        start_idx = sep_idx + 2;
    }
    chunks_buffer.erase(0, start_idx);
    return len;
}

void SessionHandle::serializeTurnsToJSON(const TurnVec& turns) {
    static constexpr auto roleStr = [](Turn::Role role) -> std::string_view {
        switch (role) {
            case Turn::Role::USER:      return "user";
            case Turn::Role::ASSISTANT: return "assistant";
            case Turn::Role::SYSTEM:    return "system";
            case Turn::Role::TOOL:      return "tool";
        }
        return "user";
    };

    for (const auto& turn : turns) {
        json_payload["messages"].push_back({
            {"role",    roleStr(turn->role)},
            {"content", turn->content}
        });
    }
}

void SessionHandle::handleEvent(std::string_view event){

    if (event.starts_with(":")){
        // SSE comment to keep connection alive, just skip
        // std::println("SSE comment");
        return;
    } else if (!event.starts_with("data: ")){
        // Not a well formed SSE, for example a plain HTTP error body
        sendError("HTTP error"); // TODO: better error reporting (code, reason)
        return;
    } else {
        if (event.compare("data: [DONE]") == 0) return;
    
        event.remove_prefix(6); // len of "data: " prefix is 6
        json parsed = json::parse(event, nullptr, false);

        if (parsed.is_discarded()){
            sendError("Malformed response");
            return;
        }


        auto choices_it = parsed.find("choices");
        // choices might be an empty array
        if (choices_it == parsed.end() || choices_it->empty()) return;
        json &choice = choices_it.value()[0]; // non-const so token strings can be moved out below

        auto delta_it = choice.find("delta");
        if (delta_it == choice.end()) return;


        auto content_it = delta_it->find("content");


        if (content_it != delta_it->end() && content_it->is_string()){
            std::string tokens = std::move(content_it->get_ref<std::string&>());

            fromNetworkMessage resp {
                .session_id = session_id,
                .kind = OUTPUT_TOKENS,
                .content = std::move(tokens)
            };

            out_queue->enqueue(std::move(resp));
        }

        auto reasoning_it = delta_it->find("reasoning");
        if (reasoning_it != delta_it->end() && reasoning_it->is_string()){
            // Reasoning tokens are not appended to the conversation history (except in middle of toolcall)
            // Separate

            // TODO: can one chunk have both content and reasoning? if no this can be simplified
            std::string tokens = std::move(reasoning_it->get_ref<std::string&>());

            fromNetworkMessage resp {
                .session_id = session_id,
                .kind = REASONING_TOKENS,
                .content = std::move(tokens)
            };

            out_queue->enqueue(std::move(resp));
        }
    }
}
