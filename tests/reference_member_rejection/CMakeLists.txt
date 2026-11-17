# Compile-failure tests: pure reflection cannot name a reference member, so a type with one must be rejected rather
# than written as an empty object. See streaming_view_rejection for how these targets are driven: each case is its own
# EXCLUDE_FROM_ALL target that ctest builds, passing only when the guard's own message appears.
foreach(member IN ITEMS LVALUE CONST_LVALUE)
    string(TOLOWER "${member}" member_lower)
    set(target "reference_member_rejects_${member_lower}")

    add_executable(${target} EXCLUDE_FROM_ALL rejection_cases.cpp)
    target_link_libraries(${target} PRIVATE glaze::glaze)
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_compile_definitions(${target} PRIVATE GLZ_REJECT_${member})

    add_test(
        NAME ${target}
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target ${target} --config $<CONFIG>
    )
    set_tests_properties(${target} PROPERTIES
        PASS_REGULAR_EXPRESSION "pure reflection cannot name a reference member"
        RESOURCE_LOCK glz_build_tree)
endforeach()

# MSVC cannot tell an lvalue reference to const from an array member here, so that case stays a less clear error
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set_tests_properties(reference_member_rejects_const_lvalue PROPERTIES DISABLED TRUE)
endif()

# Raw arrays too long to count are rejected as well, rather than left to exhaust the compiler
add_executable(reflection_rejects_huge_array EXCLUDE_FROM_ALL rejection_cases.cpp)
target_link_libraries(reflection_rejects_huge_array PRIVATE glaze::glaze)
target_compile_features(reflection_rejects_huge_array PRIVATE cxx_std_23)
target_compile_definitions(reflection_rejects_huge_array PRIVATE GLZ_REJECT_HUGE_ARRAY)

add_test(
    NAME reflection_rejects_huge_array
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target reflection_rejects_huge_array --config $<CONFIG>
)
set_tests_properties(reflection_rejects_huge_array PROPERTIES
    PASS_REGULAR_EXPRESSION "cannot count the members of this aggregate"
    RESOURCE_LOCK glz_build_tree)
