#include <gtest/gtest.h>

//#include <default_thread.h>
//
//void meta_func(const std::string& msg)
//{
//  fmt::print("Thread id: {}, {}\n", std::this_thread::get_id(), msg);
//  using namespace std::chrono_literals;
//  std::ofstream file_handle("log.txt");
//  for (int i = 1; i <= 20; ++i)
//  {
//    std::this_thread::sleep_for(100ms);
//    file_handle << fmt::format("{}:\t_thread id: {}, time stamp:{}\n",
//            i, std::this_thread::get_id(), dt::datetime::datetime_IST());
//  }
//}

//int main() {
//  std::cout << "Hello, World! current time is: " << dt::datetime::datetime_IST() << std::endl;
//  // additional arg serves no purpose, it is to test the ctr of j_thread
//  j_thread thread{meta_func, "Hello from other _thread"};
//  fmt::print("_thread id: {}, Going to sleep at: {}\n",
//          std::this_thread::get_id(), dt::datetime::datetime_IST());
//  return 0;
//}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}