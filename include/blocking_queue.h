#pragma once

#include <type_traits>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <atomic>
#include <limits>

using namespace std::chrono_literals;

template <typename T,
        typename = std::enable_if_t<std::is_move_constructible_v<T>>>
class BlockingQueue
{
public:

    /**
     * This enum represents the cases when enqueuing on the underlying queue failed
     */
    enum class ErrorCode
    {
        NO_ERROR, // Enqueued successfully, no error
        QUEUE_FULL, // Enqueuing failed as queue was full
        QUEUE_CLOSED // Enqueuing failed as queue was closed
    };

    /**
     * Constructor for BlockingQueue
     * @param max_size: maximum size of BlockingQueue, defaults to std::numeric_limits<std::size_t>::max()
     * @param wait_time: milliseconds to wait_for during try_pop from the queue, defaults to zero
     */
    explicit BlockingQueue(std::size_t max_size = std::numeric_limits<std::size_t>::max(),
                           std::chrono::milliseconds wait_time = 0ms):
    _max_size(max_size),
    _wait_time(wait_time)
    {}

    /**
     * To insert an element (constructed using args) in the queue
     * @param elem: element to insert
     * @return true if insertion was successful
     */
    template<class... Args>
    [[nodiscard]] auto push(Args&& ... args) -> std::enable_if_t<std::is_constructible_v<T, Args...>, ErrorCode>
    {
        {
            std::lock_guard lock_guard(mutex);
            if (auto code = unsafe_check_if_insertable(); code != ErrorCode::NO_ERROR) return code;
            _queue.emplace(std::forward<Args>(args)...);
        }
        _condition_variable.notify_one();
        return ErrorCode::NO_ERROR;
    }


    /**
     * Waits for en element to be there in the queue
     * No timeout, waits forever.
     * @note: this will keep waiting even if queue has been closed
     * - reason: as the return type is T and not std::unique_ptr<T>,
     *           there is no default value (at least, not for every type) to return when the queue is closed
     * @return the element at the front of the queue
     */
    [[nodiscard]] T pop() noexcept
    {
        std::unique_lock lock(mutex);
        _condition_variable.wait(lock, [this](){ return !_queue.empty();});
        auto front = std::move(_queue.front());
        _queue.pop();
        return front;
    }

    /**
     * @try_pop_for returns a pair:
     * Returning pair is necessary to differentiate between
     * 1) queue is just empty at the moment
     * 2) queue has been closed, so no more elements will be inserted in the queue
     * However, it should be noted that there still could be elements when queue was closed,
     * so this method then returns a not-nullptr as pair.first and true as pair.second
     *
     * @param wait_time milliseconds to wait for an element in the queue,
     * - returns instantly if an element is already in the queue
     * @return pair:
     * - pair.first is nullptr if there was nothing in the queue
     * - pair.second returns true if queue has been closed
     */

    [[nodiscard]] std::pair<std::unique_ptr<T>, bool> try_pop_for(const std::chrono::milliseconds& wait_time) noexcept
    {
        // wait_for might wait for way more than the wait_time
        // - e.g. let's say when thread had almost waited for wait_time, then there was a spurious wakeup
        //        and now, as the condition was not satisfied, cv will wait for another interval of wait_time
        //        and this could go on repeatedly for very long time (unbounded wait time)
        // Hence, it is better to call wait_till with a time point (bounded) rather than calling wait_for with duration
        auto time_limit = std::chrono::high_resolution_clock::now() + wait_time;

        std::unique_lock lock(mutex);
        if (_condition_variable.wait_until(lock, time_limit, [this]() {return !_queue.empty();}))
        {
            auto ptr = std::make_unique<T>(std::move(_queue.front()));
            _queue.pop();
            return std::pair<std::unique_ptr<T>, bool>(std::move(ptr), _closed);
        }
        return std::make_pair(nullptr, _closed);
    }

    /**
     * Same as @try_pop_for, only difference is that it tries for @_wait_time milliseconds set during construction
     * @return pair:
     * - pair.first is nullptr if there was nothing in the queue
     * - pair.second returns true if queue has been closed
     */
    [[nodiscard]] std::pair<std::unique_ptr<T>, bool> try_pop() noexcept
    {
        return try_pop_for(_wait_time);
    }

    /**
     * To close the queue for enqueuing, once it is closed, it can't be opened again
     * - however, if there are elements at the time of queueing,
     *   pop operations can be used to extract those elements from the queue
     */
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

    [[nodiscard]] std::size_t size() const noexcept
    {
        std::lock_guard lock_guard(mutex);
        return _queue.size();
    }

private:
    /**
     * To check the status if an element can be inserted in the queue
     * @note: Lock on mutex must be acquired before calling this as it doesn't acquire the lock
     * @return ErrorCode: NO_ERROR iff queue is not closed and not full, otherwise @returns QUEUE_CLOSED or QUEUE_FULL
     */
    [[nodiscard]] ErrorCode unsafe_check_if_insertable() const noexcept
    {
        if (_closed) return ErrorCode::QUEUE_CLOSED;
        if (_queue.size() >= _max_size) return ErrorCode::QUEUE_FULL;

        return ErrorCode::NO_ERROR;
    }

    std::size_t _max_size;
    std::chrono::milliseconds _wait_time;

    mutable std::mutex mutex{}; // to protect all!
    std::queue<T> _queue{};
    std::condition_variable _condition_variable{};
    bool _closed{false};
};
