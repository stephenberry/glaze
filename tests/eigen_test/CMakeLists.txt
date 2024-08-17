project(eigen_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

find_package (Eigen3 3.4 QUIET)
if(NOT Eigen3_FOUND)
  FetchContent_Declare(
    Eigen3
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(Eigen3)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common Eigen3::Eigen)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)
