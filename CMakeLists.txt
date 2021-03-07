cmake_minimum_required(VERSION 3.16)
project(Concurrency)

set(CMAKE_CXX_STANDARD 20)
include_directories(include)

# looking for all source files now
file(GLOB source "src/*.cpp")

add_executable(Concurrency main.cpp ${source})
target_link_libraries(Concurrency
        debug fmtd
        optimized fmt)