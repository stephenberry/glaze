project(csv_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

if (MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wa,-mbig-obj")
endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLZ_TEST_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)

# Bounded buffer tests deliberately use undersized buffers to verify overflow
# guards. GCC's optimizer emits false-positive -Wstringop-overflow warnings
# because it cannot prove the ensure_space() early-return is always taken.
add_executable(csv_bounded_buffer_test csv_bounded_buffer_test.cpp)

target_link_libraries(csv_bounded_buffer_test PRIVATE glz_test_common)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(csv_bounded_buffer_test PRIVATE "-Wno-stringop-overflow" "-Wno-array-bounds")
endif ()

add_test(NAME csv_bounded_buffer_test COMMAND csv_bounded_buffer_test)

target_code_coverage(csv_bounded_buffer_test AUTO ALL)
