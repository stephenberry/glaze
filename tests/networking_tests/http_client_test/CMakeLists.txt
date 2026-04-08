project(http_client_test)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions)

# Conditionally enable SSL support if OpenSSL is available
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
    check_cxx_source_compiles("
        #include <openssl/ssl.h>
        int main() { return 0; }
    " OPENSSL_HEADERS_AVAILABLE)

    if(OPENSSL_HEADERS_AVAILABLE)
        target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
        target_compile_definitions(${PROJECT_NAME} PRIVATE GLZ_ENABLE_SSL)
        message(STATUS "HTTP client test built with SSL support")
    endif()
endif()

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
