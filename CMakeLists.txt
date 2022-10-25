cmake_minimum_required(VERSION 3.15)

if (CPMAddPackage)
   include(use_cpm.cmake)
endif()

include(cmake/prelude.cmake)

project(
    glaze
    VERSION 0.0.8
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

add_library(glaze_glaze INTERFACE)
add_library(glaze::glaze ALIAS glaze_glaze)

set_property(TARGET glaze_glaze PROPERTY EXPORT_NAME glaze)

target_compile_features(glaze_glaze INTERFACE cxx_std_20)
target_include_directories(
    glaze_glaze ${warning_guard}
    INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

find_package(fmt 9 REQUIRED)
find_package(nanorange REQUIRED)
find_package(frozen 1.1.1 REQUIRED)
find_package(FastFloat 3.4 REQUIRED)

target_link_libraries(
    glaze_glaze INTERFACE
    fmt::fmt-header-only
    nanorange::nanorange
    frozen::frozen
    FastFloat::fast_float
)

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

if (PROJECT_IS_TOP_LEVEL)
  include(cmake/dev-mode.cmake)
endif()