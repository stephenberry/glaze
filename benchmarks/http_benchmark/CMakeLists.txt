project(http_benchmark)

find_package(Boost QUIET CONFIG)
if(NOT Boost_FOUND)
    message(STATUS "[bench] Boost not found — skipping HTTP benchmark (requires Boost.Beast)")
    return()
endif()

message(STATUS "[bench] Boost found — building HTTP benchmark (Glaze vs Boost.Beast)")

add_executable(http_benchmark http_benchmark.cpp)
target_link_libraries(http_benchmark PRIVATE
  glaze
  bencher::bencher
)
if(TARGET Boost::headers)
    target_link_libraries(http_benchmark PRIVATE Boost::headers)
elseif(TARGET Boost::boost)
    target_link_libraries(http_benchmark PRIVATE Boost::boost)
endif()
target_compile_options(http_benchmark PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
  $<$<CXX_COMPILER_ID:MSVC>:/O2>
)
