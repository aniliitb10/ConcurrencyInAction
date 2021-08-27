#include <blocking_queue.h>
#include <default_thread.h>
#include <gtest/gtest.h>

struct BlockingQueueTest : public ::testing::Test
{
};

TEST_F(BlockingQueueTest, SimplePushPopTest)
{
    // to test that queue is indeed thread safe
    BlockingQueue<int> queue{};
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
                                     queue.push(i);
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
                queue.pop();
            }
        });
    }
    for (auto &thread : consumers) thread.get_thread().join();
    EXPECT_EQ(0, queue.size());
}

TEST_F(BlockingQueueTest, BlockingTest)
{
    using namespace std::chrono_literals;

    BlockingQueue<int> queue{};
    DefaultThread producer
    {
        [&queue]()
        {
            queue.push(0);
            queue.push(1);
            std::this_thread::sleep_for(1s);
            queue.push(2);
        }
    };

    DefaultThread consumer
    {
        [&queue]()
        {
            EXPECT_EQ(0, queue.pop());
            std::this_thread::sleep_for(0.5s);
            EXPECT_EQ(1, queue.pop());
            EXPECT_EQ(2, queue.pop()); // this will keep waiting for producer to produce
        }
    };

    std::this_thread::sleep_for(0.2s);
    EXPECT_EQ(1, queue.size()); // producer produced 2 but consumer consumed 1
    std::this_thread::sleep_for(0.4s);
    EXPECT_EQ(0, queue.size()); // producer produced 2 and consumer consumed 2
    std::this_thread::sleep_for(0.5s);
    EXPECT_EQ(0, queue.size()); // producer produced 3 and consumer consumed 3
}