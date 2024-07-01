cmake_minimum_required(VERSION 3.21)

include(cmake/prelude.cmake)

project(
    glaze
    VERSION 2.9.3
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
    target_compile_options(glaze_glaze INTERFACE "/Zc:preprocessor" /permissive- /Zc:lambda)
    
    if(PROJECT_IS_TOP_LEVEL)
      target_compile_options(glaze_glaze INTERFACE 
        $<$<CONFIG:Release>:/GL>
        $<$<CONFIG:MinSizeRel>:/GL>)
      target_link_options(glaze_glaze INTERFACE 
        $<$<CONFIG:Release>:/LTCG /INCREMENTAL:NO>
        $<$<CONFIG:MinSizeRel>:/LTCG /INCREMENTAL:NO>)
    endif()
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

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

if (glaze_DEVELOPER_MODE)
  include(cmake/dev-mode.cmake)
endif()
