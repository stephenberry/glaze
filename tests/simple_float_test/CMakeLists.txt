project(simple_float_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)
target_code_coverage(${PROJECT_NAME} AUTO ALL)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
