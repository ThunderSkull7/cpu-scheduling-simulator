add_executable(cpu_scheduler_tests
    test_process.cpp
    test_process_validator.cpp
    test_fcfs_scheduler.cpp
    test_sjf_scheduler.cpp
    test_srtf_scheduler.cpp
    test_rr_scheduler.cpp
    test_priority_scheduler.cpp
    test_mlq_scheduler.cpp
    test_mlfq_scheduler.cpp
    test_scheduling_engine.cpp
    test_cli.cpp
    test_gantt_chart.cpp
    test_results_renderer.cpp
    test_csv_parser.cpp
    test_csv_workload_importer.cpp
    test_csv_results_exporter.cpp
    test_cli_csv.cpp
    test_scheduling_integration.cpp
    test_pipeline_integration.cpp
    test_cli_integration.cpp
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
