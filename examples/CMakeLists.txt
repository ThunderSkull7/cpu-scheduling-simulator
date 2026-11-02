add_executable(basic_usage
    basic_usage.cpp
)

target_link_libraries(basic_usage
    PRIVATE
        cpu_scheduler_lib
)

if(MSVC)
    target_compile_options(basic_usage PRIVATE /W4 /permissive-)
else()
    target_compile_options(basic_usage PRIVATE -Wall -Wextra -Wpedantic)
endif()
