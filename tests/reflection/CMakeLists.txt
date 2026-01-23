project(reflection)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)

## Enum hash test - tests for sparse enum hashing (issues #2246, #2262)

add_executable(enum_test enum_test.cpp)

target_link_libraries(enum_test PRIVATE glz_test_common)

add_test(NAME enum_test COMMAND enum_test)

target_code_coverage(enum_test AUTO ALL)
