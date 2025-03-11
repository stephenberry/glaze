project(asio_repe)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-32-0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(asio)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE include ${asio_SOURCE_DIR}/asio/include)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)