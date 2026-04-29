cmake_minimum_required(VERSION 3.20)
project(mingw_openssl_heap_repro CXX)

set(CMAKE_CXX_STANDARD 17)

# Fetch standalone ASIO
include(FetchContent)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-36-0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(asio)

find_package(OpenSSL REQUIRED)

add_executable(repro repro.cpp)
target_include_directories(repro PRIVATE ${asio_SOURCE_DIR}/asio/include)
target_link_libraries(repro PRIVATE OpenSSL::SSL OpenSSL::Crypto)

if(WIN32)
    target_link_libraries(repro PRIVATE ws2_32 mswsock)
endif()

enable_testing()
add_test(NAME repro COMMAND repro)
