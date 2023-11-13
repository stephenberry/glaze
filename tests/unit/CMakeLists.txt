
add_executable(prefer-ut prefer.test.cpp)
target_link_libraries(prefer-ut PRIVATE glaze_glaze Boost::ut)
add_test(NAME prefer-ut COMMAND prefer-ut)
