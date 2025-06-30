project(jsonrpc_test)

add_executable(jsonrpc_test jsonrpc_test.cpp)

target_link_libraries(jsonrpc_test PRIVATE glz_test_common)

add_test(NAME jsonrpc_test COMMAND jsonrpc_test)
