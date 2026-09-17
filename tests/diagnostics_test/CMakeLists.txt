project(diagnostics_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)

# The member diagnostics only exist at compile time, so the way to test them is to compile a program
# that has to be rejected and read the message the compiler produced. Each case is one translation
# unit; its own header comment carries the fragments its output must contain, in `// EXPECT:` for
# every compiler and in `// TRACE:` for the parts of an instantiation trace that identify the member
# (GCC renders the structural template arguments of the trace, which is what makes it readable).
file(GLOB diagnostic_cases CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cases/*.cpp")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(diagnostic_gnu_trace 1)
else()
    set(diagnostic_gnu_trace 0)
endif()

foreach(case_file IN LISTS diagnostic_cases)
    get_filename_component(case_name "${case_file}" NAME_WE)
    add_test(NAME ${PROJECT_NAME}_${case_name}
             COMMAND ${CMAKE_COMMAND}
                     -DCXX=${CMAKE_CXX_COMPILER}
                     -DSOURCE=${case_file}
                     -DINCLUDE_DIR=${glaze_SOURCE_DIR}/include
                     -DSTD=c++23
                     -DGNU_TRACE=${diagnostic_gnu_trace}
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/check_diagnostic.cmake)
endforeach()
