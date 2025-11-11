find_package(Boost QUIET CONFIG COMPONENTS system)

if(Boost_FOUND)
    add_library(glz_asio INTERFACE)
    target_link_libraries(glz_asio INTERFACE Boost::system)
    target_compile_definitions(glz_asio INTERFACE BOOST_ASIO_DYN_LINK)
else()
    FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-32-0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(asio)
    add_library(glz_asio INTERFACE)
    target_include_directories(glz_asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
endif()


# HTTP Examples
add_subdirectory(http_examples)

# Networking Tests
add_subdirectory(asio_repe)
add_subdirectory(http_router_test)
add_subdirectory(http_client_test)
add_subdirectory(http_server_api_tests)
add_subdirectory(https_test)
add_subdirectory(openapi_test)
add_subdirectory(repe_test)
add_subdirectory(repe_to_jsonrpc_test)
add_subdirectory(rest_test)
add_subdirectory(websocket_test)