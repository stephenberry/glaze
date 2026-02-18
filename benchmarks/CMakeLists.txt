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

# Fetch tsl::ordered_map
FetchContent_Declare(
  ordered_map_tsl
  GIT_REPOSITORY https://github.com/Tessil/ordered-map.git
  GIT_TAG master
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ordered_map_tsl)

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

# String write optimization benchmark (SWAR-only vs SIMD+SWAR)
# The no-SIMD baseline is a shared library so the linker can't deduplicate
# its glz::write template instantiation with the SIMD-enabled one.
add_library(string_write_no_simd SHARED string_write_no_simd.cpp)
target_link_libraries(string_write_no_simd PRIVATE glaze)
target_compile_definitions(string_write_no_simd PRIVATE GLZ_DISABLE_SIMD)
target_compile_options(string_write_no_simd PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
  $<$<CXX_COMPILER_ID:MSVC>:/O2>
)

add_executable(string_write_benchmark string_write_benchmark.cpp)
target_link_libraries(string_write_benchmark PRIVATE
  glaze
  bencher::bencher
  string_write_no_simd
)
target_compile_options(string_write_benchmark PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
  $<$<CXX_COMPILER_ID:MSVC>:/O2>
)

# Binary search benchmark (std::lower_bound vs branchless)
add_executable(binary_search_benchmark binary_search_benchmark.cpp)
target_link_libraries(binary_search_benchmark PRIVATE
  glaze
  bencher::bencher
)
target_compile_options(binary_search_benchmark PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
  $<$<CXX_COMPILER_ID:MSVC>:/O2>
)

# Ordered map benchmark (glz::ordered_map vs tsl::ordered_map)
add_executable(ordered_map_benchmark ordered_map_benchmark.cpp)
target_link_libraries(ordered_map_benchmark PRIVATE
  glaze
  bencher::bencher
  tsl::ordered_map
)
target_compile_options(ordered_map_benchmark PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
  $<$<CXX_COMPILER_ID:MSVC>:/O2>
)
