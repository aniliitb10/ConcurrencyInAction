#pragma once

#include <type_traits>
#include <queue>
#include <set>
#include <iterator>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <atomic>
#include <limits>
#include <exception>

using namespace std::chrono_literals;

/**
 * This enum represents the cases when enqueuing on the underlying queue failed
 */
enum class ErrorCode : uint8_t {
    NO_ERROR, // Enqueued successfully, no error
    QUEUE_FULL, // Enqueuing failed as queue was full
    QUEUE_CLOSED // Enqueuing failed as queue was closed
};

/**
 * A @std::condition_variable based BlockingQueue to hold a task in queue and supports following methods:
 * - @push: to push an item (task) on the queue
 * - @pop: waits for @std::chrono::milliseconds::max() for an element to be on queue to pop, otherwise throws @std::runtime_error
 * - @try_pop_for: Waits for a user specified time interval for an element to be on queue and returns a pair:
                   - pair.first is a unique pointer containing the item, otherwise nullptr if there was nothing in the queue
                   - pair.second returns true if queue has been closed, otherwise false
 * - @try_pop: like @try_pop_for, only difference is that it tries for @_wait_time milliseconds (set in constructor)

 * @tparam T: type of tasks, can be either executables or executables wrapped inside @PriorityWrapper to add priority
 * @tparam Container: @std::queue<T> or @std::multiset<T>, not any other container. This is enforced in constructor
 *                  - @std::multiset<T> is supported in case item has a priority (e.g. wrapped inside @PriorityWrapper)
 */
template<typename T,
        typename Container = std::queue<T>,
        typename = std::enable_if_t<std::is_move_constructible_v<T>>>
class BlockingQueue {
public:
    /**
     * Constructor for BlockingQueue
     * @param max_size: maximum size of BlockingQueue, defaults to std::numeric_limits<std::size_t>::max()
     * @param wait_time: milliseconds to wait_for during try_pop from the queue, defaults to 1 ms
     */
    explicit BlockingQueue(std::size_t max_size = std::numeric_limits<std::size_t>::max(),
                           std::chrono::milliseconds wait_time = 0ms) :
            _max_size(max_size),
            _wait_time(wait_time) {
        // currently accepted type is only std::queue or std::multiset
        static_assert(std::is_same_v<Container, std::queue<T>> || std::is_same_v<Container, std::multiset<T>>);
    }

    /**
     * To insert an element (constructed using args) in the queue
     * @param elem: element to insert
     * @return ErrorCode::NO_ERROR if insertion was successful, else relevant error code
     */
    template<class... Args>
    [[nodiscard]] auto push(Args &&... args) -> std::enable_if_t<std::is_constructible_v<T, Args && ...>, ErrorCode> {
        {
            std::lock_guard lock_guard(mutex);
            if (const auto code = unsafe_check_if_insertable(); code != ErrorCode::NO_ERROR) return code;
            _queue.emplace(std::forward<Args>(args)...);
        }
        _condition_variable.notify_one();
        return ErrorCode::NO_ERROR;
    }

