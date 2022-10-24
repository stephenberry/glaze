cmake_minimum_required(VERSION 3.15)

include(cmake/prelude.cmake)

project(
    glaze
    VERSION 0.0.7
    LANGUAGES CXX
)

option(glaze_USE_LOCAL_PACKAGES "Use CPM_USE_LOCAL_PACKAGES" ON) # changing default to ON to integrate better with Conan and find_package
set(CPM_USE_LOCAL_PACKAGE glaze_USE_LOCAL_PACKAGES CACHE BOOL "" FORCE)

include(cmake/CPM.cmake)

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

#find_package(fmt 9 REQUIRED)
CPMAddPackage(
   NAME fmt
   OPTIONS FMT_INSTALL
   GIT_REPOSITORY https://github.com/fmtlib/fmt
   GIT_TAG 9.0.0
)

#find_package(nanorange REQUIRED)
CPMAddPackage(
   NAME nanorange
   OPTIONS USE_SINGLE_HEADER "BUILD_TESTING OFF" DO_INSTALL
   GIT_REPOSITORY https://github.com/tcbrindle/NanoRange
   GIT_TAG bf32251d65673fe170d602777c087786c529ead8
)

if (nanorange_ADDED)
  add_library(nanorange::nanorange ALIAS nanorange)
endif()

#find_package(frozen 1.1.1 REQUIRED)
CPMAddPackage(
   NAME frozen
   GIT_REPOSITORY https://github.com/serge-sans-paille/frozen.git
   GIT_TAG 1.1.1
)

#find_package(FastFloat 3.4 REQUIRED)
CPMAddPackage(
   NAME FastFloat
   GIT_REPOSITORY https://github.com/fastfloat/fast_float.git
   GIT_TAG b4d7cceb9322972ab1c53095d195d096b2e335d0 # fixes cmake alias
)

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