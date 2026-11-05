project(custom_getter_rejection)

# Compile-failure tests: writing a glz::custom whose getter cannot be invoked must be rejected by the
# getter's own static_assert in every position a block writer looks through it. See
# streaming_view_rejection for how these targets are driven: each case is its own EXCLUDE_FROM_ALL
# target that ctest builds, passing only when the guard's own message appears.
function(glz_add_custom_getter_rejection format position)
    string(TOLOWER "${format}_${position}" case_lower)
    set(target "custom_getter_rejects_${case_lower}")

    add_executable(${target} EXCLUDE_FROM_ALL rejection_cases.cpp)
    target_link_libraries(${target} PRIVATE glaze::glaze)
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_compile_definitions(${target} PRIVATE GLZ_REJECT_FORMAT=glz::${format}
                                                 GLZ_REJECT_POSITION=${position}_holder)

    add_test(
        NAME ${target}
        # The top of the build tree: single-file generators such as Ninja have no build file in this
        # subdirectory, and project() above makes PROJECT_BINARY_DIR point here.
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target ${target} --config $<CONFIG>
    )
    set_tests_properties(${target} PROPERTIES
        PASS_REGULAR_EXPRESSION "expected invocable function"
        RESOURCE_LOCK glz_build_tree)
endfunction()

foreach(format IN ITEMS TOML YAML)
    foreach(position IN ITEMS member sequence map_value optional)
        glz_add_custom_getter_rejection(${format} ${position})
    endforeach()
endforeach()
