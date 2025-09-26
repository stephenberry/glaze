project(enum_reflection)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Variant-enum comprehensive tests
add_executable(variant_enum_test variant_enum_test.cpp)
target_link_libraries(variant_enum_test PRIVATE glz_test_common)
add_test(NAME variant_enum_test COMMAND variant_enum_test)