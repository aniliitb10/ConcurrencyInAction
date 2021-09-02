#pragma once

#include <blocking_queue.h>
#include <future>
#include <memory>
#include <atomic>
#include <type_traits>
#include <limits>

using namespace std::chrono_literals;

class ThreadPool
{
public:

    using queue_type = BlockingQueue<std::packaged_task<void()>>;

    explicit ThreadPool(size_t size = std::thread::hardware_concurrency(),
                        std::size_t max_queue_size = std::numeric_limits<std::size_t>::max(),
                        std::chrono::milliseconds wait_time = 10ms) :
            _max_pool_size(std::max(size, static_cast<std::size_t>(1))),
            _queue(max_queue_size, wait_time)
    {
        using namespace std::chrono_literals;

        // it will keep creating threads even when thread_pool was stopped
        for (std::size_t i = 0; i < _max_pool_size; ++i)
        {
            _thread_count.fetch_add(1); // slightly inaccurate but that's fine
            _threads.emplace_back([&queue = _queue]()
                                  {
                                      while (true)
                                      {
                                          // its only job is to get the task and execute it, continuously
                                          if (auto [task_ptr, closed] = queue.try_pop(); task_ptr)
                                          {
                                              (*task_ptr)();
                                          }
                                          else if (closed)
                                          {
                                              break;
                                          }
                                      }
                                  });
        }
    }

    /**
     * Enqueues the task for thread pool and returns pair of future and status
     * - queue_type::ErrorCode is ErrorCode::NO_ERROR iff task was enqueued successfully
     * - Otherwise, the task will not be run and future.get() will throw std::future_error exception
     * @tparam Func: The type of task for the thread pool
     * @tparam Args: The type of arguments for the task
     * @param func: The task for the thread pool
     * @param args: The arguments for the task
     * @return: std::pair<future, queue_type::ErrorCode>
     * - Future is to get the returned value from the task
     * - ErrorCode is ErrorCode::NO_ERROR iff task was enqueued successfully
     */
    template<typename Func, typename... Args>
    auto add_task(Func &&func, Args &&... args) ->
    std::pair<std::future<std::invoke_result_t<Func, Args...>>, queue_type::ErrorCode>
    {
        auto lambda = [func, args...]() { return func(args...); };
        using return_type = std::invoke_result_t<Func, Args...>;
        auto task = std::packaged_task<return_type()>{lambda};
        auto future = task.get_future();
        auto status = _queue.push(
                std::packaged_task<void()>([moved_task = std::move(task)]() mutable { moved_task(); }));
        return std::pair<std::future<return_type>, queue_type::ErrorCode>(std::move(future), status);
    }

    void stop() noexcept
    {
        _queue.close();
        for (auto &thread: _threads)
        {
            thread.join();
        }
    }

    [[nodiscard]] int get_thread_count() const noexcept
    {
        return _thread_count.load();
    }

    virtual ~ThreadPool()
    {
        if (!_queue.is_closed()) stop();
    }

private:
    std::size_t _max_pool_size{};
    queue_type _queue{};
    std::atomic_int32_t _thread_count{0};
    std::vector<std::thread> _threads{};
};