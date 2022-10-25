include(cmake/CPM.cmake)

macro(glaze_redirect_find_package_to_cpm method package)
  if("${package}" STREQUAL "fmt")
   CPMAddPackage(
      NAME fmt
      OPTIONS FMT_INSTALL
      GIT_REPOSITORY https://github.com/fmtlib/fmt
      GIT_TAG 9.1.0
   )
    set("${package}_FOUND" YES)
  elseif("${package}" STREQUAL "nanorange")
   CPMAddPackage(
      NAME nanorange
      OPTIONS USE_SINGLE_HEADER "BUILD_TESTING OFF" DO_INSTALL
      GIT_REPOSITORY https://github.com/tcbrindle/NanoRange
      GIT_TAG master # No version tags
   )

   if (nanorange_ADDED)
      add_library(nanorange::nanorange ALIAS nanorange)
   endif()
  elseif("${package}" STREQUAL "frozen")
  CPMAddPackage(
      NAME frozen
      GIT_REPOSITORY https://github.com/serge-sans-paille/frozen.git
      GIT_TAG 1.1.1
   )
  elseif("${package}" STREQUAL "FastFloat")
   CPMAddPackage(
      NAME FastFloat
      GIT_REPOSITORY https://github.com/fastfloat/fast_float.git
      GIT_TAG main # waiting for new release, need latest
   )
   elseif("${package}" STREQUAL "ut")
   CPMAddPackage(
    NAME ut
    OPTIONS "BOOST_UT_ENABLE_RUN_AFTER_BUILD OFF"
    GIT_REPOSITORY https://github.com/boost-ext/ut.git
    GIT_TAG v1.1.9
  )
  elseif("${package}" STREQUAL "Eigen3")
    if(NOT TARGET Eigen3::Eigen)   
    if(DEFINED CPM_Eigen3_SOURCE AND NOT ${CPM_Eigen3_SOURCE} STREQUAL "")
        set(Eigen3_SOURCE_DIR ${CPM_Eigen3_SOURCE})
        set(Eigen3_ADDED 1)
    else()
      CPMFindPackage(
        NAME Eigen3
        VERSION 3.4.0
        URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
        # Eigen's CMakelists are not intended for library use
        DOWNLOAD_ONLY YES 
      )
    endif()
    
    if(Eigen3_ADDED)
      add_library(Eigen3 INTERFACE IMPORTED GLOBAL)
      add_library(Eigen3::Eigen ALIAS Eigen3)
      target_include_directories(Eigen3 INTERFACE ${Eigen3_SOURCE_DIR})
    endif()
  endif()
  endif()
endmacro()

cmake_language(
    SET_DEPENDENCY_PROVIDER glaze_redirect_find_package_to_cpm
    SUPPORTED_METHODS FIND_PACKAGE
)