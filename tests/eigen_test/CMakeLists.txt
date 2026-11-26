project(eigen_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

find_package (Eigen3 3.4 QUIET)
if(NOT Eigen3_FOUND)
  include(FetchContent)
  # A pinned release archive rather than a git clone, so a GitLab outage does not fail the
  # configure step: CMake tries each URL in order, and the hash guarantees the Fedora
  # lookaside mirror serves the same bytes as upstream.
  FetchContent_Declare(
    Eigen3
    URL
      https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.bz2
      https://src.fedoraproject.org/repo/pkgs/eigen3/eigen-3.4.0.tar.bz2/sha512/cc488eb111e0e248744d2bc4475b345b5fb82361dff226a5b73a33bd0388de8c219cff8cffcf8f476b672fc0e223f339e8c6a1cfb6293840a4a6abf232438a89/eigen-3.4.0.tar.bz2
    URL_HASH SHA256=b4c198460eba6f28d34894e3a5710998818515104d6e74e5cc331ce31e46e626
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
