project(p2996_test)

# P2996 reflection test - requires C++26 with P2996 support
# Only built when glaze_ENABLE_REFLECTION26 is ON

add_executable(${PROJECT_NAME} p2996_test.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

# P2996 requires specific compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE
    -freflection
    -fexpansion-statements
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)
