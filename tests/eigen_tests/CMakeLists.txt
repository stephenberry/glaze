project(vireo_eigen_tests)

add_executable(${PROJECT_NAME} main.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
 target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
 target_compile_options(${PROJECT_NAME} PRIVATE /W4)
endif()

CPMAddPackage(
  NAME ut
  GIT_REPOSITORY https://github.com/boost-ext/ut.git
  GIT_TAG v1.1.9
  DOWNLOAD_ONLY YES # They use Werror.
)

if (ut_ADDED)
  add_library(ut INTERFACE)
  add_library(Boost::ut ALIAS ut)
  target_include_directories(ut INTERFACE "${ut_SOURCE_DIR}/include")
  target_compile_definitions(ut INTERFACE BOOST_UT_DISABLE_MODULE)
endif()

if(NOT TARGET Eigen)   
   if(DEFINED CPM_Eigen_SOURCE AND NOT ${CPM_Eigen_SOURCE} STREQUAL "")
      set(Eigen_SOURCE_DIR ${CPM_Eigen_SOURCE})
      set(Eigen_ADDED 1)
   else()
      CPMAddPackage(
        NAME Eigen
        VERSION 3.4.0
        URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
        # Eigen's CMakelists are not intended for library use
        DOWNLOAD_ONLY YES 
      )
   endif()
   
   if(Eigen_ADDED)
     add_library(Eigen INTERFACE IMPORTED GLOBAL)
     target_include_directories(Eigen INTERFACE ${Eigen_SOURCE_DIR})
   endif()
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE glaze Boost::ut Eigen)

include("${ut_SOURCE_DIR}/cmake/AddCustomCommandOrTest.cmake")
ut_add_custom_command_or_test(TARGET ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
