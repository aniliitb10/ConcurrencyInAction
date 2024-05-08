#include "spin_lock.h"
#include <thread>
#include <vector>
#include <mutex>
#include <benchmark/benchmark.h>


template<typename Lockable=SpinLock>
static void increment(Lockable &lockable, std::int64_t &num, std::int64_t by = 1l) {
    std::scoped_lock lock(lockable);
    num += by;
}

template<class Lockable>
static void simple_test() {
    Lockable lock{};
    std::vector<std::jthread> threads{};
    const std::int64_t thread_count{10};
    const std::int64_t task_count{100000};
    std::int64_t reference_num{0};

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(
                [&lock, &reference_num]() {
                    for (int i = 0; i < task_count; ++i) {
                        increment(lock, reference_num);
                    }
                }
        );
    }

    for (auto &&thread: threads) {
        if (thread.joinable()) thread.join();
    }
}


// just for the sake of benchmarking, with memory_order_seq_cst
class DefaultSpinLock {
    std::atomic_flag _atomic_flag = ATOMIC_FLAG_INIT;

public:
    void lock() {
        while (_atomic_flag.test_and_set()) {
            // keeps spinning if the flag is already set - representing already acquired lock
        }
    }

    void unlock() {
        _atomic_flag.clear();
    }
};

struct LockingBenchMark : public benchmark::Fixture {
};

BENCHMARK_F(LockingBenchMark, SimpleLock)(benchmark::State &state) {
    for (auto _: state) {
        simple_test<SpinLock>();
    }
}

BENCHMARK_F(LockingBenchMark, DefaultSpinLock)(benchmark::State &state) {
    for (auto _: state) {
        simple_test<DefaultSpinLock>();
    }
}

BENCHMARK_F(LockingBenchMark, Mutex)(benchmark::State &state) {
    for (auto _: state) {
        simple_test<std::mutex>();
    }
}