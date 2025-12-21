project(websocket_server)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# WebSocket close and error handling tests
project(websocket_close_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# WebSocket client tests
project(websocket_client_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# UTF-8 validation tests
project(utf8_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Shared Context Bug Test
project(shared_context_bug_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# WSS (WebSocket Secure) Test - requires OpenSSL with headers
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    # Verify OpenSSL headers are actually usable (not just libraries found)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIR})
    check_cxx_source_compiles("
        #include <openssl/ssl.h>
        int main() { return 0; }
    " OPENSSL_HEADERS_AVAILABLE)

    if(OPENSSL_HEADERS_AVAILABLE)
        project(wss_test)

        add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

        target_link_libraries(${PROJECT_NAME}
            PRIVATE
            glz_test_exceptions
            OpenSSL::SSL
            OpenSSL::Crypto
        )

        target_compile_definitions(${PROJECT_NAME} PRIVATE GLZ_ENABLE_SSL)

        add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

        message(STATUS "WSS test will be built with OpenSSL support")
    else()
        message(STATUS "WSS test skipped - OpenSSL headers not usable")
    endif()
else()
    message(STATUS "WSS test skipped - OpenSSL not found")
endif()
