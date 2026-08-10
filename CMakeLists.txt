cmake_minimum_required(VERSION 3.20)

project(OnlineJudge)


set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(

    oj_runner
    src/main.cpp
    src/runner/process_runner.cpp
    src/judge/judge.cpp
    src/verdict/verdict.cpp
    src/submission/submission.cpp
    src/compiler/compiler.cpp

)

target_include_directories(
    oj_runner
    PRIVATE
    include 
)







# # 1. ProcessRunner library target
# add_library(process_runner src/runner/process_runner.cpp)
# target_include_directories(process_runner PUBLIC include)

# # 2. Download and configure GoogleTest
# include(FetchContent)
# FetchContent_Declare(
#   googletest
#   URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
# )
# # Prevent GoogleTest from overriding compiler/runtime settings on Windows
# set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# FetchContent_MakeAvailable(googletest)

# # 3. Enable CTest testing support
# enable_testing()
# include(GoogleTest)

# # 4. Add the test executable
# add_executable(process_runner_test tests/process_runner_test.cpp)
# target_link_libraries(process_runner_test PRIVATE process_runner GTest::gtest_main)

# # Automatically discover tests for CTest runner
# gtest_discover_tests(process_runner_test)