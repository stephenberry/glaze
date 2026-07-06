project(http_server_headers_validation_test)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

project(ip_address_validation_test)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
