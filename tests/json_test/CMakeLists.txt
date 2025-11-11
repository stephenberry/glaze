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
