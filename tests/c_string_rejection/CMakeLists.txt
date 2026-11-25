# Compile-failure tests: a binary format must not be read from a null-terminated C string, whose
# length strlen would cut short at the first zero byte. See streaming_view_rejection for how these
# targets are driven: each case is its own EXCLUDE_FROM_ALL target that ctest builds, passing only
# when the guard's own message appears.
foreach(format IN ITEMS BEVE CBOR MSGPACK JSONB BSON)
    string(TOLOWER "${format}" format_lower)
    set(target "c_string_rejects_${format_lower}")

    add_executable(${target} EXCLUDE_FROM_ALL rejection_cases.cpp)
    target_link_libraries(${target} PRIVATE glaze::glaze)
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_compile_definitions(${target} PRIVATE GLZ_REJECT_FORMAT=glz::${format})

    add_test(
        NAME ${target}
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target ${target} --config $<CONFIG>
    )
    set_tests_properties(${target} PROPERTIES
        PASS_REGULAR_EXPRESSION "cannot be read from a null-terminated C string"
        RESOURCE_LOCK glz_build_tree)
endforeach()
