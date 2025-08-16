project(key_transformers_test)

add_executable(${PROJECT_NAME} key_transformers_test.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)