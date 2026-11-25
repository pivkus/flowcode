#pragma once
#include "../Tools.hpp"

class ShellTool : public Tool {
    public:
        ToolSchema schema() const override;
        ToolResult execute(const json& args, std::stop_token stop) override;

    private:
#ifdef _WIN32
        static ToolResult WindowsExecute(const json& args, std::stop_token stop);
#else
        static ToolResult PosixExecute(const json& args, std::stop_token stop);
#endif
};
