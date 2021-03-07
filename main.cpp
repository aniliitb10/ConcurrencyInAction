#include <iostream>

#include <jthread.h>
#include <util.h>
#include <fstream>
#include <thread>
#include <fmt/format.h>
#include <fmt/ostream.h>

void meta_func(const std::string& msg)
{
  fmt::print("Thread id: {}, {}\n", std::this_thread::get_id(), msg);
  using namespace std::chrono_literals;
  std::ofstream file_handle("log.txt");
  for (int i = 1; i <= 20; ++i)
  {
    std::this_thread::sleep_for(100ms);
    file_handle << fmt::format("{}\t: thread id: {}, time stamp:{}\n",
            i, std::this_thread::get_id(), dt::datetime::local_datetime());
  }
}

int main() {
  std::cout << "Hello, World! current time is: " << dt::datetime::local_datetime() << std::endl;
  // additional arg serves no purpose, it is to test the ctr of j_thread
  j_thread thread{meta_func, "Hello from other thread"};
  fmt::print("thread id: {}, Going to sleep at: {}\n",
          std::this_thread::get_id(), dt::datetime::local_datetime());
  return 0;
}