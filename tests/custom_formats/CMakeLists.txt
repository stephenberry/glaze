project(custom_formats_test)

## glz::custom across every format that supports it - issue #2786.
## Each source includes exactly one format header, so the format headers are required to be
## self-sufficient for custom read/write rather than borrowing "glaze/core/custom.hpp" from
## whatever else the translation unit happens to include.

add_executable(${PROJECT_NAME} main.cpp json_custom.cpp beve_custom.cpp cbor_custom.cpp msgpack_custom.cpp bson_custom.cpp jsonb_custom.cpp toml_custom.cpp yaml_custom.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(glaze_EETF_FORMAT)
    target_sources(${PROJECT_NAME} PRIVATE eetf_custom.cpp)
endif(glaze_EETF_FORMAT)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)
