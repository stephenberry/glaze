project(rest_server VERSION 1.0 LANGUAGES CXX)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")