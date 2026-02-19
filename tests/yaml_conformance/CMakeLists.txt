project(yaml_conformance)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
add_executable(${PROJECT_NAME}_struct ${PROJECT_NAME}_struct.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)
target_link_libraries(${PROJECT_NAME}_struct PRIVATE glz_test_common)

if (MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wa,-mbig-obj")
    target_compile_options(${PROJECT_NAME}_struct PRIVATE "-Wa,-mbig-obj")
endif ()

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
add_test(NAME ${PROJECT_NAME}_struct COMMAND ${PROJECT_NAME}_struct)

target_code_coverage(${PROJECT_NAME} AUTO ALL)
target_code_coverage(${PROJECT_NAME}_struct AUTO ALL)

# Data-driven test that iterates over the yaml-test-suite case directories.
include(FetchContent)
FetchContent_Declare(
    yaml_test_suite
    GIT_REPOSITORY https://github.com/yaml/yaml-test-suite.git
    GIT_TAG 6ad3d2c62885d82fc349026c136ef560838fdf3d # data branch, pinned 2026-02-18
)
FetchContent_MakeAvailable(yaml_test_suite)

add_executable(${PROJECT_NAME}_data ${PROJECT_NAME}_data.cpp)
target_link_libraries(${PROJECT_NAME}_data PRIVATE glz_test_common)
target_compile_definitions(${PROJECT_NAME}_data PRIVATE
    YAML_TEST_SUITE_DIR="${yaml_test_suite_SOURCE_DIR}"
)
if (MINGW)
    target_compile_options(${PROJECT_NAME}_data PRIVATE "-Wa,-mbig-obj")
endif ()
add_test(NAME ${PROJECT_NAME}_data COMMAND ${PROJECT_NAME}_data)
target_code_coverage(${PROJECT_NAME}_data AUTO ALL)
