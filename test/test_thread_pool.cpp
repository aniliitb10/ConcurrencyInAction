#include <gtest/gtest.h>
#include <thread_pool.h>
#include "util.h"

struct TestThreadPool : public ::testing::Test
{};

TEST_F(TestThreadPool, SimpleTest)
{
    std::vector<int> nums{};
    fill_random_int(std::back_inserter(nums), -100, 100, 1000);
    const std::size_t thread_pool_size{10};

    std::vector<std::future<int>> futures{};
    ThreadPool thread_pool{thread_pool_size};
    futures.reserve(nums.size());
    for (auto num : nums)
    {
        futures.emplace_back(thread_pool.add_task([num] {return num;}));
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