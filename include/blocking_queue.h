#pragma once

#include <type_traits>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <atomic>

template <typename T,
        typename = std::enable_if_t<std::is_move_constructible_v<T>>>
class BlockingQueue
{
public:
    BlockingQueue() = default;

    void push(T&& elem)
    {
        {
            std::lock_guard lock_guard(mutex);
            unsafe_must_not_be_closed();
            _queue.emplace(std::move(elem));
        }
        _condition_variable.notify_one();
    }

    void push(const T& elem)
    {
        {
            std::lock_guard lock_guard(mutex);
            unsafe_must_not_be_closed();
            _queue.emplace(elem);
        }
        _condition_variable.notify_one();
    }

    T pop() noexcept
    {
        std::unique_lock lock(mutex);
        _condition_variable.wait(lock, [this](){ return !_queue.empty();});
        auto front = std::move(_queue.front());
        _queue.pop();
        return front;
    }

    std::unique_ptr<T> try_pop_for(std::chrono::milliseconds ms) noexcept
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

    void close() noexcept
    {
        // this closes the queue for any push operations
        // but pop is still allowed
        std::lock_guard lock(mutex);
        _closed = true;
    }

    [[nodiscard]] bool is_closed() const noexcept
    {
        // if it is closed then any waiting thread should stop waiting
        // (and this is the right time to call join() on them)
        std::lock_guard lock(mutex);
        return _closed;
    }

    std::size_t size() const noexcept
    {
        std::lock_guard lock_guard(mutex);
        return _queue.size();
    }

private:
    void unsafe_must_not_be_closed() const
    {
        // unsafe: mutex must be locked before calling this function
        if (_closed)
        {
            throw std::logic_error{"Queue is already closed"};
        }
    }

    std::queue<T> _queue{};
    mutable std::mutex mutex{};
    std::condition_variable _condition_variable{};
    bool _closed{false};
};
