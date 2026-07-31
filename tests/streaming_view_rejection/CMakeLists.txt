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
    # The compile is required to fail; if it ever succeeds the guard has stopped firing.
    set_tests_properties(${target} PROPERTIES WILL_FAIL TRUE)
endforeach()
