#pragma once

#include <cstdlib>
#include <print>

#include <mutex>

inline bool debug_on(){
    static const bool on = ( std::getenv("FLOWCODE_DEBUG") != nullptr );
    return on;
}

template <class... Types> void debug_print(std::format_string<Types...> fmt, Types&&... args){
    if (!debug_on()) return;
    static std::mutex m; // multiple threads could call this at the same time
    std::lock_guard lock(m);

    std::print(stderr, "[DEBUG] ");
    std::println(stderr, fmt, std::forward<Types>(args)...);
}
