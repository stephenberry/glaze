cmake_minimum_required(VERSION 3.18)

project(
   glaze_example
   VERSION 0.0.1
   LANGUAGES CXX
)

file(GLOB srcs src/*.cpp include/*.hpp)

find_package(glaze REQUIRED)

add_executable(${PROJECT_NAME} ${srcs})
target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} PRIVATE glaze::glaze)