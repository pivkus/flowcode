#pragma once

#include <mutex>
#include <optional>
#include <queue>
#include <functional>

template<typename T> class ConcurrentQueue {
    public:
        void enqueue(T element){
            std::lock_guard<std::mutex> lock(mutex);
            queue.push(std::move(element));

            if (wakeup_cb) wakeup_cb();
        }
        std::optional<T> dequeue(){
            std::lock_guard<std::mutex> lock(mutex);
            if (queue.empty()) return std::nullopt;
            T element = std::move(queue.front());
            queue.pop();
            return element;
        }
        void setCallback(std::function<void()> cb){
            std::lock_guard<std::mutex> lock(mutex);
            wakeup_cb = std::move(cb);
        }
    private:
        std::queue<T> queue;
        std::mutex mutex;
        std::function<void()> wakeup_cb; // needs to be thread-safe

};
