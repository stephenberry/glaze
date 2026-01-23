# glz_asio is defined in the parent tests/CMakeLists.txt

option(glaze_BUILD_SSL_TESTS "Build SSL/TLS tests (requires OpenSSL headers matching target architecture)" ON)

# HTTP Examples
add_subdirectory(http_examples)
add_subdirectory(http_server_post_test)

# Networking Tests
add_subdirectory(asio_repe)
add_subdirectory(http_router_test)
add_subdirectory(url_test)
add_subdirectory(http_client_test)
if(glaze_BUILD_SSL_TESTS)
    add_subdirectory(http_client_ssl_test)
endif()
add_subdirectory(http_server_api_tests)
if(glaze_BUILD_SSL_TESTS)
    add_subdirectory(https_test)
endif()
add_subdirectory(jsonrpc_registry_test)
add_subdirectory(openapi_test)
add_subdirectory(repe_buffer_test)
add_subdirectory(repe_plugin_test)
add_subdirectory(repe_test)
add_subdirectory(registry_view_test)
add_subdirectory(repe_to_jsonrpc_test)
add_subdirectory(rest_test)
add_subdirectory(websocket_test)
