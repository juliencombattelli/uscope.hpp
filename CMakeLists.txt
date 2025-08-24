cmake_minimum_required(VERSION 3.20)
project(uscope LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

add_executable(uscope-playground src/uscope-playground.cpp)

# Find required packages
find_package(Threads REQUIRED)

# Try to find Google Benchmark
find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
    # Download and build Google Benchmark if not found
    include(FetchContent)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.4
    )

    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Enable testing of the benchmark library.")
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "Enable installation of benchmark.")
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "Enable building the unit tests which depend on gtest")

    FetchContent_MakeAvailable(googlebenchmark)
endif()

add_executable(gbench-example src/gbench-example.cpp)
target_link_libraries(gbench-example PRIVATE benchmark::benchmark)
