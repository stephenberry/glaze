project(json_performance)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

if (MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wa,-mbig-obj")
endif ()

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

set(PERF_TESTS
    json_perf_strings
    json_perf_numbers
    json_perf_objects
    json_perf_discord
    json_perf_benchmark
)

foreach(test ${PERF_TESTS})
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE glz_test_exceptions)
    if (MINGW)
        target_compile_options(${test} PRIVATE "-Wa,-mbig-obj")
    endif ()
    add_test(NAME ${test} COMMAND ${test})
endforeach()

# Optimization Benchmark
add_executable(benchmark_compare_optimization benchmark_compare_optimization.cpp simdjson/simdjson.cpp)
target_include_directories(benchmark_compare_optimization PRIVATE simdjson)
target_link_libraries(benchmark_compare_optimization PRIVATE glz_test_exceptions)
if (MINGW)
    target_compile_options(benchmark_compare_optimization PRIVATE "-Wa,-mbig-obj")
endif ()
add_test(NAME benchmark_compare_optimization COMMAND benchmark_compare_optimization)
