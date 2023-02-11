#include <gtest/gtest.h>
#include <blocking_queue.h>
#include <functional>

struct TestPriorityWrapper : public ::testing::Test {};

TEST_F(TestPriorityWrapper, BasicTest){

    PriorityWrapper<double, std::function<int()>> wrapper{0.1, []{return 1;}};
    EXPECT_FLOAT_EQ(wrapper._priority, 0.1);
    EXPECT_EQ(wrapper._task(), 1);

    IntPriorityWrapper<std::function<double()>> int_wrapper{1, []{return 1.0;}};
    EXPECT_EQ(int_wrapper._task(), 1.0);
}