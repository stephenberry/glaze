project(json_reflection_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

if (MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wa,-mbig-obj")
endif ()

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)

# Separated to work around GCC compiler bug with large translation units
add_executable(front_hash_test front_hash_test.cpp)
target_link_libraries(front_hash_test PRIVATE glz_test_common)
add_test(NAME front_hash_test COMMAND front_hash_test)
target_code_coverage(front_hash_test AUTO ALL)
