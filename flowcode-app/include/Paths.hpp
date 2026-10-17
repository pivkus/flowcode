#pragma once

#include <filesystem>

namespace paths {
    inline const std::filesystem::path& sessionsDir(){
    #if defined(__APPLE__)
        static const std::filesystem::path dir = []{
            const char *home = std::getenv("HOME");
            if (!home) throw std::runtime_error("HOME not set");
            return std::filesystem::path(home) / "Library" / "Application Support" / "flowcode" / "sessions";
        }();
        return dir;
    #else
        #error "Not implemented for this platform"
    #endif
    }
}