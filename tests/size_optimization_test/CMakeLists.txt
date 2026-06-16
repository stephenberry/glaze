project(size_optimization_test)

# Compile-time wiring checks for the default optimization level seam, built in BOTH modes:
# the normal default, and the size default (GLZ_DEFAULT_OPTIMIZATION_SIZE). The static_asserts
# in the source assert the correct behavior for whichever mode the target is compiled in.
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)
target_code_coverage(${PROJECT_NAME} AUTO ALL)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

add_executable(size_optimization_size_default ${PROJECT_NAME}.cpp)
target_link_libraries(size_optimization_size_default PRIVATE glz_test_common)
target_compile_definitions(size_optimization_size_default PRIVATE GLZ_DEFAULT_OPTIMIZATION_SIZE)
add_test(NAME size_optimization_size_default COMMAND size_optimization_size_default)

# Symbol-absence guarantee: a pure size-default binary must not link the 40 KB integer table.
# This needs a symbol reader, so it is gated to non-MSVC toolchains and skips gracefully when
# no nm-like tool is available. The target forces -Os + section GC so the guarantee is checked
# under embedded-like conditions regardless of CMAKE_BUILD_TYPE, and it links only glaze::glaze
# (no test framework / sanitizers) to keep the symbol table representative.
if(NOT MSVC)
  find_program(GLZ_NM_TOOL NAMES llvm-nm nm)
  if(GLZ_NM_TOOL)
    add_executable(size_optimization_symbols symbols_main.cpp)
    target_link_libraries(size_optimization_symbols PRIVATE glaze::glaze)
    target_compile_definitions(size_optimization_symbols PRIVATE GLZ_DEFAULT_OPTIMIZATION_SIZE)
    target_compile_options(size_optimization_symbols PRIVATE -Os -ffunction-sections -fdata-sections)
    if(APPLE)
      target_link_options(size_optimization_symbols PRIVATE -Wl,-dead_strip)
    else()
      target_link_options(size_optimization_symbols PRIVATE -Wl,--gc-sections)
    endif()
    add_test(
      NAME size_optimization_symbols
      COMMAND "${CMAKE_COMMAND}"
        "-DNM=${GLZ_NM_TOOL}"
        "-DBIN=$<TARGET_FILE:size_optimization_symbols>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_symbols.cmake"
    )
  else()
    message(STATUS "size_optimization_test: no nm-like tool found; skipping symbol-absence test")
  endif()
endif()
