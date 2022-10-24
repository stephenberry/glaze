project(eigen_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

#find_package(Eigen3 3.4 REQUIRED)
if(NOT TARGET Eigen3::Eigen)   
   if(DEFINED CPM_Eigen3_SOURCE AND NOT ${CPM_Eigen3_SOURCE} STREQUAL "")
      set(Eigen3_SOURCE_DIR ${CPM_Eigen3_SOURCE})
      set(Eigen3_ADDED 1)
   else()
      CPMAddPackage(
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

target_link_libraries(${PROJECT_NAME} PRIVATE glaze::glaze Boost::ut Eigen3::Eigen)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
