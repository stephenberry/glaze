project(beve_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)

## Skip null on read test - test for issue #1047 (BEVE format)

add_executable(beve_skip_null_on_read_test skip_null_on_read_test.cpp)

target_link_libraries(beve_skip_null_on_read_test PRIVATE glz_test_common)

add_test(NAME beve_skip_null_on_read_test COMMAND beve_skip_null_on_read_test)
