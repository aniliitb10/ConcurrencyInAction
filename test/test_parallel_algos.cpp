#include "util.h"
#include <parallel_algos.h>
#include <algorithm>
#include <gtest/gtest.h>

struct TestParallelAlgos : public ::testing::Test {
};

TEST_F(TestParallelAlgos, QuickSortTest) {
    auto nums = get_random_int_vec(2'000'000);
    EXPECT_FALSE(std::is_sorted(nums.cbegin(), nums.cend()));

    ParallelQuickSort::sort(nums.begin(), nums.end());
    EXPECT_TRUE(std::is_sorted(nums.cbegin(), nums.cend())) << nums;
}
