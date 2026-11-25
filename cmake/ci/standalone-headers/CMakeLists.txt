# Verifies that every public Glaze header compiles when it is the only #include in a
# translation unit. A header that uses a name without including the header that declares
# it compiles fine through an umbrella header (which happens to include the missing
# dependency first) but fails for a user who includes it directly. One generated
# translation unit per header catches this; the build has no other purpose.
#
# Optional dependencies (Erlang ei, OpenSSL, Asio, Eigen) are required here so that the
# headers that use them are checked too.

cmake_minimum_required(VERSION 3.31)

project(glaze_standalone_headers LANGUAGES CXX)

set(GLAZE_SOURCE_DIR "" CACHE PATH "Glaze source directory")

if(NOT GLAZE_SOURCE_DIR)
  message(FATAL_ERROR "GLAZE_SOURCE_DIR is required")
endif()

get_filename_component(GLAZE_SOURCE_DIR "${GLAZE_SOURCE_DIR}" ABSOLUTE)

if(NOT EXISTS "${GLAZE_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "GLAZE_SOURCE_DIR must point to the Glaze source tree")
endif()

# Enable the optional features whose headers would otherwise be unusable.
# glaze_INCLUDES_WITH_SYSTEM is turned off because Glaze passes its SYSTEM warning guard
# to target_link_libraries for these features, which CMake rejects on an INTERFACE library.
set(glaze_EETF_FORMAT ON)
set(glaze_ENABLE_SSL ON)
set(glaze_INCLUDES_WITH_SYSTEM OFF)
add_subdirectory("${GLAZE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/glaze")

list(APPEND CMAKE_MODULE_PATH "${GLAZE_SOURCE_DIR}/cmake")
include(glaze-asio)
glaze_setup_asio()

find_package(Eigen3 CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../non-standalone-headers.cmake")

file(GLOB_RECURSE glaze_public_headers CONFIGURE_DEPENDS RELATIVE "${GLAZE_SOURCE_DIR}/include"
  "${GLAZE_SOURCE_DIR}/include/glaze/*.hpp"
)
list(REMOVE_ITEM glaze_public_headers ${glaze_non_standalone_fragments})
list(SORT glaze_public_headers)

list(LENGTH glaze_public_headers glaze_public_header_count)
if(glaze_public_header_count EQUAL 0)
  message(FATAL_ERROR "No Glaze public headers were found")
endif()

message(STATUS "Checking ${glaze_public_header_count} Glaze public headers for standalone compilation")
message(STATUS "Excluding ${glaze_non_standalone_fragments} (compiled transitively via their parent header)")

set(glaze_standalone_sources "")
foreach(header IN LISTS glaze_public_headers)
  string(MAKE_C_IDENTIFIER "${header}" header_id)
  set(source_path "${CMAKE_CURRENT_BINARY_DIR}/generated/${header_id}.cpp")
  # configure-time write that only touches the file when its content changes, so
  # reconfiguring does not force every translation unit to rebuild
  file(CONFIGURE OUTPUT "${source_path}" CONTENT "#include <${header}>\n")
  list(APPEND glaze_standalone_sources "${source_path}")
endforeach()

add_library(${PROJECT_NAME} OBJECT ${glaze_standalone_sources})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE glaze::glaze glaze::asio Eigen3::Eigen)
