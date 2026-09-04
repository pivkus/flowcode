#include "SessionHandle.hpp"


#include <cassert>
#include <cstdlib>
#include <stdexcept>


SessionHandle::SessionHandle(Uuid id, ConcurrentQueue<fromNetworkMessage> *queue)
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
    debug_print("{}", errmsg);

    out_queue->enqueue(SessionError{
        .sid = session_id,
        .msg = std::move(errmsg)
    });
}

void SessionHandle::resetRequestState(){
    tool_schemas.reset();
    tc_incomming.clear();

    saw_done = false;
    finish_reason.clear();
    native_finish_reason.clear();
}



void SessionHandle::prepareMessage(toNetworkMessage& request){

    resetRequestState();

    // TODO: these thing should be passed in the request
    json_payload["model"] = "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning:free";
    json_payload["stream"] = true;
    json_payload["messages"] = json::array();
    json_payload["reasoning"] = { {"enabled", true} };

    serializeTurnsToJSON(*request.snapshot);
    
    if (request.tools && !request.tools->empty()){
        tool_schemas = std::move(request.tools);
        json_payload["tools"] = json::array();

        for (const auto& [_, p] : *tool_schemas){
            json schema = buildJSONSchema(*p);
            json_payload["tools"].push_back(std::move(schema));
        }
        json_payload["parallel_tool_calls"] = true;
    }

    str_payload = json_payload.dump();
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, str_payload.c_str()); // str_payload needs to live until request finishes, doesnt copy
}

std::string SessionHandle::validateToolCall(std::string& name, json& args){
    if (!tool_schemas || !tool_schemas->contains(name)) return "unknown tool '" + name + "'";

    auto& schema = tool_schemas->at(name);
    for (const ToolParam& param : schema->params ){

        if (!args.contains(param.name)){
            if (param.required) return "missing required parameter '" + param.name + "'";
            else continue;
        }

        // Here we know the key is present, check the type
        json& arg = args.at(param.name);
        switch (param.type){
            using enum ToolParamType;

            case String: {
                if (!arg.is_string()) return "parameter '" + param.name + "' must be a string";
                if (param.allowed_vals.empty()) break;
                // There are only some allowed values for this param
                auto& val = arg.get_ref<std::string&>();
                if (!std::ranges::contains(param.allowed_vals, val)){
                    std::string allowed;
                    for (const auto& v : param.allowed_vals){
                        if (!allowed.empty()) allowed += ", ";
                        allowed += v;
                    }
                    return "parameter '" + param.name + "' must be one of: " + allowed;
                }
                break;
            }
            case Integer: {
                if (!arg.is_number_integer()) return "parameter '" + param.name + "' must be an integer";
                break;
            }
            case Number: {
                if (!arg.is_number()) return "parameter '" + param.name + "' must be a number";
                break;
            }
            case Boolean: {
                if (!arg.is_boolean()) return "parameter '" + param.name + "' must be a boolean";
                break;
            }
            case StringArray: {
                if (!arg.is_array() || !std::ranges::all_of(arg, &json::is_string))
                    return "parameter '" + param.name + "' must be an array of strings";
                break;
            }
            default: {
                break;
            }
        }
    }

    return "";
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
    
    if (finish_reason == "length"){
        sendError("Context limit reached (finish_reason length)");
        return;
    }
    if (finish_reason == "content_filter"){
        sendError("Response blocked (finish_reason content_filter)");
        return;
    }
    if (finish_reason == "error" || native_finish_reason == "error"){
        sendError("Provider reported an error (finish_reason error)");
        return;
    }

    // Log wierd behaviour for easier debugging
    if (finish_reason == "stop" && !tc_incomming.empty()) debug_print("Model stopped but returned tool calls" );
    if (finish_reason == "tool_calls" && tc_incomming.empty()) debug_print("Model stopped with tool_calls yet returned None");

    if (!tc_incomming.empty()){
        ToolCallRequests tool_reqs;
        for (const auto& [_, tc] : tc_incomming){
            // Even incorrectly generated tool calls are sent so the error can be reported back to the model
            // with other potentially correct calls

            ToolCallRequest req = {
                .id = std::move(tc.id),
                .name = std::move(tc.name)
            };

            json parsed_args = json::parse(tc.args, nullptr, false);
            if (parsed_args.is_discarded()){
                req.error = "arguments are not valid JSON";
                tool_reqs.push_back(std::move(req));
                continue;
            }

            // Validate the requested tool call, error is empty when the call is valid
            req.error = validateToolCall(req.name, parsed_args);
            if (!req.error.empty()){
                tool_reqs.push_back(std::move(req));
                continue;
            }

            req.args = std::move(parsed_args);
            tool_reqs.push_back(std::move(req));
        }

        out_queue->enqueue(ToolCallsMade {
            .sid = session_id,
            .calls = std::move(tool_reqs)
        });

    } else {
        out_queue->enqueue(TurnFinished { .sid = session_id });
    }

}

CURL *SessionHandle::raw(){ return handle; }
Uuid SessionHandle::id(){ return session_id; }

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

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

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
        json msg = {{"role", roleStr(turn->role)}};

        std::visit(overloaded{
            [&](const std::string& text) {
                msg["content"] = text;
            },
            [&](const AssistantContent& a) {
                msg["content"] = a.text;
                if (!a.tool_calls.empty()) {
                    json calls = json::array();
                    for (const auto& call : a.tool_calls) {
                        calls.push_back({
                            {"id",   call.id},
                            {"type", "function"},
                            {"function", {
                                {"name",      call.name},
                                {"arguments", call.args.dump()}
                            }}
                        });
                    }
                    msg["tool_calls"] = std::move(calls);
                }
            },
            [&](const ToolResultContent& t) {
                msg["tool_call_id"] = t.tool_call_id;
                msg["content"]      = t.content;
            }
        }, turn->content);

        json_payload["messages"].push_back(std::move(msg));
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
                {"properties", std::move(properties)},
                {"required", std::move(required)}
            }}
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

            out_queue->enqueue(OutputTokensDelta {
                .sid = session_id,
                .delta = std::move(tokens)
            });
        }

        auto reasoning_it = delta_it->find("reasoning");
        if (reasoning_it != delta_it->end() && reasoning_it->is_string()){
            std::string tokens = std::move(reasoning_it->get_ref<std::string&>());

            out_queue->enqueue(ReasoningTokensDelta {
                .sid = session_id,
                .delta = std::move(tokens)
            });
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
