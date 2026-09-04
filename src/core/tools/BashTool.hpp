#pragma once
#include "../Tools.hpp"

class BashTool : public Tool {
    public:
        ToolSchema schema() const override;
        ToolResult execute(const json& args, std::stop_token stop) override;
};