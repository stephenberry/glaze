project(eigen_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

find_package (Eigen3 3.4 QUIET)
if(NOT Eigen3_FOUND)
  FetchContent_Declare(
    Eigen3
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR cmake # no CMakeLists.txt in cmake, so this turns off configure
    # hack: https://stackoverflow.com/questions/77210209/how-to-prevent-eigen-targets-to-show-up-in-the-main-app-in-a-cmake-project
  )

  set(EIGEN_BUILD_DOC OFF CACHE BOOL "Don't build Eigen documentation")
  set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "Don't build pkg-config files")
  set(EIGEN_BUILD_TESTING OFF CACHE BOOL "Don't build Eigen tests")
  
  FetchContent_MakeAvailable(Eigen3)
  
  if(NOT TARGET Eigen3::Eigen)
    add_library(Eigen3 INTERFACE IMPORTED GLOBAL)
    add_library(Eigen3::Eigen ALIAS Eigen3)
    target_include_directories(Eigen3 INTERFACE "${eigen3_SOURCE_DIR}")
  endif()
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common Eigen3::Eigen)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)
