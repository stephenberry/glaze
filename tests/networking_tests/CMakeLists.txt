# The glaze::asio target is created in the parent tests/CMakeLists.txt via
# glaze_setup_asio() (cmake/glaze-asio.cmake).

option(glaze_BUILD_SSL_TESTS "Build SSL/TLS tests (requires OpenSSL headers matching target architecture)" ON)
option(glaze_BUILD_WEBSOCKET_AUTOBAHN_TEST "Build Autobahn WebSocket compliance test" OFF)

# Named modules are compiled once, so SSL must be enabled on the module target,
# not only on the individual SSL test translation units.
if(GLAZE_ENABLE_MODULES AND glaze_BUILD_SSL_TESTS AND NOT glaze_ENABLE_SSL)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(glaze_glaze PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(glaze_glaze PUBLIC GLZ_ENABLE_SSL)
endif()

# The SSL tests below turn on GLZ_ENABLE_SSL per target and link OpenSSL themselves, rather
# than going through the glaze_ENABLE_SSL option, so they do not inherit that option's link
# libraries either. glaze/net/ssl.hpp reads the Windows ROOT certificate store through
# crypt32; MSVC and clang-cl pick that library up from the #pragma comment(lib) in the
# header, but MinGW ignores it. Naming it here keeps those targets linkable under MinGW too,
# and keeps the rule in one place instead of four.
function(glz_link_windows_cert_store target)
    if(WIN32)
        target_link_libraries(${target} PRIVATE crypt32)
    endif()
endfunction()

# HTTP Examples
add_subdirectory(http_examples)
add_subdirectory(http_server_post_test)
add_subdirectory(http_server_test)
add_subdirectory(http_smuggling_test)
add_subdirectory(http_header_strictness_test)
add_subdirectory(http_headers_test)
add_subdirectory(http_response_splitting_test)

# Networking Tests
add_subdirectory(asio_repe)
add_subdirectory(http_router_test)
add_subdirectory(url_test)
add_subdirectory(cors_test)
add_subdirectory(http_chunked_test)
add_subdirectory(http_content_length_test)
add_subdirectory(http_client_test)
add_subdirectory(http_client_pool_test)
if(NOT GLAZE_ENABLE_MODULES OR (NOT glaze_ENABLE_SSL AND NOT glaze_BUILD_SSL_TESTS))
    add_subdirectory(http_client_no_ssl_executor_test)
endif()
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
if(glaze_BUILD_WEBSOCKET_AUTOBAHN_TEST)
    add_subdirectory(websocket_autobahn_test)
endif()
add_subdirectory(websocket_test)
