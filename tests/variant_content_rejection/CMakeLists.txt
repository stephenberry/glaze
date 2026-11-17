# Compile-failure tests: a glz::meta `content` array that cannot name every alternative's key must
# not compile. See streaming_view_rejection for how these targets are driven: each case is its own
# EXCLUDE_FROM_ALL target that ctest builds, passing only when the guard's own message appears.
function(glz_add_content_rejection case message)
    string(TOLOWER "${case}" case_lower)
    set(target "variant_content_rejects_${case_lower}")

    add_executable(${target} EXCLUDE_FROM_ALL rejection_cases.cpp)
    target_link_libraries(${target} PRIVATE glaze::glaze)
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_compile_definitions(${target} PRIVATE GLZ_REJECT_CASE_${case})

    add_test(
        NAME ${target}
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target ${target} --config $<CONFIG>
    )
    set_tests_properties(${target} PROPERTIES
        PASS_REGULAR_EXPRESSION "${message}"
        RESOURCE_LOCK glz_build_tree)
endfunction()

glz_add_content_rejection(TOO_FEW "must have one entry per")
glz_add_content_rejection(TOO_MANY "must have one entry per")
glz_add_content_rejection(DEFAULT_ALTERNATIVE "needs an id for every alternative")
glz_add_content_rejection(EMPTY_KEY "must be a non-empty key")
glz_add_content_rejection(KEY_IS_TAG "must differ")
glz_add_content_rejection(NOT_STRINGS "must hold strings")
glz_add_content_rejection(WITHOUT_TAG "means nothing on")
