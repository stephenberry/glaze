project(asio_repe)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

foreach(group IN ITEMS minimal connection_state send_receive async_core async_error custom_call)
    add_test(NAME ${PROJECT_NAME}_${group} COMMAND ${PROJECT_NAME})
    set_tests_properties(${PROJECT_NAME}_${group} PROPERTIES ENVIRONMENT "GLZ_ASIO_REPE_GROUP=${group}")
endforeach()
