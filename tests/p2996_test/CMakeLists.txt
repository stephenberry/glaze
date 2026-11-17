project(p2996_test)

# P2996 reflection test - requires C++26 with P2996 support
# Only built when glaze_ENABLE_REFLECTION26 is ON

add_executable(${PROJECT_NAME} p2996_test.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

# P2996 requires specific compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE
    -freflection
)

# -fexpansion-statements is Bloomberg Clang-specific, not supported by GCC
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -fexpansion-statements
    )
endif()

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)

# Compile-failure cases for the static_asserts in the headers. Each case is an EXCLUDE_FROM_ALL target
# that ctest builds; it passes only when the guard's message appears in the output, so an unrelated
# build failure does not count as a pass. RESOURCE_LOCK serializes the builds of the shared tree.
function(glz_add_rejection_case case_name guard_prose)
    string(TOLOWER "${case_name}" case_lower)
    set(target "p2996_rejects_${case_lower}")

    add_executable(${target} EXCLUDE_FROM_ALL rejection_cases.cpp)
    target_link_libraries(${target} PRIVATE glz_test_common)
    target_compile_options(${target} PRIVATE -freflection)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${target} PRIVATE -fexpansion-statements)
    endif()
    target_compile_definitions(${target} PRIVATE GLZ_P2996_REJECT_${case_name})

    # CMAKE_BINARY_DIR: PROJECT_BINARY_DIR is this subdirectory, which has no build.ninja
    add_test(
        NAME ${target}
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target ${target} --config $<CONFIG>
    )
    set_tests_properties(${target} PROPERTIES
        PASS_REGULAR_EXPRESSION "${guard_prose}"
        RESOURCE_LOCK glz_build_tree
    )
endfunction()

glz_add_rejection_case(REPEATED_BASE "inherited more than once non-virtually")
glz_add_rejection_case(DUPLICATE_NAME "two of its reflected members have the same name")
