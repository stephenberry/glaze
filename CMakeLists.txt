cmake_minimum_required(VERSION 3.21)

include(cmake/prelude.cmake)

project(
    glaze
    VERSION 6.0.1
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

if (PROJECT_IS_TOP_LEVEL)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

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
  target_compile_options(${PROJECT_NAME}_${PROJECT_NAME} INTERFACE "-Wno-missing-braces")
endif()

set_property(TARGET ${PROJECT_NAME}_${PROJECT_NAME} PROPERTY EXPORT_NAME glaze)

target_compile_features(${PROJECT_NAME}_${PROJECT_NAME} INTERFACE cxx_std_23)
target_include_directories(
    ${PROJECT_NAME}_${PROJECT_NAME} ${warning_guard}
    INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

if (glaze_DEVELOPER_MODE)
  include(cmake/dev-mode.cmake)
endif()

option(glaze_DISABLE_SIMD_WHEN_SUPPORTED
       "disable SIMD optimizations even when targets support it (e.g. AVX2)" OFF)
if(glaze_DISABLE_SIMD_WHEN_SUPPORTED)
  target_compile_definitions(glaze_glaze INTERFACE GLZ_DISABLE_SIMD)
endif()

option(glaze_BUILD_EXAMPLES "Build GLAZE examples" OFF)

if(glaze_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

option(glaze_EETF_FORMAT "Enable Erlang external term format parsing" OFF)

if(glaze_EETF_FORMAT)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
  find_package(Erlang REQUIRED)
  target_link_libraries(
    glaze_glaze ${warning_guard}
    INTERFACE
      Erlang::EI
      Erlang::Erlang
  )
  target_compile_definitions(glaze_glaze INTERFACE GLZ_ENABLE_EETF)
endif(glaze_EETF_FORMAT)

option(glaze_ENABLE_SSL "Enable SSL/TLS support for HTTPS servers" OFF)

if(glaze_ENABLE_SSL)
  find_package(OpenSSL REQUIRED)
  target_link_libraries(
    glaze_glaze ${warning_guard}
    INTERFACE
      OpenSSL::SSL
      OpenSSL::Crypto
  )
  target_compile_definitions(glaze_glaze INTERFACE GLZ_ENABLE_SSL)
endif(glaze_ENABLE_SSL)

