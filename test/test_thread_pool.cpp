#include <gtest/gtest.h>
#include <thread_pool.h>
#include "util.h"
#include <utility>

struct TestThreadPool : public ::testing::Test
{};

TEST_F(TestThreadPool, SimpleTest)
{
    std::vector<int> nums{};
    fill_random_int(std::back_inserter(nums), -100, 100, 1000);
    constexpr std::size_t thread_pool_size{10};

    std::vector<std::future<int>> futures{};
    ThreadPool thread_pool{thread_pool_size};
    futures.reserve(nums.size());
    for (auto num : nums)
    {
        auto ret = thread_pool.add_task([num] {return num;});
        futures.emplace_back(std::move(ret.first));
    }

    std::vector<int> received{};
    std::transform(futures.begin(), futures.end(), std::back_inserter(received),
                   [](std::future<int> &future)
                   {return future.get();});

    thread_pool.stop();
    EXPECT_EQ(thread_pool.get_thread_count(), thread_pool_size);
    EXPECT_EQ(nums.size(), received.size());
    std::sort(nums.begin(), nums.end());
    std::sort(received.begin(), received.end());
    EXPECT_EQ(nums, received);
}

TEST_F(TestThreadPool, DifferentReturnTypeTest)
{
    ThreadPool thread_pool{};
    auto future_hello = thread_pool.add_task([](){return std::string{"hello"};});
    auto future_int = thread_pool.add_task([](){return 0;});
    auto future_int2 = thread_pool.add_task([](int num){return num * 2;}, 5);

    // thread_pool.stop();
    EXPECT_EQ(future_hello.first.get(), "hello");
    EXPECT_EQ(future_int.first.get(), 0);
    EXPECT_EQ(future_int2.first.get(), 10);
}