project(json_test)

set(JSON_TEST_SOURCES json_test.cpp)
set(JSON_VARIANT_SUPPORT_SOURCES json_variant_support_test.cpp)

function(add_glz_json_test target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE glz_test_common)

    if (MINGW)
        target_compile_options(${target} PRIVATE "-Wa,-mbig-obj")
    endif ()

    add_test(NAME ${target} COMMAND ${target})

    target_code_coverage(${target} AUTO ALL)

    set(non_null_target ${target}_non_null)
    add_executable(${non_null_target} ${ARGN})
    target_compile_definitions(${non_null_target} PRIVATE GLZ_NULL_TERMINATED=false)
    target_link_libraries(${non_null_target} PRIVATE glz_test_common)

    if (MINGW)
        target_compile_options(${non_null_target} PRIVATE "-Wa,-mbig-obj")
    endif ()

    add_test(NAME ${non_null_target} COMMAND ${non_null_target})
endfunction()

add_glz_json_test(${PROJECT_NAME} ${JSON_TEST_SOURCES})
add_glz_json_test(json_variant_support_test ${JSON_VARIANT_SUPPORT_SOURCES})

## glz::generic test

add_executable(json_t_test generic_test.cpp)

target_link_libraries(json_t_test PRIVATE glz_test_common)

add_test(NAME json_t_test COMMAND json_t_test)

## glz::generic_i64 and glz::generic_u64 tests

add_executable(generic_int_test generic_int_test.cpp)

target_link_libraries(generic_int_test PRIVATE glz_test_common)

add_test(NAME generic_int_test COMMAND generic_int_test)

## JSON Schema test

add_executable(jsonschema_test jsonschema_test.cpp)

target_link_libraries(jsonschema_test PRIVATE glz_test_common)

add_test(NAME jsonschema_test COMMAND jsonschema_test)

## Variant ambiguous test - test for ambiguous variant handling

add_executable(variant_ambiguous_test variant_ambiguous_test.cpp)

target_link_libraries(variant_ambiguous_test PRIVATE glz_test_common)

add_test(NAME variant_ambiguous_test COMMAND variant_ambiguous_test)

## Value-based skip test - test for issue #1994

add_executable(value_based_skip_test value_based_skip_test.cpp)

target_link_libraries(value_based_skip_test PRIVATE glz_test_common)

add_test(NAME value_based_skip_test COMMAND value_based_skip_test)

## Nullable lambda test - tests that lambdas returning nullable types can be skipped

add_executable(nullable_lambda_test nullable_lambda_test.cpp)

target_link_libraries(nullable_lambda_test PRIVATE glz_test_common)

add_test(NAME nullable_lambda_test COMMAND nullable_lambda_test)

## Byte array test - test for issue #1959 and PR #1966

add_executable(byte_array_test byte_array_test.cpp)

target_link_libraries(byte_array_test PRIVATE glz_test_common)

add_test(NAME byte_array_test COMMAND byte_array_test)

## Indexed rename_key test - generic solution for issue #1117

add_executable(indexed_rename_key_test indexed_rename_key_test.cpp)

target_link_libraries(indexed_rename_key_test PRIVATE glz_test_common)

add_test(NAME indexed_rename_key_test COMMAND indexed_rename_key_test)

## Skip null on read test - test for issue #1047

add_executable(skip_null_on_read_test skip_null_on_read_test.cpp)

target_link_libraries(skip_null_on_read_test PRIVATE glz_test_common)

add_test(NAME skip_null_on_read_test COMMAND skip_null_on_read_test)

## u8string test - test for std::u8string support as both buffer and value types

add_executable(u8string_test u8string_test.cpp)

target_link_libraries(u8string_test PRIVATE glz_test_common)

add_test(NAME u8string_test COMMAND u8string_test)

## JSON Patch test - RFC 6902

add_executable(json_patch_test json_patch_test.cpp)

target_link_libraries(json_patch_test PRIVATE glz_test_common)

add_test(NAME json_patch_test COMMAND json_patch_test)

## JSON Merge Patch test - RFC 7386

add_executable(json_merge_patch_test json_merge_patch_test.cpp)

target_link_libraries(json_merge_patch_test PRIVATE glz_test_common)

add_test(NAME json_merge_patch_test COMMAND json_merge_patch_test)

## Linear search test - test for linear_search compile-time option

add_executable(linear_search_test linear_search_test.cpp)

target_link_libraries(linear_search_test PRIVATE glz_test_common)

add_test(NAME linear_search_test COMMAND linear_search_test)

## Allocate raw pointers test - test for allocate_raw_pointers compile-time option (issue #2192)

add_executable(allocate_raw_pointers_test allocate_raw_pointers_test.cpp)

target_link_libraries(allocate_raw_pointers_test PRIVATE glz_test_common)

add_test(NAME allocate_raw_pointers_test COMMAND allocate_raw_pointers_test)

## snprintf fallback test - tests float_format with forced snprintf fallback (for iOS < 16.3 compatibility)

add_executable(snprintf_fallback_test snprintf_fallback_test.cpp)

target_link_libraries(snprintf_fallback_test PRIVATE glz_test_common)

add_test(NAME snprintf_fallback_test COMMAND snprintf_fallback_test)

## glz::lazy test - lazy JSON type that stores string_views

add_executable(lazy_test lazy_test.cpp)

target_link_libraries(lazy_test PRIVATE glz_test_common)

add_test(NAME lazy_test COMMAND lazy_test)

## Non-aggregate reflection test - C++26 P2996 reflection for non-aggregate types

add_executable(non_aggregate_reflection_test non_aggregate_reflection_test.cpp)

target_link_libraries(non_aggregate_reflection_test PRIVATE glz_test_common)

add_test(NAME non_aggregate_reflection_test COMMAND non_aggregate_reflection_test)
