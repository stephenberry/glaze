project(websocket_server)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE include ${asio_SOURCE_DIR}/asio/include)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")