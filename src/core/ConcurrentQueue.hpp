#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <functional>
#include <stop_token>

template<typename T> class ConcurrentQueue {
    public:
        void enqueue(T element){
            {
                std::lock_guard<std::mutex> lock(mutex);
                queue.push(std::move(element));
            }
            // Wakes a thread blocked in wait_dequeue on this queue (if any).
            cv.notify_one();

            if (wakeup_cb) wakeup_cb();
        }
        std::optional<T> dequeue(){
            std::lock_guard<std::mutex> lock(mutex);
            if (queue.empty()) return std::nullopt;
            T element = std::move(queue.front());
            queue.pop();
            return element;
        }
    
        // Blocks until an element is available or stop is requested. 
        std::optional<T> wait_dequeue(std::stop_token stop){
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, stop, [this]{ return !queue.empty(); });
            if (queue.empty()) return std::nullopt; // woken by stop request
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
        std::condition_variable_any cv; // for wait_dequeue
        std::function<void()> wakeup_cb; // needs to be thread-safe

};
