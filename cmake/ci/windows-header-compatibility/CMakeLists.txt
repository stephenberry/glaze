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

set(ERLANG_EI_INCLUDE_DIR "${ERLANG_OTP_DIR}/usr/include")

if(NOT EXISTS "${ERLANG_EI_INCLUDE_DIR}/ei.h")
  message(FATAL_ERROR "ei.h was not found at ${ERLANG_EI_INCLUDE_DIR}")
endif()

message(STATUS "Using Erlang ei include directory: ${ERLANG_EI_INCLUDE_DIR}")

add_subdirectory("${GLAZE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/glaze")

file(GLOB_RECURSE glaze_public_headers CONFIGURE_DEPENDS RELATIVE "${GLAZE_SOURCE_DIR}/include"
  "${GLAZE_SOURCE_DIR}/include/glaze/*.hpp"
)

list(LENGTH glaze_public_headers glaze_public_header_count)
if(glaze_public_header_count EQUAL 0)
  message(FATAL_ERROR "No Glaze public headers were found")
endif()

# --- Classify headers by their actual Winsock2/asio dependency, not by directory ---
#
# A header must be compiled with WIN32_LEAN_AND_MEAN if it (transitively) pulls in
# Winsock2 or asio. Otherwise <Windows.h> includes the legacy <Winsock.h> first,
# which then conflicts with <Winsock2.h>. Hard-coding the directories that happen to
# contain such headers today silently misclassifies any future socket header added
# elsewhere (it lands in the unsanitized group and fails with an opaque Winsock
# redefinition error). Instead we derive the set from the headers' own #include
# directives, so it stays correct automatically as headers are added/moved/renamed.
#
# Step 1: read each header's #include lines once. Record the glaze headers it
# includes (rooted "glaze/..." paths) and whether it directly pulls in Winsock/asio.
set(glaze_winsock_seeds "")
foreach(header IN LISTS glaze_public_headers)
  string(MAKE_C_IDENTIFIER "${header}" header_id)
  file(STRINGS "${GLAZE_SOURCE_DIR}/include/${header}" include_lines REGEX "^[ \t]*#[ \t]*include")

  set(glaze_header_deps_${header_id} "")
  foreach(line IN LISTS include_lines)
    # Direct dependency on another glaze header (rooted include path)
    if(line MATCHES "[<\"](glaze/[A-Za-z0-9_./]+\\.hpp)[\">]")
      list(APPEND glaze_header_deps_${header_id} "${CMAKE_MATCH_1}")
    endif()
    # Direct dependency on Winsock2/asio marks this header as a classification seed
    if(line MATCHES "[<\"](asio[/.]|winsock|ws2def|ws2tcpip|mswsock)")
      list(APPEND glaze_winsock_seeds "${header}")
    endif()
  endforeach()
endforeach()
list(REMOVE_DUPLICATES glaze_winsock_seeds)

# Step 2: propagate seeds along the include graph to a fixed point. A header is
# "lean" if its transitive include closure reaches a seed.
set(glaze_windows_lean_headers ${glaze_winsock_seeds})
set(glaze_lean_changed TRUE)
while(glaze_lean_changed)
  set(glaze_lean_changed FALSE)
  foreach(header IN LISTS glaze_public_headers)
    if(NOT header IN_LIST glaze_windows_lean_headers)
      string(MAKE_C_IDENTIFIER "${header}" header_id)
      foreach(dep IN LISTS glaze_header_deps_${header_id})
        if(dep IN_LIST glaze_windows_lean_headers)
          list(APPEND glaze_windows_lean_headers "${header}")
          set(glaze_lean_changed TRUE)
          break()
        endif()
      endforeach()
    endif()
  endforeach()
endwhile()

# Some headers are include-fragments rather than standalone units: they specialize
# templates whose primary declaration lives only in a parent header that #includes
# them (e.g. the *_registry_impl.hpp fragments are pulled in by rpc/registry.hpp and
# fail to compile on their own). They must never be #included directly, so we drop
# them from the generated translation units; they are still exercised transitively
# through their parent. This also removes the load-bearing include-ordering hack the
# directory-based approach needed for registry.hpp.
set(glaze_non_standalone_fragments
  "glaze/net/rest_registry_impl.hpp"
  "glaze/rpc/jsonrpc_registry_impl.hpp"
  "glaze/rpc/repe/repe_registry_impl.hpp"
)
list(REMOVE_ITEM glaze_public_headers ${glaze_non_standalone_fragments})
list(REMOVE_ITEM glaze_windows_lean_headers ${glaze_non_standalone_fragments})

# The unsanitized group is everything that is not Winsock2/asio-dependent.
set(glaze_windows_headers "${glaze_public_headers}")
list(REMOVE_ITEM glaze_windows_headers ${glaze_windows_lean_headers})

list(REMOVE_DUPLICATES glaze_windows_lean_headers)
list(SORT glaze_public_headers)
list(SORT glaze_windows_headers)
list(SORT glaze_windows_lean_headers)

# Counting headers is needed only for CI logs
list(LENGTH glaze_windows_headers glaze_windows_header_count)
list(LENGTH glaze_windows_lean_headers glaze_windows_lean_header_count)

