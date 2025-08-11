# Interop library for language bindings
add_library(glaze_interop SHARED
    interop.cpp
)

target_include_directories(glaze_interop PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(glaze_interop PUBLIC cxx_std_23)
target_compile_definitions(glaze_interop PRIVATE GLZ_EXPORTS)
target_link_libraries(glaze_interop PUBLIC glaze::glaze)

set_target_properties(glaze_interop PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# C++ client library for consuming interop interface
add_library(glaze_interop_client STATIC
    client.cpp
    glaze.cpp
)

target_include_directories(glaze_interop_client PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(glaze_interop_client PUBLIC cxx_std_23)

# Platform-specific linking for dynamic loading
if(UNIX AND NOT APPLE)
    target_link_libraries(glaze_interop_client PUBLIC dl)
endif()

# Installation
install(TARGETS glaze_interop glaze_interop_client
    EXPORT glaze-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES 
    ${PROJECT_SOURCE_DIR}/include/glaze/interop/interop.hpp
    ${PROJECT_SOURCE_DIR}/include/glaze/interop/client.hpp
    ${PROJECT_SOURCE_DIR}/include/glaze/interop/glaze.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/glaze/interop
)

# Examples removed - keeping only tests

# Build tests if requested
if(glaze_DEVELOPER_MODE)
    # Create custom test common for interop tests without warnings as errors
    add_library(glz_interop_test_common INTERFACE)
    target_compile_features(glz_interop_test_common INTERFACE cxx_std_23)
    target_link_libraries(glz_interop_test_common INTERFACE ut::ut glaze::glaze)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(glz_interop_test_common INTERFACE -fno-exceptions -fno-rtti)
        target_compile_options(glz_interop_test_common INTERFACE -Wall -Wextra -pedantic -Wno-missing-braces)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(glz_interop_test_common INTERFACE /GR- /bigobj)
        target_compile_options(glz_interop_test_common INTERFACE /W4 /wd4459 /wd4805)
    endif()

    # Build a minimal test plugin for dynamic loading tests (no dependencies)
    add_library(simple_plugin SHARED ${PROJECT_SOURCE_DIR}/tests/interop/test_plugin_minimal.cpp)
    target_compile_features(simple_plugin PRIVATE cxx_std_17)  # Minimal C++ requirements
    set_target_properties(simple_plugin PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN OFF
    )
    
    # Build complex structures plugin for cross-library testing
    add_library(complex_structures_plugin SHARED ${PROJECT_SOURCE_DIR}/tests/interop/test_plugin_complex_structures.cpp)
    target_compile_features(complex_structures_plugin PRIVATE cxx_std_23)  # C++23 for advanced features
    set_target_properties(complex_structures_plugin PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN OFF
    )

    # Consolidated Interop Tests (reduced from 17+ files to 3 files)
    
    # Basic interop functionality (C++ fundamentals without TypeDescriptorPool)
    add_executable(test_interop_basic ${PROJECT_SOURCE_DIR}/tests/interop/test_interop_basic.cpp)
    target_link_libraries(test_interop_basic PRIVATE glz_interop_test_common)
    add_test(NAME interop_basic_test COMMAND test_interop_basic)
    
    # Plugin functionality (combines simple_loading, dynamic_loading, plugin_comprehensive)
    add_executable(test_interop_plugins ${PROJECT_SOURCE_DIR}/tests/interop/test_interop_plugins.cpp)
    target_link_libraries(test_interop_plugins PRIVATE glz_interop_test_common)
    add_dependencies(test_interop_plugins simple_plugin)
    add_test(NAME interop_plugins_test COMMAND test_interop_plugins)
    
    # Cross-library structures (combines cross_library_structures, structure_validation, json_across_libraries, json_serialization)
    add_executable(test_interop_structures ${PROJECT_SOURCE_DIR}/tests/interop/test_interop_structures.cpp)
    target_link_libraries(test_interop_structures PRIVATE glaze_interop glaze_interop_client glz_interop_test_common)
    add_dependencies(test_interop_structures complex_structures_plugin)
    add_test(NAME interop_structures_test COMMAND test_interop_structures)
endif()