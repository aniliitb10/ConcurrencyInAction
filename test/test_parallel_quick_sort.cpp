//#include "util.h"
//#include <parallel_quick_sort.h>
//#include <algorithm>
//#include <gtest/gtest.h>
//#include <execution>

//struct TestParallelQuickSort : public ::testing::Test {
//};
//
//TEST_F(TestParallelQuickSort, StdParallelSortTest) {
//    auto nums = get_random_int_vec(2'0);
//    EXPECT_FALSE(std::is_sorted(nums.cbegin(), nums.cend()));
//
////    std::sort(std::execution::par, nums.begin(), nums.end());
//    ParallelQuickSort::sort(std::begin(nums), std::end(nums), 2);
//    EXPECT_TRUE(std::is_sorted(nums.cbegin(), nums.cend())) << nums;
//}
