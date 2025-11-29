project(module_test)

# Module tests are only built when glaze_BUILD_MODULES is enabled
if(NOT glaze_BUILD_MODULES)
  return()
endif()

# Create module test executable
add_executable(json_module_test json_module_test.cpp)

# Enable module scanning for this target
set_target_properties(json_module_test PROPERTIES CXX_SCAN_FOR_MODULES ON)

# Link against the JSON module target
target_link_libraries(json_module_test PRIVATE glaze::json_module ut::ut)

target_compile_features(json_module_test PUBLIC cxx_std_23)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(json_module_test PRIVATE
        -Wall -Wextra -pedantic
        -Wno-undefined-var-template  # Glaze uses extern template vars for reflection
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(json_module_test PRIVATE /W4)
endif()

add_test(NAME json_module_test COMMAND json_module_test)
