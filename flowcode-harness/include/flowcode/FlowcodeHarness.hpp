#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <flowcode/Messages.hpp>

class Harness {
public:
    Harness();
    ~Harness();
    Harness(const Harness&) = delete;
    Harness& operator=(const Harness&) = delete;

    // Call before any other API use, on the owning thread. Check the result.
    // Failure is permanent for this instance; subsequent calls return the same
    // result without reading again. init() must not race with other API calls.
    [[nodiscard]] bool init();

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
