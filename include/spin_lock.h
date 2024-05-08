#pragma once

#include <atomic>

class SpinLock {
    std::atomic_flag _atomic_flag = ATOMIC_FLAG_INIT;

public:
    void lock() {
        while (_atomic_flag.test_and_set(std::memory_order_acquire)) {
            // keeps spinning if the flag is already set - representing already acquired lock
        }
    }

    void unlock() {
        _atomic_flag.clear(std::memory_order_release);
    }
};