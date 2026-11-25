# Compile-failure tests: a streaming read that would fill a non-owning view must not compile.
#
# The guard is a static_assert inside the readers that alias the input buffer, so it cannot be
# checked with a static_assert from inside a normal test binary -- the binary would simply fail to
# build. Each case below is its own EXCLUDE_FROM_ALL target that ctest builds and expects to fail.
# The positive direction (types that must keep streaming) is covered in istream_buffer_test.

set(glz_rejection_cases
    DIRECT_VIEW
    BEHIND_TUPLE
    TEXT_VIEW
    TAGGED_VARIANT_VIEW
    PAST_ANY_DEPTH
    CUSTOM_SETTER
    NDJSON
    STREAM_READER
    BEVE_CONST_SPAN
)

foreach(case IN LISTS glz_rejection_cases)
    string(TOLOWER "${case}" case_lower)
    set(target "streaming_rejects_${case_lower}")

    add_executable(${target} EXCLUDE_FROM_ALL rejection_cases.cpp)
    target_link_libraries(${target} PRIVATE glaze::glaze)
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_compile_definitions(${target} PRIVATE GLZ_REJECT_CASE_${case})

    add_test(
        NAME ${target}
        COMMAND "${CMAKE_COMMAND}" --build "${PROJECT_BINARY_DIR}" --target ${target} --config $<CONFIG>
    )
    # The build must fail, and it must fail *for the right reason*: pass only when the guard's own
    # message appears in the compiler output. A plain WILL_FAIL would score any build failure -- a
    # typo, a renamed target, a stale build tree -- as a pass. PASS_REGULAR_EXPRESSION ignores the
    # exit code, so a build that succeeds emits no such message and fails the test, which is exactly
    # the direction that matters if the guard ever stops firing. Match only the assert's own prose;
    # the surrounding boilerplate is spelled differently by GCC, Clang, and MSVC.
    #
    # RESOURCE_LOCK keeps these from driving the generator concurrently. Under `ctest -j` they are
    # the only tests that write to the build tree, and if the build system is stale each one starts
    # its own regeneration of the same tree -- which races on shared state and can damage what the
    # other tests link against, not just this one.
    set_tests_properties(${target} PROPERTIES
        PASS_REGULAR_EXPRESSION "fills a non-owning view"
        RESOURCE_LOCK glz_build_tree)
endforeach()
