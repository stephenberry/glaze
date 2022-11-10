project(eigen_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

CPMFindPackage(
   NAME Eigen3
   VERSION 3.4.0
   URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
   # Eigen's CMakelists are not intended for library use
   DOWNLOAD_ONLY YES
)

if (Eigen3_ADDED)
   add_library(Eigen3 INTERFACE IMPORTED GLOBAL)
   add_library(Eigen3::Eigen ALIAS Eigen3)
   target_include_directories(Eigen3 INTERFACE ${Eigen3_SOURCE_DIR})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Boost::ut glaze::glaze Eigen3::Eigen)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
