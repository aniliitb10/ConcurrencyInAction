#include <gtest/gtest.h>
#include <thread_pool.h>
#include "util.h"
#include <utility>
#include <thread>

using namespace std::chrono_literals;

struct TestThreadPool : public ::testing::Test {
};

TEST_F(TestThreadPool, SimpleTest) {
    constexpr std::size_t thread_pool_size{2};
    ThreadPool thread_pool{thread_pool_size};
    EXPECT_EQ(thread_pool.get_thread_count(), 0);
    EXPECT_EQ(thread_pool.get_max_thread_count(), thread_pool_size);

    thread_pool.add_task([]() {return 0; });
    EXPECT_EQ(thread_pool.get_thread_count(), 1);
    EXPECT_EQ(thread_pool.get_max_thread_count(), thread_pool_size);

    // it is expected that either the new task will run on same thread, or one more thread might have been created
    thread_pool.add_task([]() {return 0; });
    EXPECT_LE(thread_pool.get_thread_count(), 2);
    EXPECT_EQ(thread_pool.get_max_thread_count(), thread_pool_size);

    // even if more tasks are added, it can't have more than @thread_pool_size threads
    for (int i = 0; i < 20; ++i)
        thread_pool.add_task([i]() {return i; });

    EXPECT_LE(thread_pool.get_thread_count(), thread_pool_size);
    EXPECT_EQ(thread_pool.get_max_thread_count(), thread_pool_size);
}

TEST_F(TestThreadPool, TestWithMultipleTasks) {
    std::vector<int> nums{};
    fill_random_int(std::back_inserter(nums), -100, 100, 1000);
    constexpr std::size_t thread_pool_size{10};

    std::vector<std::future<int>> futures{};
    ThreadPool thread_pool{thread_pool_size};
    EXPECT_EQ(thread_pool.get_thread_count(), 0);
    EXPECT_EQ(thread_pool.get_max_thread_count(), thread_pool_size);

    futures.reserve(nums.size());
    for (auto num: nums) {
        auto ret = thread_pool.add_task([num] { return num; });
        EXPECT_EQ(ret.second, ErrorCode::NO_ERROR);
        futures.emplace_back(std::move(ret.first));
    }

    std::vector<int> received{};
    std::transform(futures.begin(), futures.end(), std::back_inserter(received),
                   [](std::future<int> &future) { return future.get(); });

    thread_pool.stop();
    EXPECT_EQ(thread_pool.get_thread_count(), thread_pool_size);  // checking again, should be same
    EXPECT_EQ(nums.size(), received.size());
    std::sort(nums.begin(), nums.end());
    std::sort(received.begin(), received.end());
    EXPECT_EQ(nums, received);
}

TEST_F(TestThreadPool, DifferentReturnTypeTest) {
    ThreadPool thread_pool{};

    auto future_hello = thread_pool.add_task([]() { return std::string{"hello"}; });
    auto future_int = thread_pool.add_task([]() { return 0; });
    auto future_int2 = thread_pool.add_task([](int num) { return num * 2; }, 5);

    // thread_pool.stop();
    EXPECT_EQ(future_hello.first.get(), "hello");
    EXPECT_EQ(future_int.first.get(), 0);
    EXPECT_EQ(future_int2.first.get(), 10);
}

TEST_F(TestThreadPool, StopTest)
{
    constexpr std::size_t max_thread_count{6};
    constexpr std::size_t max_queue_size{ 2 * max_thread_count};
    constexpr auto task_interval {100ms};

    ThreadPool thread_pool{max_thread_count, max_queue_size};

    EXPECT_EQ(0, thread_pool.get_thread_count());
    EXPECT_EQ(max_thread_count, thread_pool.get_max_thread_count());
    EXPECT_EQ(max_queue_size, thread_pool.get_max_task_count());

    auto tasks = get_random_int_vec(max_thread_count + max_queue_size);
    EXPECT_EQ(max_thread_count + max_queue_size, tasks.size());

    std::vector<std::future<int>> returned_nums{};

    for (auto i : tasks)
    {
        auto ret = thread_pool.add_task(get_slow_task(task_interval, i));
        returned_nums.emplace_back(std::move(ret.first));
        EXPECT_EQ(ErrorCode::NO_ERROR, ret.second);
    }

    // This should be enough to create all @max_thread_count threads
    EXPECT_EQ(max_thread_count, thread_pool.get_thread_count());

    // Each thread has one task to complete, remaining tasks must still be in queue
    EXPECT_EQ(tasks.size() - max_thread_count, thread_pool.get_task_count());

    // Adding another task will not be successful as the queue should be full
    EXPECT_EQ(ErrorCode::QUEUE_FULL, thread_pool.add_task([](){}).second);

    // sleep for more than double the time consumed for each task to let threads finish
    std::this_thread::sleep_for(2 * task_interval + 50ms);
    EXPECT_EQ(0, thread_pool.get_task_count());

    // let's try stopping
    thread_pool.stop();
    EXPECT_TRUE(thread_pool.is_stopped());

    // Adding another task will not be successful as the queue is closed
    EXPECT_EQ(ErrorCode::QUEUE_CLOSED, thread_pool.add_task([](){}).second);
}

