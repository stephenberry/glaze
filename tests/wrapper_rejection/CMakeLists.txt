project(wrapper_rejection_test)

# Compile-time checks that wrappers are never reflected
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Compile-failure tests: writing a glz::invoke member in any format, or reading one in any format but
# JSON, must not compile. See streaming_view_rejection for how these targets are driven: each case is
# its own EXCLUDE_FROM_ALL target that ctest builds, passing only when the guard's own message appears.
function(glz_add_invoke_rejection direction format message)
    string(TOLOWER "${direction}_${format}" case_lower)
    set(target "invoke_rejects_${case_lower}")

    add_executable(${target} EXCLUDE_FROM_ALL rejection_cases.cpp)
    target_link_libraries(${target} PRIVATE glaze::glaze)
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_compile_definitions(${target} PRIVATE GLZ_REJECT_${direction} GLZ_REJECT_FORMAT=glz::${format})

    add_test(
        NAME ${target}
        COMMAND "${CMAKE_COMMAND}" --build "${PROJECT_BINARY_DIR}" --target ${target} --config $<CONFIG>
    )
    set_tests_properties(${target} PROPERTIES
        PASS_REGULAR_EXPRESSION "${message}"
        RESOURCE_LOCK glz_build_tree)
endfunction()

foreach(format IN ITEMS JSON BEVE CBOR MSGPACK TOML YAML JSONB BSON)
    glz_add_invoke_rejection(WRITE ${format} "glz::invoke members cannot be written")
endforeach()

foreach(format IN ITEMS BEVE CBOR MSGPACK TOML YAML JSONB BSON)
    glz_add_invoke_rejection(READ ${format} "glz::invoke members can only be read from JSON")
endforeach()
