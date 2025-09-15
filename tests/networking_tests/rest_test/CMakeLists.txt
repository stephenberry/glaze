project(rest_test)



add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions glz_asio)

add_executable(rest_registry_test rest_registry_test.cpp)

target_link_libraries(rest_registry_test PRIVATE glz_test_exceptions)

add_subdirectory(rest_server)