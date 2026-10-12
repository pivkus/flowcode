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

    using MessageCallback = std::function<void()>;
    // Runs on the message-producing thread. Unregistering waits for an active
    // callback; callbacks must not call setMessageCallback themselves.
    void setMessageCallback(MessageCallback cb);
    void pushMessage(fromUIMessage msg);
    std::optional<toUIMessage> popMessage();
private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
