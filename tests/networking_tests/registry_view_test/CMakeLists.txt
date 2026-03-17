project(registry_view_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

if(glaze_BUILD_REGISTRY_NOEXCEPT_TESTS)
    add_executable(${PROJECT_NAME}_noexceptions ${PROJECT_NAME}.cpp)
    target_link_libraries(${PROJECT_NAME}_noexceptions PRIVATE glz_test_common)
    add_test(NAME ${PROJECT_NAME}_noexceptions COMMAND ${PROJECT_NAME}_noexceptions)
endif()
