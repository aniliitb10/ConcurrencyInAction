#include <gtest/gtest.h>
#include <priority_wrapper.h>
#include <functional>

struct TestPriorityWrapper : public ::testing::Test {};

TEST_F(TestPriorityWrapper, BasicTest){

    PriorityWrapper<std::function<int()>> wrapper1{2, []{return 1;}};
    EXPECT_EQ(wrapper1._priority, 2);
    EXPECT_EQ(wrapper1._task(), 1);

    PriorityWrapper<std::function<double()>> wrapper2{1, []{return 3.0;}};
    EXPECT_FLOAT_EQ(wrapper2._task(), 3.0);
    EXPECT_EQ(wrapper2._priority, 1);
}

TEST_F(TestPriorityWrapper, ComparisionTest) {
    PriorityWrapper<std::function<int()>> wrapper1{2, []{return 1;}};
    PriorityWrapper<std::function<int()>> wrapper2{1, []{return 1;}};
    PriorityWrapper<std::function<int()>> wrapper3{1, []{return 3;}};

    EXPECT_NE(wrapper2, wrapper1);
    EXPECT_LT(wrapper2, wrapper1);
    EXPECT_EQ(wrapper2, wrapper3);
}