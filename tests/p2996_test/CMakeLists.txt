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

# Compile-failure cases: the reflection refuses a base whose second subobject it cannot name, and a
# keyed reader refuses a type whose keys cannot be told apart. Both guards are `static_assert`s in the
# headers, so a case cannot be a normal test binary -- it would not build. Each case is its own
# EXCLUDE_FROM_ALL target that ctest builds and expects to fail, and it passes only when the guard's own
# prose appears in the compiler output. A plain WILL_FAIL would score any build failure -- a typo, a
# renamed target, a stale tree -- as a pass; PASS_REGULAR_EXPRESSION ignores the exit code, so a guard
# that stops firing emits no such message and fails the test, which is the direction that matters.
#
# RESOURCE_LOCK keeps these from driving the generator concurrently: they are the only targets here
# that write to the build tree, and under `ctest -j` each one would otherwise start its own
# regeneration of the same tree.
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

    add_test(
        NAME ${target}
        COMMAND "${CMAKE_COMMAND}" --build "${PROJECT_BINARY_DIR}" --target ${target} --config $<CONFIG>
    )
    set_tests_properties(${target} PROPERTIES
        PASS_REGULAR_EXPRESSION "${guard_prose}"
        RESOURCE_LOCK glz_build_tree
    )
endfunction()

glz_add_rejection_case(REPEATED_BASE "glz::meta is needed for this type")
glz_add_rejection_case(KEYED_LOOKUP "glz::meta whose value names the members apart")
