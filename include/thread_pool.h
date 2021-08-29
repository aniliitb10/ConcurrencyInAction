#pragma once

#include <blocking_queue.h>
#include <future>
#include <memory>
#include <default_thread.h>
#include <atomic>

template <typename R>
class ThreadPool
{
public:
    ThreadPool(size_t size = std::thread::hardware_concurrency()) :
    _size(std::max(size, static_cast<std::size_t>(1)))
    {
        using namespace std::chrono_literals;

        // if there was no _stop check in for loop
        // it will keep creating threads even when thread_pool was stopped
        for (std::size_t i = 0; i < _size && !_stop; ++i)
        {
            _thread_count.fetch_add(1); // slightly inaccurate but that's fine
            _threads.emplace_back([&]()
            {
                while (!_stop)
                {
                    // its only job is to get the task and execute it, continuously
                    auto task_ptr = _queue.try_pop_for(100ms);
                    if (task_ptr)
                    {
                        auto& task = *task_ptr;
                        task();
                    }
                }
            });
        }
    }

    template <typename Func, typename... Args>
    std::future<R> add_task(Func&& func, Args... args)
    {
        auto lambda = [func, args...]() {return func(args...);};
        std::packaged_task<R()> task{lambda};
        auto future = task.get_future();
        _queue.push(std::move(task));
        return future; // copy-elision guarantees move operation
    }

    void stop() noexcept
    {
        _stop = true;
        for (auto& thread : _threads)
        {
            thread.get_thread().join();
        }
    }

    [[nodiscard]] int get_thread_count() const noexcept
    {
        return _thread_count.load();
    }

private:
    std::size_t _size{};
    BlockingQueue<std::packaged_task<R()>> _queue{};
    std::atomic_bool _stop{false};
    std::atomic_int32_t _thread_count{0};
    std::vector<DefaultThread> _threads{};
};