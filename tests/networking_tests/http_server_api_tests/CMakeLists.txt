project(http_server_api_tests)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})