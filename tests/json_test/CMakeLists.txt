project(json_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

if (MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wa,-mbig-obj")
endif ()

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)

add_executable(jsonschema_test jsonschema_test.cpp)

target_link_libraries(jsonschema_test PRIVATE glz_test_common)

add_test(NAME jsonschema_test COMMAND jsonschema_test)

target_code_coverage(jsonschema_test AUTO ALL)

