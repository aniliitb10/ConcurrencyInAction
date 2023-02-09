#pragma once

#include <blocking_queue.h>
#include <future>
#include <memory>
#include <type_traits>
#include <limits>
#include <thread>

using namespace std::chrono_literals;

/**
 *  A thread pool to manage a group of threads to execute tasks
 *  It uses @BlockingQueue to store and extract tasks
 *
 *  @note: ThreadPool class itself is not thread safe, it is expected to be used from just one thread
 *  - otherwise, mutex must be used for synchronization
 *
 */
class ThreadPool {
public:

    using queue_type = BlockingQueue<std::packaged_task<void()>>;
    using ErrorCode = BlockingQueue<std::packaged_task<void()>>::ErrorCode;

    /**
     * Constructor of ThreadPool class
     * @param size thread pool size, min 1 and default: std::max(std::thread::hardware_concurrency(), 1)
     * @param max_queue_size max queue size, default: std::numeric_limits<std::size_t>::max()
     * @param wait_time wait time for BlockingQueue, default: 1ms
     */
    explicit ThreadPool(size_t size = std::thread::hardware_concurrency(),
                        std::size_t max_queue_size = std::numeric_limits<std::size_t>::max(),
                        std::chrono::milliseconds wait_time = 1ms) :
            _max_thread_count(std::max(size, static_cast<std::size_t>(1))),
            _queue(max_queue_size, wait_time) {}

    /**
     * Enqueues the task for thread pool and returns pair of future and status
     * - queue_type::ErrorCode is ErrorCode::NO_ERROR iff task was enqueued successfully
     * - Otherwise, the task will not be run and future.get() will throw std::future_error exception
     * @tparam Func: The type of task for the thread pool
     * @tparam Args: The type of arguments for the task
     * @param func: The task for the thread pool
     * @param args: The arguments for the task
     * @return std::pair<future, queue_type::ErrorCode>
     * - Future is to get the returned value from the task
     * - ErrorCode is ErrorCode::NO_ERROR iff task was enqueued successfully
     */
    template<typename Func, typename... Args>
    auto add_task(Func &&func, Args &&... args) ->
    std::pair<std::future<std::invoke_result_t<Func, Args...>>, ErrorCode> {
        auto lambda = [func, args...]() { return func(args...); };
        using return_type = std::invoke_result_t<Func, Args...>;
        auto task = std::packaged_task<return_type()>{lambda};
        auto future = task.get_future();
        auto status = _queue.push(
                std::packaged_task<void()>([moved_task = std::move(task)]() mutable { moved_task(); }
                )
        );

        launch_threads(); // just checking if we need to launch more threads
        return std::pair<std::future<return_type>, queue_type::ErrorCode>(std::move(future), status);
    }

    void stop() noexcept {
        _queue.close();
        for (auto &thread: _threads) {
            if (thread.joinable()) thread.join();
        }
    }

    [[nodiscard]] std::size_t get_thread_count() const noexcept {
        return _threads.size();
    }

    [[nodiscard]] std::size_t get_max_thread_count() const noexcept {
        return _max_thread_count;
    }

    [[nodiscard]] std::size_t get_task_count() const noexcept {
        return _queue.size();
    }

    [[nodiscard]] std::size_t get_max_task_count() const noexcept {
        return _queue.get_max_size();
    }

    [[nodiscard]] bool is_stopped() const noexcept {
        return _queue.is_closed();
    }

    virtual ~ThreadPool() {
        stop();
    }

private:
    /**
     * Intention is to create threads only if there is at least one task to execute
     * This is called from constructor as well as when adding a task
     *  - It is called while adding the task because, may be, initially all threads were not created
     *  - So, calling them while adding new tasks will try to utilize maximum quota of threads
     *  This method will never be called simultaneously from different threads
     *  - As this class is not thread safe, @add_task MUST NOT be called from multiple threads
     *  - And @add_task will only be called after construction
     */
    void launch_threads() {
        while (_threads.size() < _max_thread_count) {
            auto [initial_task_ptr, is_closed] = _queue.try_pop();
            if (initial_task_ptr) {
                _threads.emplace_back([this, moved_initial_task_ptr = std::move(initial_task_ptr)]() {
                    (*moved_initial_task_ptr)(); // execute the first task first
                    while (true) {
                        // its only job is to get the task and execute it, continuously
                        if (auto [task_ptr, closed] = _queue.try_pop(); task_ptr) {
                            (*task_ptr)();
                        } else if (closed) {
                            break;
                        }
                    }
                });
            } else {
                // there is no pending task to execute, hence better to not launch any threads
                return;
            }
        }
    }

    // The class is not intended to be thread-safe, hence there is no need to use atomics
    // The only member variable which is accessed via multiple threads is queue_type, and it is thread-safe
    std::size_t _max_thread_count;
    queue_type _queue{};
    std::vector<std::jthread> _threads{};
};