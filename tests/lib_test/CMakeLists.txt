project(lib_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_link_libraries(${PROJECT_NAME} PRIVATE glaze::glaze Boost::ut)

add_subdirectory(test_lib)

add_dependencies(${PROJECT_NAME} test_lib)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

