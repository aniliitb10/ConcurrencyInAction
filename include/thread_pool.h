#pragma once

#include <blocking_queue.h>
#include <future>
#include <memory>
#include <type_traits>
#include <limits>
#include <thread>
#include <priority_wrapper.h>

using namespace std::chrono_literals;

// Following are some aliases to avoid boilerplate code withing functions
using Elem = std::packaged_task<void()>;
using SequentialQueue = BlockingQueue<Elem>;

using PriorityElem = PriorityWrapper<Elem>;
using PriorityQueue = BlockingQueue<PriorityElem, std::multiset<PriorityElem>>;

template <typename Func, typename... Args>
using TaskReturnType = std::pair<std::future<std::invoke_result_t<Func, Args&&...>>, ErrorCode>;

/**
 *  A thread pool to manage a group of threads to execute tasks
 *  It expects a template parameter for the underlying container to store tasks
 *  - By default, it uses @BlockingQueue to store and extract tasks
 *  - Otherwise, @PriorityQueue is another candidate for underlying queue
 *
 *  Threadpool permits concurrent invocation of @add_task member methods and other helper methods
 *  - except @stop
 *  - methods which permit concurrent invocation have been annotated with @thread_safe
 */

template <typename QueueType = SequentialQueue>
class ThreadPool {
public:
    /**
     * Constructor of ThreadPool class
     * @param thread_count thread pool size, min 1 and default: std::max(std::thread::hardware_concurrency(), 1)
     * @param max_queue_size max queue size, default: std::numeric_limits<std::size_t>::max()
     * @param wait_time wait time for BlockingQueue, default: 0ms
     */
    explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency(),
                        std::size_t max_queue_size = std::numeric_limits<std::size_t>::max(),
                        std::chrono::milliseconds wait_time = 0ms) :
            _thread_count(thread_count == 0 ? static_cast<std::size_t>(1) : thread_count),
            _queue(max_queue_size, wait_time) {
        for (std::size_t i = 0; i < thread_count; i++) {
            _threads.emplace_back(&ThreadPool<QueueType>::worker_thread, this);
        }
    }

    /**
     * @thread_safe: can be called from multiple threads simultaneously
     * Enqueues the task sequentially for thread pool and returns pair of future and status
     * - ErrorCode is ErrorCode::NO_ERROR iff task was enqueued successfully
     * - Otherwise, the task will not be run and future.get() will throw std::future_error exception
     * @tparam Func: The type of task for the thread pool
     * @tparam Args: The type of arguments for the task
     * @param func: The task for the thread pool
     * @param args: The arguments for the task
     * @return std::pair<future, ErrorCode>
     * - Future is to get the returned value from the task
     * - ErrorCode is ErrorCode::NO_ERROR iff task was enqueued successfully
     */
    template<typename Func, typename... Args>
    auto add_task(Func &&func, Args &&... args) ->
    std::enable_if_t<std::is_same_v<QueueType, SequentialQueue>, TaskReturnType<Func, Args...>> {
        auto lambda = [func, args...]() { return std::invoke(func, args...); };
        using return_type = std::invoke_result_t<Func, Args...>;
        auto task = std::packaged_task<return_type()>{lambda};
        auto future = task.get_future();
        auto status = _queue.push(
                Elem([moved_task = std::move(task)]() mutable { moved_task(); }
                )
        );
        return std::pair<std::future<return_type>, ErrorCode>(std::move(future), status);
    }

    /**
     * @thread_safe: can be called from multiple threads simultaneously
     * Enqueues the task as per their priority for thread pool and returns pair of future and status
     * - ErrorCode is ErrorCode::NO_ERROR iff task was enqueued successfully
     * - Otherwise, the task will not be run and future.get() will throw std::future_error exception
     * @tparam Func: The type of task for the thread pool
     * @tparam Args: The type of arguments for the task
     * @param priority: The priority of the task for the thread pool
     * @param func: The task for the thread pool
     * @param args: The arguments for the task
     * @return std::pair<future, ErrorCode>
     * - Future is to get the returned value from the task
     * - ErrorCode is ErrorCode::NO_ERROR iff task was enqueued successfully
     */

    template<typename Func, typename... Args>
    auto add_task(int priority, Func &&func, Args &&... args) ->
    std::enable_if_t<std::is_same_v<QueueType, PriorityQueue>, TaskReturnType<Func, Args...>> {
        auto lambda = [func, args...]() { return std::invoke(func, args...); };
        using return_type = std::invoke_result_t<Func, Args...>;
        auto task = std::packaged_task<return_type()>{lambda};
        auto future = task.get_future();
        auto status = _queue.push(
                PriorityElem(priority, [moved_task = std::move(task)]() mutable { moved_task(); }
                )
        );
        return std::pair<std::future<return_type>, ErrorCode>(std::move(future), status);
    }

    /* This should be called only from any one thread (NOT @thread_safe)
     * If it is not called explicitly, then destructor calls it
     * */
    void stop() noexcept {
        _queue.close();
        for (auto &thread: _threads) {
            if (thread.joinable()) thread.join();
        }
    }

    /* @thread_safe: can be called from multiple threads simultaneously
     * Returns the counts of threads */
    [[nodiscard]] std::size_t get_thread_count() const noexcept {
        return _thread_count;
    }

    /* @thread_safe: can be called from multiple threads simultaneously
     * Returns the count of tasks yet to be picked up by worker threads */
    [[nodiscard]] std::size_t get_task_count() const noexcept {
        return _queue.size();
    }

    /* @thread_safe: can be called from multiple threads simultaneously
     * Returns the maximum queue size */
    [[nodiscard]] std::size_t get_max_task_count() const noexcept {
        return _queue.get_max_size();
    }

    /* @thread_safe: can be called from multiple threads simultaneously
     * Returns true if the queue has been stopped (can't enqueue anymore) */
    [[nodiscard]] bool is_stopped() const noexcept {
        return _queue.is_closed();
    }

    virtual ~ThreadPool() {
        stop();
    }

private:

    /**
     * This function is run by newly launched thread
     * Runs continuously unless the queue is closed
     * */
    void worker_thread() {
        // its only job is to get the task and execute it, continuously, hence in a while loop
        while (true) {
            if (auto [task_ptr, closed] = _queue.try_pop(); task_ptr) {
                (*task_ptr)();
            } else if (closed) {  // no more items on the queue (as task_ptr is nullptr) and queue is closed, hence done
                break; // break the infinite loop, so that the thread is now joinable
            } else {
                std::this_thread::yield(); // allow other threads to run
            }
        }
    }

    // The class is not intended to be thread-safe, hence there is no need to use atomics
    // The only member variable which is accessed via multiple threads is QueueType, and it is thread-safe
    const std::size_t _thread_count;
    QueueType _queue;
    std::vector<std::jthread> _threads{};
};