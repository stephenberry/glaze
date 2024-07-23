project(jsonrpc_example)
add_executable(json-rpc EXCLUDE_FROM_ALL)
target_sources(json-rpc PRIVATE json-rpc.cpp)
target_link_libraries( json-rpc PRIVATE glaze::glaze)

