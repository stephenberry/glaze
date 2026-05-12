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

# Standalone regression test for issue #2540 (map-typed struct fields must be
# rendered as TOML tables, not scalars).  Kept separate so it can run quickly
# in isolation -- intentionally does NOT link the heavier ``glz_test_common``
# fixture so it stays fast on every supported platform.
add_executable(toml_map_field_test toml_map_field_test.cpp)
target_link_libraries(toml_map_field_test PRIVATE glaze::glaze)
target_compile_features(toml_map_field_test PRIVATE cxx_std_23)
if (MINGW)
    target_compile_options(toml_map_field_test PRIVATE "-Wa,-mbig-obj")
endif ()
add_test(NAME toml_map_field_test COMMAND toml_map_field_test)
target_code_coverage(toml_map_field_test AUTO ALL)