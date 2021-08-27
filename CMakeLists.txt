cmake_minimum_required(VERSION 3.16)
project(Concurrency)

set(CMAKE_CXX_STANDARD 20)
include_directories(include)

# looking for all source files now
file(GLOB test "test/*.cpp")

add_executable(Concurrency main.cpp ${test})
target_link_libraries(Concurrency fmt gtest pthread)  # why does the order matter? pthread then gtest fails to compile
