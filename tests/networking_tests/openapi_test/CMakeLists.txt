project(openapi_test)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-32-0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(asio)

add_executable(openapi_test openapi_test.cpp)
target_include_directories(openapi_test PRIVATE include ${asio_SOURCE_DIR}/asio/include)
target_link_libraries(openapi_test PRIVATE glz_test_exceptions)
