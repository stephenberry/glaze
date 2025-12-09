# glz_asio is defined in the parent tests/CMakeLists.txt

# HTTP Examples
add_subdirectory(http_examples)

# Networking Tests
add_subdirectory(asio_repe)
add_subdirectory(http_router_test)
add_subdirectory(http_client_test)
add_subdirectory(http_server_api_tests)
add_subdirectory(https_test)
add_subdirectory(jsonrpc_registry_test)
add_subdirectory(openapi_test)
add_subdirectory(repe_buffer_test)
add_subdirectory(repe_plugin_test)
add_subdirectory(repe_test)
add_subdirectory(registry_view_test)
add_subdirectory(repe_to_jsonrpc_test)
add_subdirectory(rest_test)
add_subdirectory(websocket_test)