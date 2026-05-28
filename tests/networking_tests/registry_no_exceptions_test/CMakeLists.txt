project(registry_no_exceptions_test)

# Exceptions build: verifies the new APIs behave the same with exceptions enabled.
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# No-exceptions build: the core purpose of this test. glz_test_common compiles with
# -fno-exceptions -fno-rtti, so this would not even compile before issue #2265 was fixed.
if(glaze_BUILD_REGISTRY_NOEXCEPT_TESTS)
    add_executable(${PROJECT_NAME}_noexceptions ${PROJECT_NAME}.cpp)
    target_link_libraries(${PROJECT_NAME}_noexceptions PRIVATE glz_test_common)
    add_test(NAME ${PROJECT_NAME}_noexceptions COMMAND ${PROJECT_NAME}_noexceptions)
endif()
