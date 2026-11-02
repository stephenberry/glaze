project(bson_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)

# Compile-failure test: a member BSON cannot encode as an element (here glz::cast) must not compile.
# See streaming_view_rejection for how these targets are driven: the case is its own EXCLUDE_FROM_ALL
# target that ctest builds, passing only when the guard's own message appears.
add_executable(bson_rejects_cast_member EXCLUDE_FROM_ALL cast_member_rejection.cpp)
target_link_libraries(bson_rejects_cast_member PRIVATE glaze::glaze)
target_compile_features(bson_rejects_cast_member PRIVATE cxx_std_23)

add_test(
    NAME bson_rejects_cast_member
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target bson_rejects_cast_member --config $<CONFIG>
)
set_tests_properties(bson_rejects_cast_member PROPERTIES
    PASS_REGULAR_EXPRESSION "BSON can only encode a document"
    RESOURCE_LOCK glz_build_tree)
