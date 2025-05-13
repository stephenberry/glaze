project(inplace_vector_test)

add_executable(${PROJECT_NAME} inplace_vector_test.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

if (MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wa,-mbig-obj")
endif ()

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)
