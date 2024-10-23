project(roundtrip)

function(add_roundtrip_executable FORMAT)
    set(EXE_NAME "${PROJECT_NAME}_${FORMAT}")
    add_executable(${EXE_NAME} ${PROJECT_NAME}.cpp)
    target_compile_definitions(${EXE_NAME} PRIVATE GLZ_TEST_FORMAT=glz::${FORMAT})
    target_link_libraries(${EXE_NAME} PRIVATE glz_test_common)
    add_test(NAME ${EXE_NAME} COMMAND ${EXE_NAME})
    set_tests_properties(${EXE_NAME} PROPERTIES LABELS "format_${FORMAT}")
endfunction()

set(SUPPORTED_FORMATS JSON BEVE)

foreach(FORMAT IN LISTS SUPPORTED_FORMATS)
    add_roundtrip_executable(${FORMAT})
endforeach()
