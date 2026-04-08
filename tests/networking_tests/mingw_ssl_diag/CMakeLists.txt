project(mingw_ssl_diag)

find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
    check_cxx_source_compiles("
        #include <openssl/ssl.h>
        int main() { return 0; }
    " MINGW_SSL_DIAG_HEADERS_OK)

    if(MINGW_SSL_DIAG_HEADERS_OK)
        add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

        target_link_libraries(${PROJECT_NAME} PRIVATE
            glz_test_exceptions
            OpenSSL::SSL
            OpenSSL::Crypto
        )

        target_compile_definitions(${PROJECT_NAME} PRIVATE GLZ_ENABLE_SSL)

        add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
    else()
        message(STATUS "mingw_ssl_diag skipped - OpenSSL headers not usable")
    endif()
else()
    message(STATUS "mingw_ssl_diag skipped - OpenSSL not found")
endif()
