cmake_minimum_required(VERSION 3.21)
project(http_server_post_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    glz_test_exceptions
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
