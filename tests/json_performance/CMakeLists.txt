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
    space_prefill_benchmark
)

foreach(test ${PERF_TESTS})
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE glz_test_exceptions)
    target_include_directories(${test} PRIVATE ${CMAKE_SOURCE_DIR}/include/glaze/ext)
    if (MINGW)
        target_compile_options(${test} PRIVATE "-Wa,-mbig-obj")
    endif ()
    add_test(NAME ${test} COMMAND ${test})
endforeach()

# Optimization Benchmark
include(FetchContent)
FetchContent_Declare(
  simdjson
  URL https://github.com/simdjson/simdjson/archive/refs/tags/v3.11.0.zip
)
# We only need the library, not the tests/benchmarks of simdjson
set(SIMDJSON_JUST_LIBRARY ON CACHE INTERNAL "")
FetchContent_MakeAvailable(simdjson)

add_executable(benchmark_compare_optimization benchmark_compare_optimization.cpp)
# Link against simdjson library target provided by their CMake
target_link_libraries(benchmark_compare_optimization PRIVATE glz_test_exceptions simdjson)
if (MINGW)
    target_compile_options(benchmark_compare_optimization PRIVATE "-Wa,-mbig-obj")
endif ()
add_test(NAME benchmark_compare_optimization COMMAND benchmark_compare_optimization)
