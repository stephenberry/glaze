project(size_optimization_test)

add_library(glz_size_modules STATIC)
target_sources(glz_size_modules PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${glaze_SOURCE_DIR}/modules"
  FILES ${glaze_library_modules})
target_include_directories(glz_size_modules PUBLIC "${glaze_SOURCE_DIR}/modules")
target_compile_features(glz_size_modules PUBLIC cxx_std_23)
target_compile_options(glz_size_modules PRIVATE "$<TARGET_PROPERTY:glaze_glaze,COMPILE_OPTIONS>")
target_compile_definitions(glz_size_modules PRIVATE GLZ_BUILDING_MODULES PUBLIC GLZ_DEFAULT_OPTIMIZATION_SIZE)
target_link_libraries(glz_size_modules PRIVATE glaze::asio)

# Compile-time wiring checks for the default optimization level seam, built in BOTH modes:
# the normal default, and the size default (GLZ_DEFAULT_OPTIMIZATION_SIZE). The static_asserts
# in the source assert the correct behavior for whichever mode the target is compiled in.
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)
target_code_coverage(${PROJECT_NAME} AUTO ALL)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

add_executable(size_optimization_size_default ${PROJECT_NAME}.cpp)
target_link_libraries(size_optimization_size_default PRIVATE glz_size_modules)
add_test(NAME size_optimization_size_default COMMAND size_optimization_size_default)

# Symbol-absence guarantee: a pure size-default binary must not link the 40 KB integer table.
# This needs a symbol reader, so it is gated to non-MSVC toolchains and skips gracefully when
# no nm-like tool is available. The target forces -Os + section GC so the guarantee is checked
# under embedded-like conditions regardless of CMAKE_BUILD_TYPE, and it links only the size-mode library
# (no test framework / sanitizers) to keep the symbol table representative.
if(NOT MSVC)
  find_program(GLZ_NM_TOOL NAMES llvm-nm nm)
  if(GLZ_NM_TOOL)
    add_executable(size_optimization_symbols symbols_main.cpp)
    target_link_libraries(size_optimization_symbols PRIVATE glz_size_modules)
    target_compile_options(size_optimization_symbols PRIVATE
      -Os
      -ffunction-sections
      -fdata-sections
    )
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
