add_executable(cpu_scheduler_tests
    test_process.cpp
    test_process_validator.cpp
    test_fcfs_scheduler.cpp
    test_sjf_scheduler.cpp
    test_srtf_scheduler.cpp
    test_rr_scheduler.cpp
)

target_link_libraries(cpu_scheduler_tests
    PRIVATE
        cpu_scheduler_lib
        GTest::gtest
        GTest::gtest_main
)

if(MSVC)
    target_compile_options(cpu_scheduler_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(cpu_scheduler_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GoogleTest)
gtest_discover_tests(cpu_scheduler_tests)