TEST_F(TestThreadPool, StopBeforeThreadsFinishTest)
{
    constexpr std::size_t max_thread_count{6};
    constexpr std::size_t max_queue_size{ 2 * max_thread_count};
    constexpr auto task_interval {100ms};

    ThreadPool thread_pool{max_thread_count, max_queue_size};
    auto tasks = get_random_int_vec(max_thread_count + max_queue_size);
    EXPECT_EQ(max_thread_count + max_queue_size, tasks.size());

    std::vector<std::future<int>> returned_nums{};

    for (auto i : tasks)
    {
        auto ret = thread_pool.add_task(get_slow_task(task_interval, i));
        returned_nums.emplace_back(std::move(ret.first));
        EXPECT_EQ(ErrorCode::NO_ERROR, ret.second);
    }

    // This should be enough to create all @max_thread_count threads
    EXPECT_EQ(max_thread_count, thread_pool.get_thread_count());

    // Each thread has one task to complete, remaining tasks must still be in queue
    EXPECT_EQ(tasks.size() - max_thread_count, thread_pool.get_task_count());

    // Adding another task will not be successful as the queue should be full
    EXPECT_EQ(ErrorCode::QUEUE_FULL, thread_pool.add_task([](){}).second);

    // let's try stopping, it joins the threads. Hence, all threads must have finished
    thread_pool.stop();
    EXPECT_TRUE(thread_pool.is_stopped());
    EXPECT_EQ(0, thread_pool.get_task_count());

    // Adding another task will not be successful as the queue is closed
    EXPECT_EQ(ErrorCode::QUEUE_CLOSED, thread_pool.add_task([](){}).second);
}

TEST_F(TestThreadPool, PriorityQueueBasicTest) {
    constexpr std::size_t max_thread_count{2};
    constexpr std::size_t max_queue_size{10};
    constexpr auto task_interval {100ms};

    ThreadPool<PriorityQueueType> thread_pool{max_thread_count, max_queue_size};
    EXPECT_EQ(max_queue_size, thread_pool.get_max_task_count());
    EXPECT_EQ(max_thread_count, thread_pool.get_max_thread_count());

    // using BlockingQueue to store the returned values as it is also thread safe
    BlockingQueue<int> thread_safe_queue{};
    auto func = [&thread_safe_queue, task_interval](int num) {
        std::this_thread::sleep_for(task_interval);
        thread_safe_queue.push(num);
        return num;
    };

    // as the thread count is 2, first 2 threads will keep the threads occupied
    // and then other tasks will be prioritized as per their priority
    std::vector<std::future<int>> returns{};
    const auto task_nums = get_vector({3, 3, 2, 1, 0, 4});
    for (auto n : task_nums)
    {
        auto result = thread_pool.add_task(n, func, n);
        EXPECT_EQ(ErrorCode::NO_ERROR, result.second);
        returns.emplace_back(std::move(result.first));
    }
    EXPECT_EQ(4, thread_pool.get_task_count());

    std::vector<int> actual_result{};
    for (auto&& r : returns) {
        actual_result.push_back(r.get());
    }

    // Since the futures were stored in the order the tasks were pushed
    // hence, the orders must remain same
    EXPECT_EQ(task_nums, actual_result);

    // But the processing was done in the order of priority
    // hence, thread_safe_queue must have stored the results in the order of priority
    // - but since result and priorities are same, it is expected that order will be same as their priority
    // - except for the first 2 elements, as they were picked up by both threads as soon as they were pushed
    std::vector<int> process_order{};
    for (std::size_t i = 0; i < task_nums.size(); ++i) {
        process_order.push_back(thread_safe_queue.pop());
    }
    auto expected_order = get_vector({3, 3, 0, 1, 2, 4});
    EXPECT_EQ(expected_order, process_order);
}