message(STATUS "Found ${glaze_public_header_count} Glaze public headers")
message(STATUS "Using unsanitized Windows.h for ${glaze_windows_header_count} Glaze public headers")
message(STATUS "Using WIN32_LEAN_AND_MEAN Windows.h for ${glaze_windows_lean_header_count} Glaze public headers: ${glaze_windows_lean_headers}")
message(STATUS "Excluding ${glaze_non_standalone_fragments} (compiled transitively via their parent header)")

# Checks that Glaze code haven't accidentally #undef-ed any Windows macro
function(append_windows_macro_asserts content_var error_context require_delete require_small)
  set(macro_names min max ERROR near far)

  if(require_delete)
    set(macro_names min max ERROR DELETE near far)
  endif()

  foreach(macro_name IN LISTS macro_names)
    string(APPEND ${content_var}
      "#ifndef ${macro_name}\n"
      "#error ${macro_name} macro ${error_context}\n"
      "#endif\n"
    )
  endforeach()

  if(require_small)
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
  set(win32_lean_and_mean_line "#undef WIN32_LEAN_AND_MEAN")

  # WIN32_LEAN_AND_MEAN avoids Winsock.h so Winsock2.h can be included later
  if(use_win32_lean_and_mean)
    set(win32_lean_and_mean_line "#define WIN32_LEAN_AND_MEAN")
  endif()

  string(APPEND ${content_var}
    "#ifdef _WINDOWS_\n"
    "#error Windows.h must not be included before this compatibility source\n"
    "#endif\n"
    "#undef NOMINMAX\n"
    "${win32_lean_and_mean_line}\n"
    "#undef NOGDI\n"
    "#include <Windows.h>\n"
    "#include <BaseTsd.h>\n"
    "#if !defined(_SSIZE_T_DEFINED)\n"
    "using ssize_t = SSIZE_T;\n"
    "#define _SSIZE_T_DEFINED\n"
    "#endif\n"
  )

  # Verify that none of the Windows macros was #undef-ed by Glaze code
  append_windows_macro_asserts(${content_var} "is expected after including Windows.h" TRUE FALSE)

  string(APPEND ${content_var}
    "#ifdef small\n"
    "#define GLAZE_WINDOWS_HEADER_HAS_SMALL 1\n"
    "#endif\n"
  )

  set(${content_var} "${${content_var}}" PARENT_SCOPE)
endfunction()

# Each header group is compiled as ONE aggregate translation unit (Windows.h once,
# followed by every header in the group), rather than one TU per header or per
# directory. This is deliberate, not just a speed optimization:
#
#   * Glaze headers are not all standalone. Many rely on declarations established by
#     an earlier include (e.g. util/dump.hpp uses string_literal, core/array_apply.hpp
#     uses size_t) and do not compile in isolation. Splitting per-header or per-subdir
#     would produce spurious failures that have nothing to do with Windows macros, so
#     the aggregate TU in a stable (sorted) order is what actually reflects real usage.
#   * A genuine Windows-macro collision is still pinpointed: the compiler reports the
#     offending header through its #include stack, and the macro-preservation #error
#     below names the exact header it followed. So the aggregate model does not lose
#     the ability to locate a failure.
#
# Writes a .cpp file that includes Windows.h, then tests every provided Glaze header.
function(write_header_test_source source_path use_win32_lean_and_mean)
  set(source_content "")
  append_windows_prelude(source_content ${use_win32_lean_and_mean})

  foreach(header_path IN LISTS ARGN)
    string(REPLACE "\\" "/" header_include_path "${header_path}")
    string(APPEND source_content
      "#include <${header_include_path}>\n"
    )
    append_windows_macro_asserts(source_content "was not preserved after including ${header_include_path}" FALSE TRUE)
  endforeach()

  file(WRITE "${source_path}" "${source_content}")
endfunction()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(source_path "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROJECT_NAME}.cpp")
set(lean_source_path "${CMAKE_CURRENT_BINARY_DIR}/generated/glaze_windows_lean_header_compatibility.cpp")

# Generate regular source test file without WIN32_LEAN_AND_MEAN (simulating poor Windows environment)
write_header_test_source("${source_path}" FALSE Eigen/Dense ${glaze_windows_headers})

# Generate the test source for Asio and Winsock2-dependent headers with #define WIN32_LEAN_AND_MEAN
write_header_test_source("${lean_source_path}" TRUE asio.hpp ${glaze_windows_lean_headers})

add_library(${PROJECT_NAME} OBJECT "${source_path}" "${lean_source_path}")
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_compile_definitions(${PROJECT_NAME} PRIVATE GLZ_ENABLE_EETF ASIO_STANDALONE)
target_include_directories(${PROJECT_NAME} PRIVATE "${ERLANG_EI_INCLUDE_DIR}")

find_package(Eigen3 CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Eigen3::Eigen asio::asio)

if(TARGET glaze::glaze)
  target_link_libraries(${PROJECT_NAME} PRIVATE glaze::glaze)
elseif(TARGET glaze)
  target_link_libraries(${PROJECT_NAME} PRIVATE glaze)
else()
  target_include_directories(${PROJECT_NAME} PRIVATE "${GLAZE_SOURCE_DIR}/include")
endif()