    /**
     * Waits for en element to be there in the queue
     * No timeout, waits forever (actually just 24 Hr, to be precise, check @try_pop_for).
     * @note: this will keep waiting even if queue has been closed
     * - reason: as the return type is TaskType and not std::unique_ptr<TaskType>,
     *           there is no default value (at least, not for every type) to return when the queue is closed
     * @return the element at the front of the queue
     */
    [[nodiscard]] T pop() noexcept(false) {
        auto elem = std::move(try_pop_for(std::chrono::milliseconds::max()).first);
        if (!elem) {
            throw std::runtime_error("Timed out waiting for task");
        }
        return std::move(*elem);
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

    [[nodiscard]] std::pair<std::unique_ptr<T>, bool> try_pop_for(const std::chrono::milliseconds &wait_time) noexcept {
        // wait_for might wait for way more than the wait_time
        // - e.g. let's say when thread had almost waited for wait_time, then there was a spurious wakeup
        //        and now, as the condition was not satisfied, cv will wait for another interval of wait_time
        //        and this could go on repeatedly for very long time (unbounded wait time)
        // Hence, it is better to call wait_till with a time point (bounded) rather than calling wait_for with duration
        // It can also be called with std::chrono::milliseconds::max() if it is planned to wait forever
        // -- 'forever' is roughly 24h,
        // -- milliseconds::max() (https://en.cppreference.com/w/cpp/chrono/duration/max) doesn't work for some reason
        auto time_limit = std::chrono::high_resolution_clock::now() +
                          ((wait_time == std::chrono::milliseconds::max()) ? 24h : wait_time);

        std::unique_lock lock(mutex);
        if (_condition_variable.wait_until(lock, time_limit, [this]() { return !_queue.empty(); })) {
            if constexpr (std::is_same_v<Container, std::queue<T>>) {
                auto ptr = std::make_unique<T>(std::move(_queue.front()));
                _queue.pop();
                return std::pair<std::unique_ptr<T>, bool>(std::move(ptr), _closed);
            } else if constexpr (std::is_same_v<Container, std::multiset<T>>) {
                auto itr = _queue.begin();
                // extract is the only way to take a move-only object out of a set
                // https://en.cppreference.com/w/cpp/container/multiset/extract
                auto ptr = std::make_unique<T>(std::move(_queue.extract(itr).value()));
                return std::pair<std::unique_ptr<T>, bool>(std::move(ptr), _closed);
            }
        }
        return std::make_pair(nullptr, _closed);
    }

    /**
     * Same as @try_pop_for, only difference is that it tries for @_wait_time milliseconds set during construction
     * @return pair:
     * - pair.first is nullptr if there was nothing in the queue
     * - pair.second returns true if queue has been closed
     */
    [[nodiscard]] std::pair<std::unique_ptr<T>, bool> try_pop() noexcept {
        auto wait_time_milli_s = is_closed() ? 0ms : _wait_time;
        return try_pop_for(wait_time_milli_s);
    }

    /**
     * To close the queue for enqueuing, once it is closed, it can't be opened again
     * - however, if there are elements at the time of queueing,
     *   pop operations can be used to extract those elements from the queue
     */
    void close() noexcept {
        // this closes the queue for any push operations
        // but pop is still allowed
        std::lock_guard lock{mutex};
        _closed = true;
    }

    /* Returns true iff the queue is closed */
    [[nodiscard]] bool is_closed() const noexcept {
        // if it is closed then any waiting thread should stop waiting
        // (and this is the right time to call join() on them)
        std::lock_guard lock{mutex};
        return _closed;
    }

    /* To return the size of the queue */
    [[nodiscard]] std::size_t size() const noexcept {
        std::lock_guard lock_guard(mutex);
        return _queue.size();
    }

    /* To check if the queue is empty */
    [[nodiscard]] bool is_empty() const noexcept {
        return this->size() == 0;
    }

    [[nodiscard]] std::size_t get_max_size() const noexcept {
        return _max_size;
    }

private:
    /**
     * To check the status if an element can be inserted in the queue
     * @note: Lock on mutex must be acquired before calling this as it doesn't acquire the lock
     * @return ErrorCode: NO_ERROR iff queue is not closed and not full, otherwise @returns QUEUE_CLOSED or QUEUE_FULL
     */
    [[nodiscard]] ErrorCode unsafe_check_if_insertable() const noexcept {
        if (_closed) return ErrorCode::QUEUE_CLOSED;
        if (_queue.size() >= _max_size) return ErrorCode::QUEUE_FULL;

        return ErrorCode::NO_ERROR;
    }

    std::atomic_uint64_t _max_size; // it might be read without locking mutex, hence atomic
    std::chrono::milliseconds _wait_time;

    mutable std::mutex mutex{}; // to protect all!
    Container _queue{};
    std::condition_variable _condition_variable{};
    bool _closed{false};
};
