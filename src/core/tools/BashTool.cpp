#include "BashTool.hpp"

#include <unistd.h>
#include <csignal>
#include <algorithm>
#include <cstdint>

// reproc++ library for executing commands
#include <reproc++/reproc.hpp>
#include <reproc++/drain.hpp>

namespace {
    constexpr int bash_default_timeout = 120;
    constexpr int bash_max_timeout =  600;
    constexpr int bash_max_output_chars =  600;
    constexpr std::string_view bash_truncation_msg = "TOOL SYSTEM WARNING: command output was truncated!";
    
}

class CappedStringSink {
    std::string& out_str;
    std::size_t max;
    bool& truncated;
public:
    CappedStringSink(std::string& out, std::size_t max, bool& truncated)
        : out_str(out), max(max), truncated(truncated) {}

    // TODO: right now we truncate at the end - apperantly it is better to truncate in the middle
    std::error_code operator()(reproc::stream, const uint8_t* buffer, std::size_t size) {
        std::size_t room = out_str.size() < max ? max - out_str.size() : 0;
        if (room > 0) {
            out_str.append(reinterpret_cast<const char*>(buffer), std::min(size, room));
        }
        if (size > room) truncated = true;

        // Always returns success, so reproc keeps reading the pipes
        return {};
    }
};

static inline std::string error_str(std::error_code ec){
    if (ec.value() == static_cast<int>(std::errc::timed_out)){
        return "ERROR: Timeout reached, try again with a higher value if possible - " + ec.message();
    } else {
        return "ERROR: Internal error in tool - " + ec.message();
    }
}

ToolSchema BashTool::schema() const {
    return ToolSchema {
        .name = "bash",
        .description = "Execute a bash command and return its output.",
        .params = {
            ToolParam {
                .name = "command",
                .description = "The bash command to execute.",
                .type = ToolParamType::String,
                .required = true
            },
            ToolParam {
                .name = "timeout",
                .description = "The timeout for the command in seconds (default: 120, max: 600)",
                .type = ToolParamType::Integer,
                .required = false
            }
        }
    };
}


ToolResult BashTool::execute(const json& args, std::stop_token stop) {

    ToolResult result = { .ok = true, .content = "" };
    // Here the args should already be validated against tool schema
    // It is not the execute's responsibility to handle missing keys
    json cmd_obj = args.at("command");
    std::string& cmd = cmd_obj.get_ref<std::string&>();

    int timeout_sec = bash_default_timeout;
    if (args.contains("timeout")){
        int provided_timeout = args.at("timeout").get<int>();
        if (provided_timeout > 0){
            timeout_sec = provided_timeout < bash_max_timeout ? provided_timeout : bash_max_timeout;
        }
    }

    reproc::process process;

    // Include "-c" flag so that bash executes the unsplit command from the string
    std::vector<char*> sh_args = {
        const_cast<char*>("/bin/sh"), 
        const_cast<char*>("-c"),
        cmd.data()
    };

    // execvp expects a null-terminated array
    sh_args.push_back(nullptr);

    // Fork ourselfs so when can call setsid - create a new process group - before exec
    // This allows us to kill any child processes bash will spawn (same pgid)
    auto [in_child, f_ec] = process.fork(reproc::options{ 
        .deadline = reproc::milliseconds(timeout_sec * 1000)
    });
    if (f_ec){
        // No cleanup needed here
        result.ok = false;
        result.content = error_str(f_ec);
        return result;
    }

    if (in_child){
        if (setsid() == -1) _exit(127);
        execvp(sh_args[0], sh_args.data());
        _exit(127); // exec failed
    }

    auto [pid, p_ec] = process.pid();
    assert(!p_ec); // this cannot fail after fork retunrns


    // kill processes by pid/pgid is safe until wait() returns and reaps them
    // after that they might be recycled by OS and are not safe to kill(), guard with "safe_kill"
    std::atomic<bool> safe_kill{true};

    std::stop_callback on_stop(stop, [pid, &safe_kill]{
        if (safe_kill.load(std::memory_order_acquire)){
            // -pid to kill the group - all the processes bash spawned
            ::kill(-pid, SIGKILL);
            // TODO: right now we straight up kills (instead of terminate-wait-kill) because its easier
            //       its not a big deal - investigate if its worth doing properly
        }
    });

    std::string stdout_str;
    std::string stderr_str;
    
    bool out_trunc = false;
    CappedStringSink out_sink(stdout_str, bash_max_output_chars, out_trunc);
    bool err_trunc = false;
    CappedStringSink err_sink(stderr_str, bash_max_output_chars, err_trunc);

    // Collect output and error streams into strings, will block until streams return EOF
    auto d_ec = reproc::drain(process, out_sink, err_sink);
    if (d_ec){
        ::kill(-pid, SIGKILL);
        result.ok = false;
        result.content = error_str(d_ec);
    }

    int exit_code = 0;
    std::error_code w_ec;

    // process might still be running even after drain returns, wait until it terminates
    std::tie(exit_code, w_ec) = process.wait(reproc::infinite);
    safe_kill.store(false, std::memory_order_release);

    if (w_ec){
        result.ok = false;
        result.content = error_str(w_ec);
    }

    // Any failure path will populate the result object
    if (!result.ok) return result;

    if (out_trunc) stdout_str.append(bash_truncation_msg);
    if (err_trunc) stderr_str.append(bash_truncation_msg);
    
    json json_response;
    json_response["exit code"] = exit_code;
    if (out_trunc || err_trunc) json_response["truncated"] = true;

    json_response["stdout"] = std::move(stdout_str);
    json_response["stderr"] = std::move(stderr_str);


    result.content = json_response.dump();
    return result;

}