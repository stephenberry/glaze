project(ws_benchmark)

# uWebSockets uses epoll/kqueue — not available on Windows
if(WIN32)
    message(STATUS "[bench] WebSocket benchmark not supported on Windows — skipping")
    return()
endif()

find_package(Boost QUIET CONFIG)
if(NOT Boost_FOUND)
    message(STATUS "[bench] Boost not found — skipping WebSocket benchmark (requires Boost.Beast client)")
    return()
endif()

include(FetchContent)

# Download uWebSockets and uSockets sources (Makefile projects, no CMakeLists.txt)
FetchContent_Declare(uWebSockets
  GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
  GIT_TAG master
  GIT_SHALLOW TRUE
)
FetchContent_Declare(uSockets
  GIT_REPOSITORY https://github.com/uNetworking/uSockets.git
  GIT_TAG master
  GIT_SHALLOW TRUE
)

set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(uWebSockets uSockets)

# Build uSockets as static library (no SSL, no libuv — raw kqueue/epoll)
add_library(uSockets_static STATIC
  ${usockets_SOURCE_DIR}/src/bsd.c
  ${usockets_SOURCE_DIR}/src/context.c
  ${usockets_SOURCE_DIR}/src/loop.c
  ${usockets_SOURCE_DIR}/src/socket.c
  ${usockets_SOURCE_DIR}/src/eventing/epoll_kqueue.c
)
target_include_directories(uSockets_static PUBLIC ${usockets_SOURCE_DIR}/src)
target_compile_definitions(uSockets_static PUBLIC LIBUS_NO_SSL)
target_compile_options(uSockets_static PRIVATE -O3 -w)

# uWebSockets header-only interface
add_library(uWebSockets_headers INTERFACE)
target_include_directories(uWebSockets_headers INTERFACE ${uwebsockets_SOURCE_DIR}/src)
target_link_libraries(uWebSockets_headers INTERFACE uSockets_static)
target_compile_definitions(uWebSockets_headers INTERFACE UWS_NO_ZLIB)

# Benchmark executable
add_executable(ws_benchmark ws_benchmark.cpp)
target_link_libraries(ws_benchmark PRIVATE
  glaze
  bencher::bencher
  uWebSockets_headers
)
if(TARGET Boost::headers)
    target_link_libraries(ws_benchmark PRIVATE Boost::headers)
elseif(TARGET Boost::boost)
    target_link_libraries(ws_benchmark PRIVATE Boost::boost)
endif()
target_compile_options(ws_benchmark PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
  $<$<CXX_COMPILER_ID:MSVC>:/O2>
)
