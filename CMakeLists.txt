cmake_minimum_required(VERSION 3.20)

project(CPUSchedulingSimulator
    VERSION 0.1.0
    DESCRIPTION "C++20 CPU Scheduling Simulator"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_EXAMPLES "Build example programs" ON)

# Library target
add_library(cpu_scheduler_lib
    src/process.cpp
    src/process_validator.cpp
    src/process_state.cpp
    src/execution_segment.cpp
    src/scheduling_result.cpp
    src/fcfs_scheduler.cpp
    src/sjf_scheduler.cpp
    src/srtf_scheduler.cpp
    src/rr_scheduler.cpp
    src/gantt_chart.cpp
)

target_include_directories(cpu_scheduler_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(cpu_scheduler_lib PRIVATE /W4 /permissive-)
else()
    target_compile_options(cpu_scheduler_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Unit testing
include(CTest)
if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(tests)
endif()
