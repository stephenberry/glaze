project(toml_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

if (MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wa,-mbig-obj")
endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    GLZ_TEST_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)

## glz::custom for TOML - issue #2786
## A separate translation unit that includes only "glaze/toml.hpp", so that the TOML headers are
## required to be self-sufficient for custom read/write.

add_executable(toml_custom_test custom_test.cpp)

target_link_libraries(toml_custom_test PRIVATE glz_test_common)

add_test(NAME toml_custom_test COMMAND toml_custom_test)

target_code_coverage(toml_custom_test AUTO ALL)
