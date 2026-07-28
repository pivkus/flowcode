#pragma once
#include <functional>
#include <map>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include <json.hpp>
using json = nlohmann::json;


enum class ToolParamType { String, Integer, Number, Boolean, StringArray };

struct ToolParam {
    std::string name;
    std::string description;
    ToolParamType type;
    bool required;

    std::vector<std::string> allowed_vals; // String only, empty is unconstrained
};

struct ToolResult {
    bool ok = false;
    std::string content;
};


// The stop_token is the pool worker's, long running tools should poll it
using ToolFn = std::function<ToolResult(const json& args, std::stop_token stop)>;

struct ToolSchema {
    std::string name;
    std::string description;
    std::vector<ToolParam> params;

    ToolFn execute;
};
using SchemaPtr = std::shared_ptr<const ToolSchema>;
using SchemaMap = std::map<std::string, SchemaPtr, std::less<>>;

struct ToolCallRequest {
    bool correct;

    std::string id; 
    std::string name;
    json args;
};
using ToolCallRequests = std::vector<ToolCallRequest>;

struct ResolvedCall {
    uint64_t call_id;
    ToolFn fn;
    json args;
};

class ToolRegistry {
    public:
        void registerTool(ToolSchema schema, ToolFn fn);
        SchemaPtr get(std::string_view key) const;
    private:
        SchemaMap registry;

};
