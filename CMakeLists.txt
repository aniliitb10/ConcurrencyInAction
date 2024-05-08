cmake_minimum_required(VERSION 3.28)
project(Concurrency)

set(CMAKE_CXX_STANDARD 20)
include_directories(include)

# looking for all source files now
file(GLOB test "test/*.cpp")

find_package(GTest REQUIRED)

add_executable(concurrency main.cpp ${test})
target_link_libraries(concurrency ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} pthread)

# adding benchmarking binary
add_executable(benchmarking_bin benchmark.cpp test/benchmarking/spin_lock_test.cpp)
target_link_libraries(benchmarking_bin benchmark pthread)
