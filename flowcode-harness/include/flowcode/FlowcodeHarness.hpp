#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <filesystem>
#include <mutex>
#include <string>
#include <utility>

#include <flowcode/Messages.hpp>

struct HarnessConfig {
    HarnessConfig(std::filesystem::path persistance_dir,
                  std::filesystem::path system_prompt_path,
                  std::optional<std::string> openrouter_key = std::nullopt)
        : persistance_dir(std::move(persistance_dir)),
          system_prompt_path(std::move(system_prompt_path)),
          openrouter_key(std::move(openrouter_key)) {}

    // Each copy owns its credential and mutex.
    HarnessConfig(const HarnessConfig& other)
        : HarnessConfig(other.persistance_dir, other.system_prompt_path,
                        other.getOpenrouterKey()) {}

    std::filesystem::path persistance_dir;
    std::filesystem::path system_prompt_path;

    std::optional<std::string> getOpenrouterKey() const {
        std::lock_guard lock(key_mutex);
        return openrouter_key;
    }

    void setOpenrouterKey(std::optional<std::string> key) const {
        std::lock_guard lock(key_mutex);
        openrouter_key = std::move(key);
    }

private:
    mutable std::mutex key_mutex;
    mutable std::optional<std::string> openrouter_key;
};

class Harness {
public:
    explicit Harness(HarnessConfig config);
    ~Harness();
    Harness(const Harness&) = delete;
    Harness& operator=(const Harness&) = delete;

    // Call before any other API use, on the owning thread. Check the result.
    // Failure is permanent for this instance; subsequent calls return the same
    // result without reading again. init() must not race with other API calls.
    [[nodiscard]] bool init();

    const HarnessConfig config;

    // Runs on the message-producing thread. Unregistering waits for an active
    // callback; callbacks must not call setMessageCallback themselves.
    using MessageCallback = std::function<void()>;
    // These operations throw std::logic_error until init() succeeds.
    void setMessageCallback(MessageCallback cb);
    void pushMessage(fromUIMessage msg);
    std::optional<toUIMessage> popMessage();
private:
    class Impl;
    
    std::unique_ptr<Impl> impl;
    bool init_attempted = false;
    Impl& initializedImpl();
};
