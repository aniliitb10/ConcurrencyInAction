#pragma once

#include <thread>
#include <exception>
#include <type_traits>

/*
 * A thread with default action (if not explicitly actioned) during destruction!
 * either join or detach: can be mentioned at compile time, default is join
 * */

class DefaultThread
{
public:
    enum class Action
    {
        Join,
        Detach
    };

    DefaultThread() noexcept = delete;

    DefaultThread(std::thread&& thread, Action action = Action::Join) : _action(action), _thread(std::move(thread))
    {
        if (!_thread.joinable())
        {
            throw std::logic_error{"No underlying thread"};
        }
    }

    template<class Callable, class... Args, typename = std::enable_if_t<std::is_invocable_v<Callable, Args...>>>
    DefaultThread(Callable&& callable, Args... args, Action action = Action::Join):
    _thread(std::forward<Callable>(callable), std::forward<Args>(args)...),
    _action(action)
    {} // this will be joinable, no need to check

    DefaultThread(DefaultThread&&) noexcept = default;
    DefaultThread& operator=(DefaultThread&&) noexcept = default;

    DefaultThread& operator=(std::thread&& thread) = delete; // as there is no way to set action
    DefaultThread& operator=(const DefaultThread&)  = delete;
    DefaultThread(const DefaultThread&)  = delete;


    virtual ~DefaultThread()
    {
        if (joinable())
        {
            if (_action == Action::Join)
            {
                _thread.join();
            }
            else
            {
                _thread.detach();
            }
        }
    }

    [[nodiscard]] bool joinable() const noexcept
    {
        return _thread.joinable();
    }

    [[nodiscard]] Action get_action() const noexcept
    {
        return _action;
    }

    [[nodiscard]] const std::thread &get_thread() const noexcept
    {
        return _thread;
    }

    [[nodiscard]] std::thread &get_thread() noexcept
    {
        return _thread;
    }

private:
    Action _action;
    std::thread _thread;
};