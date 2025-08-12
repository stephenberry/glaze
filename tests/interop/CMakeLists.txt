# Build plugin libraries needed for tests
add_library(simple_plugin SHARED test_plugin_minimal.cpp)
target_compile_features(simple_plugin PRIVATE cxx_std_17)
set_target_properties(simple_plugin PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET default
    VISIBILITY_INLINES_HIDDEN OFF
)

add_library(complex_structures_plugin SHARED test_plugin_complex_structures.cpp)
target_compile_features(complex_structures_plugin PRIVATE cxx_std_23)
set_target_properties(complex_structures_plugin PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET default
    VISIBILITY_INLINES_HIDDEN OFF
)

# Basic interop functionality test
project(interop_basic_test)
add_executable(${PROJECT_NAME} test_interop_basic.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Plugin functionality test
project(interop_plugins_test)
add_executable(${PROJECT_NAME} test_interop_plugins.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)
add_dependencies(${PROJECT_NAME} simple_plugin)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Cross-library structures test
project(interop_structures_test)
add_executable(${PROJECT_NAME} test_interop_structures.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glaze_interop glaze_interop_client glz_test_common)
add_dependencies(${PROJECT_NAME} complex_structures_plugin)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})