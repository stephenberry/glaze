include(CMakeDependentOption)

# ---- Developer mode ----

# Developer mode enables targets and code paths in the CMake scripts that are
# only relevant for the developer(s) of glaze
# Targets necessary to build the project must be provided unconditionally, so
# consumers can trivially build and package the project
if(PROJECT_IS_TOP_LEVEL)
  cmake_dependent_option(
    glaze_DEVELOPER_MODE "Enable developer mode" ON
    "PROJECT_IS_TOP_LEVEL" OFF
  )
endif()

# ---- Warning guard ----

# target_include_directories with the SYSTEM modifier will request the compiler
# to omit warnings from the provided paths, if the compiler supports that
# This is to provide a user experience similar to find_package when
# add_subdirectory or FetchContent is used to consume this project
set(warning_guard "")
if(NOT PROJECT_IS_TOP_LEVEL)
  option(
      glaze_INCLUDES_WITH_SYSTEM
      "Use SYSTEM modifier for glaze's includes, disabling warnings"
      ON
  )
  mark_as_advanced(glaze_INCLUDES_WITH_SYSTEM)
  if(glaze_INCLUDES_WITH_SYSTEM)
    set(warning_guard SYSTEM)
  endif()
endif()
