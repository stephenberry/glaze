project(yaml_conformance)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
add_executable(${PROJECT_NAME}_struct ${PROJECT_NAME}_struct.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)
target_link_libraries(${PROJECT_NAME}_struct PRIVATE glz_test_common)

if (MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wa,-mbig-obj")
    target_compile_options(${PROJECT_NAME}_struct PRIVATE "-Wa,-mbig-obj")
endif ()

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
add_test(NAME ${PROJECT_NAME}_struct COMMAND ${PROJECT_NAME}_struct)

target_code_coverage(${PROJECT_NAME} AUTO ALL)
target_code_coverage(${PROJECT_NAME}_struct AUTO ALL)
