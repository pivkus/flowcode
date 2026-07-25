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
    
    if (!request.tools.empty()){
        json_payload["tools"] = json::array();
        for (const SchemaPtr& p : request.tools){
            json schema = buildJSONSchema(*p);
            json_payload["tools"].push_back(std::move(schema));
        }
        json_payload["parallel_tool_calls"] = false;
    }

    str_payload = json_payload.dump();
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, str_payload.c_str()); // str_payload needs to live until request finishes, doesnt copy
}

void SessionHandle::completeMessage(){

    // handle remaining chunk that might be present
    if (chunks_buffer.length() > 0){
        handleEvent(chunks_buffer);
        chunks_buffer.clear();
    }

    bool clean_termination = saw_done || !finish_reason.empty() || !native_finish_reason.empty();
    if (!clean_termination) {
        sendError("Model didnt terminate correctly");
        return;
    }

    // TODO: support and log other finish reasons: content_filter, error
    if (finish_reason == "length"){
        sendError("Context limit reach, truncated (finish_reason length)");
        return;
    }

    std::println("{}", tc_incomming[0].name);
    std::println("{}", tc_incomming[0].args);

    // TODO: log wierd finish_reason/tc_incomming combinations to make bug fixing easier in the future
    // like finish_reason "stop" but tc_incomming has something



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

std::string_view SessionHandle::toJSONType(ToolParamType type){
    switch (type) {
        case ToolParamType::String:      return "string";
        case ToolParamType::Integer:     return "integer";
        case ToolParamType::Number:      return "number";
        case ToolParamType::Boolean:     return "boolean";
        case ToolParamType::StringArray: return "array";
    }
    return "string"; // unreachable; silences -Wreturn-type
}

json SessionHandle::buildJSONSchema(const ToolSchema& schema){
    json properties = json::object();
    json required = json::array();

    for (const ToolParam& prop : schema.params){

        if (prop.type == ToolParamType::StringArray){
            properties[prop.name] = {
                {"type", "array"},
                {"items", {{"type", "string"}}},
                {"description", prop.description}
            };
        } else {
            properties[prop.name] = {
                {"type", toJSONType(prop.type)},
                {"description", prop.description}
            };
        }

        if (!prop.allowed_vals.empty()) properties[prop.name]["enum"] = prop.allowed_vals;
        if (prop.required) required.push_back(prop.name);

    }

    return {
        {"type", "function"},
        {"function", {
            {"name", schema.name},
            {"description", schema.description},
            {"parameters", {
                {"type", "object"},
                {"properties", std::move(properties)}
            }},
            {"required", std::move(required)}
        }}
    };
}

void SessionHandle::handleEvent(std::string_view event){

    if (event.starts_with(":")){
        // SSE comment to keep connection alive, just skip
        return;
    } else if (!event.starts_with("data: ")){
        // Not a well formed SSE, for example a plain HTTP error body
        sendError("HTTP error"); // TODO: better error reporting (code, reason)
        return;
    } else {
        if (event.compare("data: [DONE]") == 0){
            saw_done = true;
            return;
        };

        event.remove_prefix(6); // len of "data: " prefix is 6
        json parsed = json::parse(event, nullptr, false);

        if (parsed.is_discarded()){
            sendError("Malformed response");
            return;
        }

        auto choices_it = parsed.find("choices");
        if (choices_it == parsed.end() || choices_it->empty()) return; // choices might be an empty array
        json &choice = choices_it.value()[0]; // non-const so token strings can be moved out below

        auto fr_it = choice.find("finish_reason");
        if (fr_it != choice.end() && fr_it->is_string()){
            finish_reason = fr_it->get_ref<std::string&>();
        }

        auto nfr_it = choice.find("native_finish_reason");
        if (nfr_it != choice.end() && nfr_it->is_string()){
            native_finish_reason = std::move(nfr_it->get_ref<std::string&>());
        }

        auto delta_it = choice.find("delta");
        if (delta_it == choice.end()) return;



        auto content_it = delta_it->find("content");
        if (content_it != delta_it->end() && content_it->is_string()){
            std::string tokens = std::move(std::move(content_it->get_ref<std::string&>()));
            

            fromNetworkMessage resp {
                .session_id = session_id,
                .kind = OUTPUT_TOKENS,
                .content = std::move(tokens)
            };

            out_queue->enqueue(std::move(resp));
        }

        auto reasoning_it = delta_it->find("reasoning");
        if (reasoning_it != delta_it->end() && reasoning_it->is_string()){
            std::string tokens = std::move(reasoning_it->get_ref<std::string&>());

            fromNetworkMessage resp {
                .session_id = session_id,
                .kind = REASONING_TOKENS,
                .content = std::move(tokens)
            };

            out_queue->enqueue(std::move(resp));
        }

        auto toolcall_it = delta_it->find("tool_calls");
        if (toolcall_it != delta_it->end() && toolcall_it->is_array()){
            for (const auto& chunk : toolcall_it.value()){

                // "index" should reliably be present and is used to identify each call
                // it may be ommited, 0 as the default
                int index = 0;
                auto index_it = chunk.find("index");
                if (index_it != chunk.end() && index_it->is_number_integer()){
                    index = index_it->get<int>();
                }

                ToolSlot& ts = tc_incomming[index];

                auto id_it = chunk.find("id");
                if (id_it != chunk.end() && id_it->is_string()){
                    ts.id = id_it->get_ref<const std::string&>();
                }

                // "function" might not be present, but we dont really care about those chunks anyways
                auto function_it = chunk.find("function");
                if (function_it == chunk.end()) continue;
                
                // "name" is usually present only on the first chunk
                auto name_it = function_it->find("name");
                if (name_it != function_it->end() && name_it->is_string()){
                    ts.name = name_it->get_ref<const std::string&>();
                }

                // "arguments" are json chunks that need to be appended
                auto arg_it = function_it->find("arguments");
                if (arg_it != function_it->end() && arg_it->is_string()){
                    ts.args += arg_it->get_ref<const std::string&>();
                }


            }
        }
    }
}
