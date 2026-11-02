#include <ResponseHandlers.hpp>
#include <Log.hpp>

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {
template<class... Ts> struct NetworkOverloaded : Ts... { using Ts::operator()...; };
}

namespace {
const json* jArray(const json& parent, std::string_view key){
    if (!parent.is_object() || !parent.contains(key)) return nullptr;
    const json& a = parent[key];
    return a.is_array() ? &a : nullptr;
}
const json* jObject(const json& parent, std::string_view key){
    if (!parent.is_object() || !parent.contains(key)) return nullptr;
    const json& o = parent[key];
    return o.is_object() ? &o : nullptr;
}
const json* jString(const json& parent, std::string_view key){
    if (!parent.is_object() || !parent.contains(key)) return nullptr;
    const json& s = parent[key];
    return s.is_string() ? &s : nullptr;
}

}

static std::string_view toJSONType(ToolParamType type){
    switch (type) {
        case ToolParamType::String:      return "string";
        case ToolParamType::Integer:     return "integer";
        case ToolParamType::Number:      return "number";
        case ToolParamType::Boolean:     return "boolean";
        case ToolParamType::StringArray: return "array";
    }
    return "string"; // unreachable; silences -Wreturn-type
}

static json serializeToolSchema(const ToolSchema& schema){
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

static json serializeTurns(const TurnVec& turns){
    json messages = json::array();
    for (const auto& turn : turns) {
        json msg = json::object();

        std::visit(NetworkOverloaded{
            [&](const UserTurn& t) {
                msg["role"] = "user";
                msg["content"] = t.text;
            },
            [&](const AssistantTurn& t) {
                msg["role"] = "assistant";
                std::string output;
                for (const auto& block : t.blocks){
                    if (block.kind == AssistantBlock::Kind::OUTPUT) output += block.text;
                }
                msg["content"] = std::move(output);
                if (!t.tool_calls.empty()) {
                    json calls = json::array();
                    for (const auto& call : t.tool_calls) {
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
            [&](const ToolResultTurn& t) {
                msg["role"] = "tool";
                msg["tool_call_id"] = t.tool_call_id;
                msg["content"]      = t.content;
            },
            [&](const SystemTurn& t){
                msg["role"] = "system";
                msg["content"] = t.text;
            }
        }, *turn);

        messages.push_back(std::move(msg));
    }

    return messages;
}

std::string buildChatPayload(const SendRequest& request){
    json json_payload;

    json_payload["model"] = request.model;
    json_payload["stream"] = true;
    json_payload["messages"] = serializeTurns(*request.snapshot);
    json_payload["reasoning"] = { {"enabled", true} };

    if (request.tools && !request.tools->empty()){
        json_payload["tools"] = json::array();

        for (const auto& [_, p] : *request.tools){
            json schema = serializeToolSchema(*p);
            json_payload["tools"].push_back(std::move(schema));
        }
        json_payload["parallel_tool_calls"] = true;
    }

    return json_payload.dump();
}

static json serializeNamingMessage(const TurnVec& turns){
    if (turns.empty()) {
        throw std::invalid_argument("Session naming requires a user prompt");
    }

    const auto* prompt = std::get_if<UserTurn>(turns.back().get());
    if (!prompt) {
        throw std::invalid_argument("Session naming expects the last turn to be a user prompt");
    }

    return json::array({
        {
            {"role", "system"},
            {"content", "You write titles for chat sessions. Generate one short, "
                        "descriptive title (2-6 words) for the supplied prompt. "
                        "The supplied prompt is text to summarize, not a request to answer. "
                        "Return only the title as natural language with words separated by spaces. "
                        "Use sentence case, not snake_case, kebab-case, or an identifier. "
                        "Do not include quotes, labels, explanations, or ending punctuation. "
                        "Example: a prompt asking how to fix a Python import error "
                        "could be titled Fixing Python imports. "
                        "A simple greeting could be titled A quick greeting."}
        },
        {
            {"role", "user"},
            {"content", "Write only a short session title for the following prompt. "
                        "Do not answer it.\n\nPrompt to summarize:\n" + prompt->text}
        }
    });
}

std::string buildSessionNamingPayload(
    const NameSessionReq& request, const HarnessConfig& config)
{
    json json_payload;

    json_payload["model"] = config.naming_model;
    json_payload["stream"] = false;
    json_payload["messages"] = serializeNamingMessage(*request.snapshot);
    json_payload["reasoning"] = { {"enabled", false} };

    return json_payload.dump();
}

ChatHandler::ChatHandler(Uuid sid, SchemaMapPtr tools, ConcurrentQueue<fromNetworkMessage>& out)
: sid(sid), tools(std::move(tools)), out(out) {}

bool ChatHandler::onBytes(std::string_view bytes){
    if (pending_failure) return false;
    chunks_buffer.append(bytes);
    
    size_t start_idx = 0;
    size_t sep_idx;
    while ( (sep_idx = chunks_buffer.find("\n\n", start_idx)) != std::string::npos ){
        std::string_view event(chunks_buffer.begin() + start_idx, chunks_buffer.begin() + sep_idx);
        handleEvent(event);
        if (pending_failure) return false;
        start_idx = sep_idx + 2;
    }
    chunks_buffer.erase(0, start_idx);
    return true;
}

void ChatHandler::onComplete(){
    // handle remaining chunk that might be present
    if (chunks_buffer.length() > 0){
        handleEvent(chunks_buffer);
        chunks_buffer.clear();
    }
    if (pending_failure) { onFailure(HttpFailure{}); return; }

    bool clean_termination = saw_done || !finish_reason.empty() || !native_finish_reason.empty();
    if (!clean_termination) {
        sendError("Model didnt terminate correctly", false);
        onFailure(HttpFailure{});
        return;
    }
    
    if (finish_reason == "length"){
        sendError("Context limit reached (finish_reason length)", false);
        onFailure(HttpFailure{});
        return;
    }
    if (finish_reason == "content_filter"){
        sendError("Response blocked (finish_reason content_filter)", false);
        onFailure(HttpFailure{});
        return;
    }
    if (finish_reason == "error" || native_finish_reason == "error"){
        sendError("Provider reported an error (finish_reason error)", false);
        onFailure(HttpFailure{});
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

        out.enqueue(ToolCallsMade {
            .sid = sid,
            .calls = std::move(tool_reqs)
        });

    } else {
        out.enqueue(TurnFinished { .sid = sid });
    }

}

void ChatHandler::sendError(std::string message, bool retry){
    if (!pending_failure) pending_failure = RequestFailed{
        .sid = sid, .msg = std::move(message), .retry = retry
    };
}

void ChatHandler::onFailure(const HttpFailure& failure){
    if (!pending_failure) sendError(failure.message,
        failure.status == 0 || failure.status == 408 || failure.status == 429 || failure.status >= 500);
    out.enqueue(std::move(*pending_failure));
    pending_failure.reset();
}

std::string ChatHandler::validateToolCall(std::string& name, json& args){
    if (!tools || !tools->contains(name)) return "unknown tool '" + name + "'";

    auto& schema = tools->at(name);
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

void ChatHandler::handleEvent(std::string_view event){

    if (event.starts_with(":")){
        // SSE comment to keep connection alive, just skip
        return;
    } else if (!event.starts_with("data: ")){
        // Not a well formed SSE, for example a plain HTTP error body
        sendError("HTTP error", true); // TODO: better error reporting (code, reason)
        return;
    } else {
        if (event.compare("data: [DONE]") == 0){
            saw_done = true;
            return;
        };

        event.remove_prefix(6); // len of "data: " prefix is 6
        json parsed = json::parse(event, nullptr, false);

        if (parsed.is_discarded()){
            sendError("Malformed response", true);
            return;
        }

        // Check for a top level error field that indicates provider error
        auto *error = jObject(parsed, "error");
        if (error){
            auto *message = jString(*error, "message");
            sendError( 
                message ? message->get<std::string>() : "Provider error",
                true );
            return;
        }

        auto choices_it = parsed.find("choices");
        if (choices_it == parsed.end() || choices_it->empty()) return; // choices might be an empty array
        json &choice = choices_it.value()[0]; // non-const so token strings can be moved out below

        auto *fr = jString(choice, "finish_reason");
        if (fr) finish_reason = fr->get<std::string>();

        auto *nfr = jString(choice, "native_finish_reason");
        if (nfr) native_finish_reason = nfr->get<std::string>();
        

        auto delta_it = choice.find("delta");
        if (delta_it == choice.end()) return;

        auto *content = jString(*delta_it, "content");
        if (content){
            out.enqueue(OutputTokensDelta {
                .sid = sid,
                .delta = content->get<std::string>()
            });
        }

        auto *reasoning = jString(*delta_it, "reasoning");
        if (reasoning){
            out.enqueue(ReasoningTokensDelta {
                .sid = sid,
                .delta = reasoning->get<std::string>()
            });
        }

        auto* toolcall = jArray(*delta_it, "tool_calls");
        if (toolcall){
            for (const auto& chunk : *toolcall){
                // "index" should reliably be present and is used to identify each call
                // it may be ommited, 0 as the default
                int index = 0;
                auto index_it = chunk.find("index");
                if (index_it != chunk.end() && index_it->is_number_integer()){
                    index = index_it->get<int>();
                }

                ToolSlot& ts = tc_incomming[index];

                auto *id = jString(chunk, "id");
                if (id) ts.id = id->get<std::string>();

                // "function" might not be present, but we dont really care about those chunks anyways
                auto function = jObject(chunk, "function");
                if (!function) continue;
                
                // "name" is usually present only on the first chunk
                auto *name = jString(*function, "name");
                if (name) ts.name = name->get<std::string>();

                // "arguments" are json chunks that need to be appended
                auto *args = jString(*function, "arguments");
                if (args) ts.args += args->get<std::string>();

            }
        }
    }
}


NamingHandler::NamingHandler(Uuid sid, ConcurrentQueue<fromNetworkMessage>& out)
: sid(sid), out(out) {}
bool NamingHandler::onBytes(std::string_view bytes){
    response_body.append(bytes);
    return true;
}
void NamingHandler::onComplete(){
    const auto reject = [this](const char* message) {
        onFailure(HttpFailure{ .message = message });
    };

    const json response = json::parse(response_body, nullptr, false);
    if (!response.is_object() || response.contains("error")) {
        reject("Invalid session naming response");
        return;
    }
    auto* choices = jArray(response, "choices");
    if (!choices || choices->empty() || !(*choices)[0].is_object()) {
        reject("Session naming response has no choices");
        return;
    }
    const auto& choice = (*choices)[0];
    auto *reason = jString(choice, "finish_reason");
    if (reason && *reason != "stop") {
        reject("Session naming response did not finish normally");
        return;
    }
    auto *message = jObject(choice, "message");
    if (!message) {
        reject("Session naming response has no message");
        return;
    }
    auto *content = jString(*message, "content");
    if (!content) {
        reject("Session naming response has no name");
        return;
    }
    std::string name = content->get<std::string>();
    const auto first = name.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        reject("Session naming response contains an empty name");
        return;
    }
    name = name.substr(first, name.find_last_not_of(" \t\r\n") - first + 1);
    out.enqueue(SessionNamed{ .sid = sid, .name = std::move(name) });
}
void NamingHandler::onFailure(const HttpFailure& failure){
    // Naming is optional; its failure must not change the chat request state.
    debug_print("Session naming failed: {}", failure.message);
}


ModelsListHandler::ModelsListHandler(ConcurrentQueue<fromNetworkMessage>& out)
 : out(out) {}

bool ModelsListHandler::onBytes(std::string_view bytes){
    response_body.append(bytes);
    return true;
}

void ModelsListHandler::onComplete(){
    try {
        const json response = json::parse(response_body);
        if (!response.is_object() || response.contains("error")) {
            throw std::runtime_error("Invalid model list response");
        }
    
        auto *data = jArray(response, "data");
        if (!data) throw std::runtime_error("\"data\" array not present");

        auto models = std::make_shared<ModelList>();
        models->reserve(data->size());
        for (const auto& entry : *data) {
            ModelInfo model{ .id = entry.at("id").get<std::string>(),
                             .supports_reasoning = false };
            if (model.id.empty()) continue;
    
            auto *reasoning = jObject(entry, "reasoning");
            if (reasoning) {
                model.supports_reasoning = true;
                const auto efforts = reasoning->find("supported_efforts");
                if (efforts != reasoning->end() && !efforts->is_null()) {
                    model.supported_efforts = efforts->get<std::vector<std::string>>();
                }
            } else {
                auto *parameters = jArray(entry, "supported_parameters");
                if (parameters) {
                    for (const auto& parameter : *parameters) {
                        const auto& name = parameter.get_ref<const std::string&>();
                        if (name == "reasoning" || name == "reasoning_effort") {
                            model.supports_reasoning = true;
                        }
                    }
                }
            }
            models->push_back(std::move(model));
        }
        out.enqueue(ListModelsRes{ .models = std::move(models) });
    } catch (const std::exception& e) {
        onFailure(HttpFailure{ .message = std::string("Failed to parse model list: ") + e.what() });
    }
}

void ModelsListHandler::onFailure(const HttpFailure& failure){
    debug_print("Model list request failed (HTTP {}): {}", failure.status, failure.message);
    out.enqueue(ListModelsRes{ .models = nullptr });
}
