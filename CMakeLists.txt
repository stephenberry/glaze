cmake_minimum_required(VERSION 3.18)

include(cmake/prelude.cmake)

project(
    glaze
    VERSION 0.0.9
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

add_library(glaze_glaze INTERFACE)
add_library(glaze::glaze ALIAS glaze_glaze)

if (MSVC)
   string(REGEX MATCH "\/cl(.exe)?$" matched_cl ${CMAKE_CXX_COMPILER})
   if (matched_cl)
      # for a C++ standards compliant preprocessor, not needed for clang-cl
      target_compile_options(glaze_glaze INTERFACE "/Zc:preprocessor")
   endif()
else()
   target_compile_options(glaze_glaze INTERFACE "-Wno-missing-braces")
endif()

set_property(TARGET glaze_glaze PROPERTY EXPORT_NAME glaze)

target_compile_features(glaze_glaze INTERFACE cxx_std_20)
target_include_directories(
    glaze_glaze ${warning_guard}
    INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

include(cmake/CPM.cmake)

CPMFindPackage(
   NAME fmt
   OPTIONS FMT_INSTALL
   GIT_REPOSITORY https://github.com/fmtlib/fmt
   GIT_TAG 9.1.0
)

CPMFindPackage(
   NAME nanorange
   OPTIONS USE_SINGLE_HEADER "BUILD_TESTING OFF" DO_INSTALL
   GIT_REPOSITORY https://github.com/tcbrindle/NanoRange
   GIT_TAG master # No version tags
)

if (nanorange_ADDED)
   add_library(nanorange::nanorange ALIAS nanorange)
endif()

CPMFindPackage(
   NAME frozen
   GIT_REPOSITORY https://github.com/serge-sans-paille/frozen.git
   GIT_TAG 1.1.1
)

CPMFindPackage(
   NAME FastFloat
   GIT_REPOSITORY https://github.com/fastfloat/fast_float.git
   GIT_TAG v3.6.0
)

CPMAddPackage(
   NAME dragonbox
   GIT_REPOSITORY https://github.com/jk-jeon/dragonbox
   GIT_TAG 1.1.3
)

target_link_libraries(
    glaze_glaze INTERFACE
    fmt::fmt-header-only
    dragonbox::dragonbox_to_chars
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
