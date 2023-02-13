#include <gtest/gtest.h>
#include <default_thread.h>
#include <exception>
#include <thread>
#include "util.h"

//std::size_t terminate_counter{0};

struct TestDefaultThread : public ::testing::Test {
    std::size_t _call_counter{};
};

TEST_F(TestDefaultThread, SimpleTest) {
    EXPECT_EXCEPTION(DefaultThread thread{std::thread{}}, std::logic_error, "No underlying thread");
}

TEST_F(TestDefaultThread, ThreadJoinTest) {
    auto call_counter = _call_counter;
    {
        DefaultThread thread{[this]() { _call_counter++; }}; // ensures join and hence, function completes
    }
    EXPECT_EQ(call_counter + 1, _call_counter);

    // again using std::thread explicitly
    call_counter = _call_counter;
    {
        DefaultThread thread{std::thread{[this]() { _call_counter++; }}};
    }
    EXPECT_EQ(call_counter + 1, _call_counter);
}

TEST_F(TestDefaultThread, ThreadDetachTest) {
    auto call_counter = _call_counter;
    using namespace std::literals::chrono_literals;
    // ensures detach and hence, function takes at least one second to complete
    // hence, call_counter doesn't change in next line
    DefaultThread thread{[this]() {
        std::this_thread::sleep_for(1s);
        _call_counter++;
    }, DefaultThread::Action::Detach};
    EXPECT_EQ(call_counter, _call_counter);

    // but may be sleep for 2 seconds, and then check again
    std::this_thread::sleep_for(2s);
    EXPECT_EQ(call_counter + 1, _call_counter);
}

TEST_F(TestDefaultThread, ConstructibleTest) {
    static_assert(!std::is_trivially_constructible_v<DefaultThread>);
    static_assert(!std::is_copy_constructible_v<DefaultThread>);
    static_assert(!std::is_default_constructible_v<DefaultThread>);

    static_assert(std::is_move_constructible_v<DefaultThread>);
    static_assert(!std::is_constructible_v<DefaultThread>);
}

//TEST_F(TestDefaultThread, TerminateHandlerTest)
//{
//    // terminate handler should never return, this could be the reason core dump happens
//    // even after terminate_handler was set
//    auto lambda= [](){std::cout << "std::terminate was called\n"; terminate_counter++;};
//    std::set_terminate(lambda);
//    std::thread thread1{[](){}};
//    std::thread thread2{[]{}};
//    auto terminate_counter_copy = terminate_counter;
//    thread1 = std::move(thread2); // this should call std::terminate thread1 is still joinable
//    EXPECT_EQ(terminate_counter_copy+1, terminate_counter);
//}



