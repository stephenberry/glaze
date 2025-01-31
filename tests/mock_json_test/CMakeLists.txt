project(mock_json_test)

add_compile_definitions(GLZ_TEST_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)
