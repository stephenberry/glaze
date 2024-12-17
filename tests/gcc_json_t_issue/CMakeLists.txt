project(gcc_json_t_issue)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp unused.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)
