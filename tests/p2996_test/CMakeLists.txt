project(p2996_test)

# P2996 reflection tests - require C++26 with P2996 support
# Only built when glaze_ENABLE_REFLECTION26 is ON

foreach(test_name p2996_test optionalize_test)
    add_executable(${test_name} ${test_name}.cpp)

    target_link_libraries(${test_name} PRIVATE glz_test_common)

    # P2996 requires specific compiler flags
    target_compile_options(${test_name} PRIVATE
        -freflection
    )

    # -fexpansion-statements is Bloomberg Clang-specific, not supported by GCC
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${test_name} PRIVATE
            -fexpansion-statements
        )
    endif()

    add_test(NAME ${test_name} COMMAND ${test_name})

    target_code_coverage(${test_name} AUTO ALL)
endforeach()
