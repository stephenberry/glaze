project(socket_test)

FetchContent_Declare(
    libfork
    GIT_REPOSITORY https://github.com/conorwilliams/libfork.git
    GIT_TAG v3.8.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(libfork)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common libfork::libfork)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)
