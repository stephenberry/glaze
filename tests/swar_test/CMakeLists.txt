project(swar_test)

add_executable(swar_test swar_test.cpp)
target_link_libraries(swar_test PRIVATE glz_test_common)
add_test(NAME swar_test COMMAND swar_test)
