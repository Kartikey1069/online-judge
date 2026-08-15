cmake_minimum_required(VERSION 3.20)

project(OnlineJudge)


set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(FetchContent)


FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

FetchContent_MakeAvailable(googletest)

add_executable(

    oj_runner
    src/main.cpp
    
)

add_executable(
    oj_tests
    tests/testcase/testsuite_test.cpp
    tests/judge/judge_test.cpp
    tests/runner/process_runner_test.cpp
    tests/support/fixture.cpp
    tests/testrunner/testrunner_test.cpp
)

add_executable(
    fixture_exit_zero
    tests/fixtures/exit_zero.cpp
)

add_executable(
    fixture_print_stdout
    tests/fixtures/print_stdout.cpp
)

add_executable(
    fixture_print_stderr
    tests/fixtures/print_stderr.cpp
)

add_executable(
    fixture_echo_stdin
    tests/fixtures/echo_stdin.cpp
)

add_executable(
    fixture_exit_nonzero
    tests/fixtures/exit_nonzero.cpp
)

add_executable(
    fixture_large_output
    tests/fixtures/large_output.cpp
)
add_executable(
    fixture_print_both
    tests/fixtures/print_both.cpp
)

add_executable(
    fixture_print_args
    tests/fixtures/print_args.cpp
)

add_executable(
    fixture_print_no_newline
    tests/fixtures/print_no_newline.cpp
)
add_executable(
    fixture_large_both
    tests/fixtures/large_both.cpp
)
add_executable(
    fixture_print_binary
    tests/fixtures/print_binary.cpp
)
add_executable(
    fixture_terminate_by_signal
    tests/fixtures/terminate_by_signal.cpp
)

add_library(
    oj_core
    src/runner/process_runner.cpp
    src/judge/judge.cpp
    src/verdict/verdict.cpp
    src/submission/submission.cpp
    src/compiler/compiler.cpp
    src/testrunner/testrunner.cpp
    src/testcase/testsuite.cpp
)


target_link_libraries(
    oj_runner 
    PRIVATE
    oj_core 
)

target_link_libraries(
    oj_tests
    PRIVATE
    oj_core
    GTest::gtest_main
    GTest::gtest
    GTest::gmock
    GTest::gmock_main
)
target_include_directories(
    oj_tests
    PRIVATE
    tests
)


target_compile_definitions(oj_tests PRIVATE
    FIXTURE_DIRECTORY="$<TARGET_FILE_DIR:fixture_echo_stdin>"
)

target_include_directories(
    oj_core
    PUBLIC
    include 
)



enable_testing()

add_test(
    NAME oj_tests
    COMMAND oj_tests
)