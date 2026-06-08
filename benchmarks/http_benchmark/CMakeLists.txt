project(http_benchmark)

find_package(Boost QUIET CONFIG)
if(NOT Boost_FOUND)
    message(STATUS "[bench] Boost not found — skipping HTTP benchmark (requires Boost.Beast)")
    return()
endif()

message(STATUS "[bench] Boost found — building HTTP benchmark (Glaze vs Boost.Beast)")

# Route Glaze's networking headers through the shared Asio selector. Boost is
# required here (Boost.Beast), so glaze::asio resolves to the Boost backend and
# carries GLZ_USE_BOOST_ASIO + Boost::headers. Linking it keeps this benchmark's
# direct boost::asio/boost::beast usage and Glaze's headers on the same backend,
# instead of each consumer remembering to set the macro itself (issue #2599).
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
include(glaze-asio)
glaze_setup_asio()

add_executable(http_benchmark http_benchmark.cpp)
target_link_libraries(http_benchmark PRIVATE
  glaze
  glaze::asio
  bencher::bencher
)
target_compile_options(http_benchmark PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
  $<$<CXX_COMPILER_ID:MSVC>:/O2>
)
