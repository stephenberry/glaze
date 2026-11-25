find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(module_root "${PROJECT_SOURCE_DIR}/modules")
set(header_root "${PROJECT_BINARY_DIR}/include")
set(header_generator "${PROJECT_SOURCE_DIR}/tools/glz_generate_headers.py")
file(GLOB_RECURSE header_inputs CONFIGURE_DEPENDS
    "${module_root}/*.ixx" "${module_root}/*.hpp" "${module_root}/*.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${header_generator}" ${header_inputs})

set(header_list "${PROJECT_BINARY_DIR}/glaze-headers.txt")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${header_generator}" --module-root "${module_root}"
        --include-root "${header_root}" --output-list "${header_list}"
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
)
file(STRINGS "${header_list}" generated_headers ENCODING UTF-8)
target_sources(glaze_glaze INTERFACE
    FILE_SET generated_headers TYPE HEADERS BASE_DIRS "${header_root}" FILES ${generated_headers})
