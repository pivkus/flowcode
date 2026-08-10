#include "Tools.hpp"


void ToolRegistry::registerTool(std::unique_ptr<Tool> tool){
    if (!tool) return;

    ToolSchema schema = tool->schema();
    std::string tool_name = schema.name;

    auto s_ptr = std::make_shared<const ToolSchema>(std::move(schema));
    schemas[tool_name] = std::move(s_ptr);
    tools[tool_name] = std::move(tool);
}

SchemaPtr ToolRegistry::get_schema(std::string_view name) const {
    if (auto it = schemas.find(name); it != schemas.end()) return it->second;
    return nullptr;
}

Tool* ToolRegistry::get_tool(std::string_view name) const {
    if (auto it = tools.find(name); it != tools.end()) return it->second.get();
    return nullptr;
}

