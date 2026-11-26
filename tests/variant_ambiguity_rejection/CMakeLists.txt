# Compile-failure tests: reading JSON into a variant whose alternatives nothing can tell apart must
# not compile. See streaming_view_rejection for how these targets are driven: each case is its own
# EXCLUDE_FROM_ALL target that ctest builds, passing only when the guard's own message appears. What
# must keep compiling, including writing the same variants, is covered in json_variant_support_test.
foreach(case IN ITEMS
    IDENTICAL
    REORDERED
    RENAMED_BY_META
    DUPLICATE_TYPE
    THROUGH_POINTER
    IDS_WITHOUT_TAG
    AMONG_OTHER_ALTERNATIVES
    BESIDE_A_MAP
    MEMBER_OF_STRUCT
)
    string(TOLOWER "${case}" case_lower)
    set(target "variant_ambiguity_rejects_${case_lower}")

    add_executable(${target} EXCLUDE_FROM_ALL rejection_cases.cpp)
    target_link_libraries(${target} PRIVATE glaze::glaze)
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_compile_definitions(${target} PRIVATE GLZ_REJECT_CASE_${case})

    add_test(
        NAME ${target}
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target ${target} --config $<CONFIG>
    )
    set_tests_properties(${target} PROPERTIES
        PASS_REGULAR_EXPRESSION "the same keys holding the same types"
        RESOURCE_LOCK glz_build_tree)
endforeach()
