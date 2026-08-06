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

)

target_include_directories(
    oj_runner
    PRIVATE
    include 
)