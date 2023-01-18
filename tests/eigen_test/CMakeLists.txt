project(eigen_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

find_package (Eigen3 3.4 QUIET)
if(NOT Eigen3_FOUND)
  FetchContent_Declare(
    Eigen3
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
    GIT_SHALLOW TRUE
  )
  FetchContent_GetProperties(Eigen3)
  if(NOT eigen3_POPULATED)
    FetchContent_Populate(Eigen3)
    add_library(Eigen3 INTERFACE IMPORTED GLOBAL)
    add_library(Eigen3::Eigen ALIAS Eigen3)
    target_include_directories(Eigen3 INTERFACE "${eigen3_SOURCE_DIR}")
  endif()
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Boost::ut glaze::glaze Eigen3::Eigen)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
