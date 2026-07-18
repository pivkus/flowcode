#pragma once

#include <cstdlib>
#include <fstream>
#include <string>

// Minimal .env loader: KEY=VALUE lines, '#' comments. Doesn't override existing env vars.
inline void loadEnvFile(const std::string &path = ".env"){
    std::ifstream file(path);
    if (!file) return;

    std::string line;
    while (std::getline(file, line)){
        if (line.empty() || line[0] == '#') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        setenv(key.c_str(), value.c_str(), 0);
    }
}
