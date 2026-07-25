#include "Tools.hpp"


void ToolRegistry::registerTool(ToolSchema schema){
    auto ptr = std::make_shared<const ToolSchema>(std::move(schema));
    registry[ptr->name] = std::move(ptr);
}

SchemaPtr ToolRegistry::get(std::string_view key){
    if (auto it = registry.find(key); it != registry.end()) return it->second;
    return nullptr;
}
