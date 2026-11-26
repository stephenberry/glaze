project(test_lib)

add_library(test_lib SHARED)
target_link_libraries(test_lib PRIVATE glaze::glaze)
set_target_properties(test_lib PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugin/$<CONFIG>"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugin/$<CONFIG>"
)

target_sources(test_lib
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    test_lib.ixx
)
