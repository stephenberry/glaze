cmake_minimum_required(VERSION 3.18)
project(glaze_benchmarks)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch bencher
FetchContent_Declare(
  bencher
  GIT_REPOSITORY https://github.com/stephenberry/bencher.git
  GIT_TAG main
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(bencher)

# Glaze - use parent directory as header-only library
add_library(glaze INTERFACE)
target_include_directories(glaze INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Lazy JSON benchmark
add_executable(lazy_benchmark lazy_benchmark.cpp)
target_link_libraries(lazy_benchmark PRIVATE
  glaze
  bencher::bencher
)
target_compile_options(lazy_benchmark PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
  $<$<CXX_COMPILER_ID:MSVC>:/O2>
)

# Simple float benchmark
add_executable(simple_float_benchmark simple_float_benchmark.cpp)
target_link_libraries(simple_float_benchmark PRIVATE
  glaze
  bencher::bencher
)
target_compile_options(simple_float_benchmark PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
  $<$<CXX_COMPILER_ID:MSVC>:/O2>
)

# skip_ws optimization benchmark (PR #2269)
add_executable(skip_ws_benchmark skip_ws_benchmark.cpp)
target_link_libraries(skip_ws_benchmark PRIVATE
  glaze
  bencher::bencher
)
target_compile_options(skip_ws_benchmark PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
  $<$<CXX_COMPILER_ID:MSVC>:/O2>
)

# String write optimization benchmark (old SWAR vs new SIMD)
add_executable(string_write_benchmark string_write_benchmark.cpp)
target_link_libraries(string_write_benchmark PRIVATE
  glaze
  bencher::bencher
)
target_compile_options(string_write_benchmark PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
  $<$<CXX_COMPILER_ID:MSVC>:/O2>
)
