project(jsonrpc_example)
add_executable(json-rpc EXCLUDE_FROM_ALL)
target_sources(json-rpc PRIVATE json-rpc.cpp)
target_link_libraries( json-rpc PRIVATE glaze::glaze)

add_executable(wrapping_middleware_example EXCLUDE_FROM_ALL)
target_sources(wrapping_middleware_example PRIVATE wrapping_middleware_example.cpp)
target_link_libraries(wrapping_middleware_example PRIVATE glaze::glaze)

