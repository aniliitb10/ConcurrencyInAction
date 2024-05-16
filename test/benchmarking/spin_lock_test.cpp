#include "spin_lock.h"
#include <thread>
#include <vector>
#include <mutex>
#include <benchmark/benchmark.h>
#include <thread_pool.h>


template<typename Lockable=SpinLock>
static void increment(Lockable &lockable, std::int64_t &num, std::int64_t by = 1l) {
    std::scoped_lock lock(lockable);
    num += by;
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
    const std::int64_t thread_count{2};
    const std::int64_t task_count{1'000'000};
    SpinLock lock{};
    std::int64_t reference_num{0};


    ThreadPool<> thread_pool{thread_count, std::numeric_limits<std::size_t>::max(), 0ms, false};
    for (int i = 0; i < task_count; ++i) {
        thread_pool.add_task([&lock, &reference_num]() { increment(lock, reference_num); });
    }

    for (auto _: state) {
        thread_pool.start();
        thread_pool.stop();
    }
}

BENCHMARK_F(LockingBenchMark, DefaultSpinLock)(benchmark::State &state) {
    const std::int64_t thread_count{2};
    const std::int64_t task_count{1'000'000};
    DefaultSpinLock lock{};
    std::int64_t reference_num{0};


    ThreadPool<> thread_pool{thread_count, std::numeric_limits<std::size_t>::max(), 0ms, false};
    for (int i = 0; i < task_count; ++i) {
        thread_pool.add_task([&lock, &reference_num]() { increment(lock, reference_num); });
    }

    for (auto _: state) {
        thread_pool.start();
        thread_pool.stop();
    }
}

BENCHMARK_F(LockingBenchMark, Mutex)(benchmark::State &state) {
    const std::int64_t thread_count{2};
    const std::int64_t task_count{1'000'000};
    std::mutex lock{};
    std::int64_t reference_num{0};


    ThreadPool<> thread_pool{thread_count, std::numeric_limits<std::size_t>::max(), 0ms, false};
    for (int i = 0; i < task_count; ++i) {
        thread_pool.add_task([&lock, &reference_num]() { increment(lock, reference_num); });
    }

    for (auto _: state) {
        thread_pool.start();
        thread_pool.stop();
    }
}