project(diagnostics_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)

# The member diagnostics only exist at compile time, so the way to test them is to compile a program
# that has to be rejected and read the message the compiler produced. Each case is one translation
# unit with a target of its own, held out of the default build because it must never succeed, and the
# test builds it through the build system so that it inherits the flags this project is configured
# with. Compiling it here with a command line written by hand does not work: clang-linux configures
# with -stdlib=libc++, and MSVC takes /Zs rather than -fsyntax-only, so a case would be rejected for
# the wrong reason on both.
file(GLOB diagnostic_cases CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cases/*.cpp")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(diagnostic_gnu_trace 1)
else()
    set(diagnostic_gnu_trace 0)
endif()

foreach(case_file IN LISTS diagnostic_cases)
    get_filename_component(case_name "${case_file}" NAME_WE)
    set(case_target diagnostic_case_${case_name})

    add_executable(${case_target} EXCLUDE_FROM_ALL "${case_file}")
    target_link_libraries(${case_target} PRIVATE glz_test_common)

    add_test(NAME ${PROJECT_NAME}_${case_name}
             COMMAND ${CMAKE_COMMAND}
                     -DBUILD_DIR=${CMAKE_BINARY_DIR}
                     -DTARGET=${case_target}
                     -DCONFIG=$<CONFIG>
                     -DSOURCE=${case_file}
                     -DGNU_TRACE=${diagnostic_gnu_trace}
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/check_diagnostic.cmake)

    # Each of these drives the build system for its own target, so keep them from doing it at once.
    set_tests_properties(${PROJECT_NAME}_${case_name} PROPERTIES RESOURCE_LOCK diagnostic_cases)
endforeach()
