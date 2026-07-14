cmake_minimum_required(VERSION 3.18)
project(glaze_benchmarks)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (PROJECT_IS_TOP_LEVEL)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

include(FetchContent)
include(CheckIncludeFileCXX)

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

# Float serialization throughput: glz::to_chars (zmij) fast + size-opt vs std::to_chars
add_executable(float_write_benchmark float_write_benchmark.cpp)
target_link_libraries(float_write_benchmark PRIVATE
  glaze
  bencher::bencher
)
target_compile_options(float_write_benchmark PRIVATE
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

# UTF-8 validation benchmark (stateless whole-buffer validator vs streaming validator)
add_executable(utf8_benchmark utf8_benchmark.cpp)
target_link_libraries(utf8_benchmark PRIVATE
  glaze
  bencher::bencher
)
target_compile_options(utf8_benchmark PRIVATE
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

# Ordered map benchmark (glz::ordered_small_map vs glz::ordered_map)
add_executable(ordered_map_benchmark ordered_map_benchmark.cpp)
target_link_libraries(ordered_map_benchmark PRIVATE
  glaze
  bencher::bencher
)
target_compile_options(ordered_map_benchmark PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
  $<$<CXX_COMPILER_ID:MSVC>:/O2>
)

# Generic JSON map type benchmark (ordered_small_map vs std::map)
add_executable(generic_benchmark generic_benchmark.cpp)
target_link_libraries(generic_benchmark PRIVATE
  glaze
  bencher::bencher
)
target_compile_options(generic_benchmark PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
  $<$<CXX_COMPILER_ID:MSVC>:/O2>
)

# Ordered map memory comparison
check_include_file_cxx("malloc/malloc.h" GLZ_HAS_MALLOC_MALLOC_H)
if (GLZ_HAS_MALLOC_MALLOC_H)
  add_executable(ordered_map_memory ordered_map_memory.cpp)
  target_link_libraries(ordered_map_memory PRIVATE
    glaze
  )
  target_compile_options(ordered_map_memory PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
    $<$<CXX_COMPILER_ID:MSVC>:/O2>
  )
endif()

add_executable(ip_address_validation_benchmark ip_address_validation_benchmark.cpp)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
include(glaze-asio)
glaze_setup_asio()

target_link_libraries(ip_address_validation_benchmark PRIVATE
  glaze
  glaze::asio
  bencher::bencher
)
target_compile_definitions(ip_address_validation_benchmark PRIVATE
  $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
  $<$<PLATFORM_ID:Windows>:NOMINMAX>
)
target_compile_options(ip_address_validation_benchmark PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
  $<$<CXX_COMPILER_ID:MSVC>:/O2>
)

# HTTP server benchmark: Glaze vs Boost.Beast
add_subdirectory(http_benchmark)
