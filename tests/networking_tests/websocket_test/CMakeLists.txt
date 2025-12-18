project(websocket_server)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# WebSocket close and error handling tests
project(websocket_close_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# WebSocket client tests
project(websocket_client_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# UTF-8 validation tests
project(utf8_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Shared Context Bug Test
project(shared_context_bug_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
