#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>


enum class ToolParamType { String, Integer, Number, Boolean, StringArray };

struct ToolParam {
    std::string name;
    std::string description;
    ToolParamType type;
    bool required;

    std::vector<std::string> allowed_vals; // String only, empty is unconstrained
};

struct ToolSchema {
    std::string name;
    std::string description;
    std::vector<ToolParam> params;
};

struct ToolCallRequest {
    std::string name;
    json args;
};

using SchemaPtr = std::shared_ptr<const ToolSchema>;

class ToolRegistry {
    public:
        void registerTool(ToolSchema schema);
        SchemaPtr get(std::string_view key);
    private:
        std::map<std::string, SchemaPtr, std::less<>> registry;

};
