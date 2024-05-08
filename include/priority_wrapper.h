#pragma once

#include <functional>

/*
 * To wrap a task with some priority, to make a trivial task a prioritized one
 * - Lower the number, higher the priority
 * An object of the class will be callable and will have same effect as calling task().
 * */
template<typename TaskType>
struct PriorityWrapper {

    template<class... Args>
    explicit PriorityWrapper(int priority, Args &&... args) :
            _priority(priority), _task(std::forward<Args>(args)...) {}

    PriorityWrapper(PriorityWrapper &&) noexcept = default;

    PriorityWrapper &operator=(PriorityWrapper &&) noexcept = default;

    // to make it callable
    auto operator()() -> std::invoke_result_t<TaskType> {
        return _task();
    }

    bool operator<(const PriorityWrapper &rhs) const noexcept {
        return _priority < rhs._priority;
    }

    bool operator==(const PriorityWrapper &rhs) const noexcept {
        return _priority == rhs._priority;
    }

    int _priority;
    TaskType _task;
};