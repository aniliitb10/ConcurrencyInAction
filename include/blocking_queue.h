#pragma once

#include <type_traits>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>


template <typename T,
        typename = std::enable_if_t<std::is_move_constructible_v<T>>>
class BlockingQueue
{
public:
    BlockingQueue() = default;

    void push(T&& elem) noexcept
    {
        {
            std::lock_guard lock_guard(mutex);
            _queue.emplace(std::move(elem));
        }
        _condition_variable.notify_all();
    }

    void push(const T& elem) noexcept
    {
        {
            std::lock_guard lock_guard(mutex);
            _queue.emplace(elem);
        }
        _condition_variable.notify_all();
    }

    T pop() noexcept
    {
        std::unique_lock lock(mutex);
        _condition_variable.wait(lock, [this](){ return !_queue.empty();});
        auto front = std::move(_queue.front());
        _queue.pop();
        return front;
    }

    std::unique_ptr<T> try_pop_for(std::chrono::milliseconds ms)
    {
        std::unique_lock lock(mutex);
        if (_condition_variable.wait_for(lock, ms, [this]() {return !_queue.empty();}))
        {
            auto ptr = std::make_unique<T>(std::move(_queue.front()));
            _queue.pop();
            return ptr;
        }
        return nullptr;
    }

    std::size_t size() const noexcept
    {
        std::lock_guard lock_guard(mutex);
        return _queue.size();
    }

private:
    std::queue<T> _queue{};
    mutable std::mutex mutex{};
    std::condition_variable _condition_variable{};
};
