#include <blocking_queue.h>
#include <default_thread.h>
#include <priority_wrapper.h>
#include <gtest/gtest.h>
#include <future>
#include <mutex>
#include <set>
#include <algorithm>
#include "util.h"


struct BlockingQueueTest : public ::testing::Test
{
    // Create a wrapper class which is move only, for testing
    class StrWrapperMoveOnly
    {
    public:
        StrWrapperMoveOnly(std::string value) : _value(std::move(value)) {}
        StrWrapperMoveOnly() = delete;

        // disable copy operations
        StrWrapperMoveOnly(const StrWrapperMoveOnly&) = delete;
        StrWrapperMoveOnly& operator=(const StrWrapperMoveOnly&) = delete;

        // enable move operations
        StrWrapperMoveOnly(StrWrapperMoveOnly&&) = default;
        StrWrapperMoveOnly& operator=(StrWrapperMoveOnly&&) = default;

        // provide a getter
        [[nodiscard]] const std::string& get() const {return _value; }

    private:
        std::string _value;
    };

    class StrWrapperNoCopyNoMove {
    public:
        StrWrapperNoCopyNoMove() = default;

        // disable copy and move operations
        StrWrapperNoCopyNoMove(const StrWrapperMoveOnly &) = delete;
        StrWrapperNoCopyNoMove &operator=(const StrWrapperMoveOnly &) = delete;
        StrWrapperNoCopyNoMove(StrWrapperNoCopyNoMove &&) = delete;
        StrWrapperNoCopyNoMove &operator=(StrWrapperNoCopyNoMove &&) = delete;
    };
};

TEST_F(BlockingQueueTest, SimplePushPopTest)
{
    // to test that queue is indeed thread safe
    using queue_type = BlockingQueue<int>;
    queue_type queue{};
    const int thread_count{10};
    const int variable_count{1000};

    std::vector<DefaultThread> producers{};
    producers.reserve(thread_count);

    for (int i = 0; i < thread_count; ++i)
    {
        producers.emplace_back([&queue]()
                             {
                                 for (int i = 0; i < variable_count; ++i)
                                 {
                                     EXPECT_EQ(queue.push(i), ErrorCode::NO_ERROR);
                                 }
                             });
    }

    for (auto &thread : producers) thread.get_thread().join();
    EXPECT_EQ(thread_count * variable_count, queue.size());

    // now popping begins
    std::vector<DefaultThread> consumers{};
    consumers.reserve(thread_count);

    for (int i = 0; i < thread_count; ++i)
    {
        consumers.emplace_back([&queue]()
        {
            for (int i = 0; i < variable_count; ++i)
            {
                auto num = queue.pop();
                EXPECT_TRUE( num >= 0 && num < variable_count);
            }
        });
    }
    for (auto &thread : consumers) thread.get_thread().join();
    EXPECT_EQ(0, queue.size());
}

TEST_F(BlockingQueueTest, BlockingTest)
{
    using namespace std::chrono_literals;
    using queue_type = BlockingQueue<int>;
    queue_type queue{};
    DefaultThread producer
    {
        [&queue]()
        {
            EXPECT_EQ(queue.push(0), ErrorCode::NO_ERROR);
            EXPECT_EQ(queue.push(1), ErrorCode::NO_ERROR);
            std::this_thread::sleep_for(0.5s);
            EXPECT_EQ(queue.push(2), ErrorCode::NO_ERROR);
        }
    };

    DefaultThread consumer
    {
        [&queue]()
        {
            // allow some buffer time to get items on the queue
            std::this_thread::sleep_for(0.2s);
            EXPECT_EQ(2, queue.size());
            EXPECT_EQ(0, queue.pop());

            EXPECT_EQ(1, queue.size());
            EXPECT_EQ(1, queue.pop());

            // next item will be pushed on queue after 1 sec
            EXPECT_EQ(0, queue.size());

            // now wait for it to be pushed
            std::this_thread::sleep_for(0.5s);
            EXPECT_EQ(1, queue.size());
            EXPECT_EQ(2, queue.pop()); // this will keep waiting for producer to produce
            EXPECT_EQ(0, queue.size());
        }
    };
}

TEST_F(BlockingQueueTest, ConstructionTest)
{
    static_assert(std::is_default_constructible_v<BlockingQueue<int>>);
    static_assert(std::is_default_constructible_v<BlockingQueue<std::packaged_task<void(void)>>>);
    static_assert(std::is_default_constructible_v<BlockingQueue<StrWrapperMoveOnly>>);
    // following fails to compile, there is no way to assert that
    // static_assert(!std::is_default_constructible_v<BlockingQueue<std::mutex>>);
}

TEST_F(BlockingQueueTest, WithMoveOnlyTypes)
{
    using namespace std::chrono_literals;

    const std::string sample_string{"sample_string"};
    using queue_type = BlockingQueue<StrWrapperMoveOnly>;
    queue_type queue{};
    DefaultThread consumer{[&queue, &sample_string]
    {
        auto received_string = queue.pop();
        EXPECT_EQ(received_string.get(), sample_string);
    }};

    // this will go out of scope (joined) in next line
    // so, no need to limit its scope for quick evaluation
    DefaultThread producer{[&sample_string, &queue]()
    {
        EXPECT_EQ(queue.push(sample_string), ErrorCode::NO_ERROR);
    }};
}

TEST_F(BlockingQueueTest, MaxSizeTest)
{
    BlockingQueue<int> queue{20};
    auto nums = get_random_int_vec(queue.get_max_size());
    for ( auto i : nums) {
        EXPECT_EQ(ErrorCode::NO_ERROR, queue.push(i));
    }
    EXPECT_EQ(ErrorCode::QUEUE_FULL, queue.push(0));
}

TEST_F(BlockingQueueTest, PriorityTest) {
    using ElemType = PriorityWrapper<std::function<int()>>;
    using ContainerType = std::multiset<ElemType>;
    using QueueType = BlockingQueue<ElemType, ContainerType>;
    QueueType queue{};

    auto nums = get_vector({4,5,6,1,-1,0,5});
    auto sorted_nums{nums};
    std::sort(sorted_nums.begin(), sorted_nums.end());
    EXPECT_NE(nums, sorted_nums);

    for (auto num : nums)
    {
        EXPECT_EQ(ErrorCode::NO_ERROR, queue.push(num, [num]{return num;}));
    }

    std::vector<int> returned_nums{};
    for (auto n : nums)
    {
        auto popped_elem = queue.pop();
        EXPECT_EQ(popped_elem._priority, popped_elem._task());
        returned_nums.push_back(popped_elem._priority);
    }

    EXPECT_EQ(sorted_nums, returned_nums);
}

TEST_F(BlockingQueueTest, PriorityStressTest) {
    using ElemType = PriorityWrapper<std::function<int()>>;
    using ContainerType = std::multiset<ElemType>;
    using QueueType = BlockingQueue<ElemType, ContainerType>;
    QueueType queue{};

    auto nums = get_random_int_vec(10'000);
    auto sorted_nums{nums};
    std::sort(sorted_nums.begin(), sorted_nums.end());
    EXPECT_NE(nums, sorted_nums);

    for (auto num : nums)
    {
        EXPECT_EQ(ErrorCode::NO_ERROR, queue.push(num, [num]{return num;}));
    }

    std::vector<int> returned_nums{};
    for (auto n : nums)
    {
        auto popped_elem = queue.pop();
        EXPECT_EQ(popped_elem._priority, popped_elem._task());
        returned_nums.push_back(popped_elem._priority);
    }

    EXPECT_EQ(sorted_nums, returned_nums);
}