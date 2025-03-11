project(lib_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

add_subdirectory(test_lib)

add_dependencies(${PROJECT_NAME} test_lib)

target_compile_definitions(${PROJECT_NAME} PRIVATE GLZ_TEST_DIRECTORY="${CMAKE_SOURCE_DIR}/bin")

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)
