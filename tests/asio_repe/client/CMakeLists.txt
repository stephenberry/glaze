project(repe_client)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE include ${asio_SOURCE_DIR}/asio/include)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

target_code_coverage(${PROJECT_NAME} AUTO ALL)