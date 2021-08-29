#pragma once

#include <blocking_queue.h>
#include <future>
#include <memory>
#include <default_thread.h>
#include <atomic>
#include <type_traits>

class ThreadPool
{
public:
    explicit ThreadPool(size_t size = std::thread::hardware_concurrency()) :
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
                                              auto &task = *task_ptr;
                                              task();
                                          }
                                      }
                                  });
        }
    }

    template<typename Func, typename... Args>
    auto add_task(Func &&func, Args &&... args) -> std::future<std::invoke_result_t<Func, Args...>>
    {
        auto lambda = [func, args...]() {return func(args...);};
        using return_type = std::result_of_t<Func(Args...)>;
        auto task_ptr = std::make_shared<std::packaged_task<return_type()>>(lambda);
        auto future = task_ptr->get_future();
        std::packaged_task<void()> stripped_task {[task_ptr](){auto& task = *task_ptr; task();}};
        _queue.push(std::move(stripped_task));
        return future;
    }

    void stop() noexcept
    {
        _stop = true;
        for (auto &thread: _threads)
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
    BlockingQueue<std::packaged_task<void()>> _queue{};
    std::atomic_bool _stop{false};
    std::atomic_int32_t _thread_count{0};
    std::vector<DefaultThread> _threads{};
};