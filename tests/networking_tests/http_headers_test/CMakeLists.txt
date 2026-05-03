project(http_headers_test)

add_executable(http_headers_test http_headers_test.cpp)
target_link_libraries(http_headers_test PRIVATE glz_test_exceptions)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
