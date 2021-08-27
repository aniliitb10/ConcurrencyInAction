#pragma once

#include <type_traits>
#include <queue>
#include <mutex>
#include <condition_variable>


/* A Blocking queue only for items which are nothrow copy and move constructible
 * - because push and pop use these constructors */
template <typename T,
        typename = std::enable_if_t<std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_move_constructible_v<T>>>
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
        _condition_variable.notify_one();
    }

    void push(const T& elem) noexcept
    {
        {
            std::lock_guard lock_guard(mutex);
            _queue.emplace(elem);
        }
        _condition_variable.notify_one();
    }

    T pop() noexcept
    {
        std::unique_lock lock(mutex);
        _condition_variable.wait(lock, [this](){ return !_queue.empty();});
        auto front = _queue.front();
        _queue.pop();
        return front;
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
