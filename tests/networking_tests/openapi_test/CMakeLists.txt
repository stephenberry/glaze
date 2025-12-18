project(openapi_test)

add_executable(openapi_test openapi_test.cpp)
target_link_libraries(openapi_test PRIVATE glz_test_exceptions)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
