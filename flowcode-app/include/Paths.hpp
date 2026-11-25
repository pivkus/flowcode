#pragma once

#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace paths {
    inline const std::filesystem::path& sessionsDir(){
    #if defined(__APPLE__)
        static const std::filesystem::path dir = []{
            const char *home = std::getenv("HOME");
            if (!home) throw std::runtime_error("HOME not set");
            return std::filesystem::path(home) / "Library" / "Application Support" / "flowcode" / "sessions";
        }();
        return dir;
    #elif defined(_WIN32)
        static const std::filesystem::path dir = []{
            const wchar_t *localAppData = ::_wgetenv(L"LOCALAPPDATA");
            if (!localAppData || !*localAppData) throw std::runtime_error("LOCALAPPDATA not set");
            return std::filesystem::path(localAppData) / "flowcode" / "sessions";
        }();
        return dir;
    #elif defined(__linux__)
        static const std::filesystem::path dir = []{
            const char *xdgDataHome = std::getenv("XDG_DATA_HOME");
            if (xdgDataHome && std::filesystem::path(xdgDataHome).is_absolute())
                return std::filesystem::path(xdgDataHome) / "flowcode" / "sessions";
            // Fallback to HOME if XDG_DATA_HOME is not correct (absolute path)
            const char *home = std::getenv("HOME");
            if (!home || !*home) throw std::runtime_error("HOME not set");
            return std::filesystem::path(home) / ".local" / "share" / "flowcode" / "sessions";
        }();
        return dir;
    #else
        #error "Not implemented for this platform"
    #endif
    }
}
