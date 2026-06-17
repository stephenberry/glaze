cmake_minimum_required(VERSION 3.21)

project(glaze_windows_header_compatibility LANGUAGES CXX)

set(GLAZE_SOURCE_DIR "" CACHE PATH "Glaze source directory")
set(ERLANG_OTP_DIR "" CACHE PATH "Erlang/OTP installation directory")

if(NOT GLAZE_SOURCE_DIR)
  message(FATAL_ERROR "GLAZE_SOURCE_DIR is required")
endif()

if(NOT ERLANG_OTP_DIR)
  message(FATAL_ERROR "ERLANG_OTP_DIR is required")
endif()

get_filename_component(GLAZE_SOURCE_DIR "${GLAZE_SOURCE_DIR}" ABSOLUTE)
get_filename_component(ERLANG_OTP_DIR "${ERLANG_OTP_DIR}" ABSOLUTE)

if(NOT EXISTS "${GLAZE_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "GLAZE_SOURCE_DIR must point to the Glaze source tree")
endif()

set(erlang_ei_candidate_include_dirs
  "${ERLANG_OTP_DIR}/usr/include"
  "${ERLANG_OTP_DIR}/lib/erl_interface/include"
)

foreach(candidate_include_dir IN LISTS erlang_ei_candidate_include_dirs)
  if(EXISTS "${candidate_include_dir}/ei.h")
    set(ERLANG_EI_INCLUDE_DIR "${candidate_include_dir}")
    break()
  endif()
endforeach()

if(NOT ERLANG_EI_INCLUDE_DIR)
  message(FATAL_ERROR "ei.h was not found under ${ERLANG_OTP_DIR}")
endif()

message(STATUS "Using Erlang ei include directory: ${ERLANG_EI_INCLUDE_DIR}")

add_subdirectory("${GLAZE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/glaze")

find_package(Eigen3 CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)

file(GLOB_RECURSE glaze_public_headers CONFIGURE_DEPENDS "${GLAZE_SOURCE_DIR}/include/glaze/*.hpp")
file(GLOB_RECURSE glaze_windows_lean_header_compatibility_headers CONFIGURE_DEPENDS
  "${GLAZE_SOURCE_DIR}/include/glaze/net/*.hpp"
  "${GLAZE_SOURCE_DIR}/include/glaze/rpc/*.hpp"
)
list(APPEND glaze_windows_lean_header_compatibility_headers
  "${GLAZE_SOURCE_DIR}/include/glaze/ext/glaze_asio.hpp"
  "${GLAZE_SOURCE_DIR}/include/glaze/file/hostname_include.hpp"
)
set(glaze_windows_header_compatibility_headers "${glaze_public_headers}")
list(REMOVE_ITEM glaze_windows_header_compatibility_headers ${glaze_windows_lean_header_compatibility_headers})
list(SORT glaze_public_headers)
list(SORT glaze_windows_header_compatibility_headers)
list(SORT glaze_windows_lean_header_compatibility_headers)
set(glaze_rpc_registry_header "${GLAZE_SOURCE_DIR}/include/glaze/rpc/registry.hpp")
list(REMOVE_ITEM glaze_windows_lean_header_compatibility_headers "${glaze_rpc_registry_header}")
list(PREPEND glaze_windows_lean_header_compatibility_headers "${glaze_rpc_registry_header}")
list(LENGTH glaze_public_headers glaze_public_header_count)
list(LENGTH glaze_windows_header_compatibility_headers glaze_windows_header_compatibility_header_count)
list(LENGTH glaze_windows_lean_header_compatibility_headers glaze_windows_lean_header_compatibility_header_count)

if(glaze_public_header_count EQUAL 0)
  message(FATAL_ERROR "No Glaze public headers were found")
endif()

message(STATUS "Found ${glaze_public_header_count} Glaze public headers")
message(STATUS "Using unsanitized Windows.h for ${glaze_windows_header_compatibility_header_count} Glaze public headers")
message(STATUS "Using WIN32_LEAN_AND_MEAN Windows.h for ${glaze_windows_lean_header_compatibility_header_count} Glaze public headers")

function(append_windows_macro_checks content_var error_context check_delete check_small)
  set(macro_names min max ERROR near far)

  if(check_delete)
    set(macro_names min max ERROR DELETE near far)
  endif()

  foreach(macro_name IN LISTS macro_names)
    string(APPEND ${content_var}
      "#ifndef ${macro_name}\n"
      "#error ${macro_name} macro ${error_context}\n"
      "#endif\n"
    )
  endforeach()

  if(check_small)
    string(APPEND ${content_var}
      "#ifdef GLAZE_WINDOWS_HEADER_HAS_SMALL\n"
      "#ifndef small\n"
      "#error small macro ${error_context}\n"
      "#endif\n"
      "#endif\n"
    )
  endif()

  set(${content_var} "${${content_var}}" PARENT_SCOPE)
endfunction()

function(append_windows_prelude content_var use_win32_lean_and_mean)
  set(win32_lean_and_mean_line "#undef WIN32_LEAN_AND_MEAN\n")

  if(use_win32_lean_and_mean)
    set(win32_lean_and_mean_line "#define WIN32_LEAN_AND_MEAN\n")
  endif()

  string(APPEND ${content_var}
    "#ifdef _WINDOWS_\n"
    "#error Windows.h must not be included before this compatibility source\n"
    "#endif\n"
    "#undef NOMINMAX\n"
    "${win32_lean_and_mean_line}"
    "#undef NOGDI\n"
    "#include <Windows.h>\n"
    "#include <BaseTsd.h>\n"
    "#if !defined(_SSIZE_T_DEFINED)\n"
    "using ssize_t = SSIZE_T;\n"
    "#define _SSIZE_T_DEFINED\n"
    "#endif\n"
  )

  append_windows_macro_checks(${content_var} "is expected after including Windows.h" TRUE FALSE)

  string(APPEND ${content_var}
    "#ifdef small\n"
    "#define GLAZE_WINDOWS_HEADER_HAS_SMALL 1\n"
    "#endif\n"
  )

  set(${content_var} "${${content_var}}" PARENT_SCOPE)
endfunction()

function(write_compatibility_source source_path use_win32_lean_and_mean headers_var)
  set(source_content "")
  append_windows_prelude(source_content ${use_win32_lean_and_mean})

  foreach(external_header IN LISTS ARGN)
    string(APPEND source_content "#include <${external_header}>\n")
  endforeach()

  if(ARGN)
    append_windows_macro_checks(source_content "was not preserved after including external integration headers" FALSE TRUE)
  endif()

  foreach(header_path IN LISTS ${headers_var})
    file(RELATIVE_PATH header_relative_path "${GLAZE_SOURCE_DIR}/include" "${header_path}")
    string(REPLACE "\\" "/" header_include_path "${header_relative_path}")
    string(APPEND source_content "#include <${header_include_path}>\n")
    append_windows_macro_checks(source_content "was not preserved after including ${header_include_path}" FALSE TRUE)
  endforeach()

  file(WRITE "${source_path}" "${source_content}")
endfunction()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(source_path "${CMAKE_CURRENT_BINARY_DIR}/generated/glaze_windows_header_compatibility.cpp")
set(lean_source_path "${CMAKE_CURRENT_BINARY_DIR}/generated/glaze_windows_lean_header_compatibility.cpp")

write_compatibility_source("${source_path}" FALSE glaze_windows_header_compatibility_headers Eigen/Dense)
write_compatibility_source("${lean_source_path}" TRUE glaze_windows_lean_header_compatibility_headers asio.hpp)

add_library(glaze_windows_header_compatibility OBJECT "${source_path}" "${lean_source_path}")
target_compile_features(glaze_windows_header_compatibility PRIVATE cxx_std_23)
target_compile_definitions(glaze_windows_header_compatibility PRIVATE GLZ_ENABLE_EETF ASIO_STANDALONE)
target_include_directories(glaze_windows_header_compatibility PRIVATE "${ERLANG_EI_INCLUDE_DIR}")
target_link_libraries(glaze_windows_header_compatibility PRIVATE Eigen3::Eigen asio::asio)

if(TARGET glaze::glaze)
  target_link_libraries(glaze_windows_header_compatibility PRIVATE glaze::glaze)
elseif(TARGET glaze)
  target_link_libraries(glaze_windows_header_compatibility PRIVATE glaze)
else()
  target_include_directories(glaze_windows_header_compatibility PRIVATE "${GLAZE_SOURCE_DIR}/include")
endif()