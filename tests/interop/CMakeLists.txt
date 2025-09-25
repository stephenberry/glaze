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
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Plugin functionality test
project(interop_plugins_test)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)
add_dependencies(${PROJECT_NAME} simple_plugin)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Cross-library structures test
project(interop_structures_test)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glaze_interop glaze_interop_client glz_test_common)
add_dependencies(${PROJECT_NAME} complex_structures_plugin)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Pure C FFI test - demonstrates C interface works without C++ templates
project(test_c_ffi)
add_executable(${PROJECT_NAME} test_c_ffi.c)
set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 11)
target_link_libraries(${PROJECT_NAME} PRIVATE glaze_interop)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Comprehensive optional types test
project(interop_optional_test)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE glaze_interop glaze_interop_client glaze::glaze ut::ut)
# Set compile options directly without inheriting from glz_test_common
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-rtti -Wall -Wextra -pedantic -Wno-missing-braces)
endif()
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Comprehensive variant types test
project(interop_variant_test)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE glaze_interop glaze_interop_client glaze::glaze ut::ut)
# Set compile options directly without inheriting from glz_test_common
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-rtti -Wall -Wextra -pedantic -Wno-missing-braces)
endif()
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Variant with struct types test
project(interop_variant_struct_test)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE glaze_interop glaze_interop_client glaze::glaze ut::ut)
# Set compile options directly without inheriting from glz_test_common
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-rtti -Wall -Wextra -pedantic -Wno-missing-braces)
endif()
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Complex types interop test
project(interop_complex_test)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE glaze_interop glaze_interop_client glaze::glaze ut::ut)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-rtti -Wall -Wextra -pedantic -Wno-missing-braces)
endif()
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Member function interop test
project(interop_member_function_test)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE glaze_interop glaze_interop_client glaze::glaze ut::ut)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE  -fno-rtti -Wall -Wextra -pedantic -Wno-missing-braces)
endif()
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Zero copy interop test
project(interop_zero_copy_test)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE glaze_interop glaze_interop_client glaze::glaze ut::ut)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-rtti -Wall -Wextra -pedantic -Wno-missing-braces)
endif()
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Core interop functionality test
project(test_interop_core)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE glaze_interop glaze_interop_client glaze::glaze ut::ut)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-rtti -Wall -Wextra -pedantic -Wno-missing-braces)
endif()
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})