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
        # Test 1: Full SSL support (GLZ_ENABLE_SSL defined + OpenSSL linked)
        add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)

        target_link_libraries(${PROJECT_NAME} PRIVATE
            glz_test_exceptions
            OpenSSL::SSL
            OpenSSL::Crypto
        )

        target_compile_definitions(${PROJECT_NAME} PRIVATE GLZ_ENABLE_SSL)

        add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

        # Test 2: OpenSSL DLLs linked but GLZ_ENABLE_SSL NOT defined.
        # Isolates whether the crash is caused by DLL loading or by the macro.
        # We un-define GLZ_ENABLE_SSL that comes from the glaze::glaze interface
        # to ensure SSL code paths are not compiled.
        add_executable(mingw_link_only_test mingw_link_only_test.cpp)

        target_link_libraries(mingw_link_only_test PRIVATE
            glz_test_exceptions
            OpenSSL::SSL
            OpenSSL::Crypto
        )
        # Un-define GLZ_ENABLE_SSL that comes from glaze::glaze interface.
        # GCC processes flags left-to-right, and target_compile_options
        # appends after interface definitions, so -U wins.
        target_compile_options(mingw_link_only_test PRIVATE -UGLZ_ENABLE_SSL)

        add_test(NAME mingw_link_only_test COMMAND mingw_link_only_test)
    else()
        message(STATUS "mingw_ssl_diag skipped - OpenSSL headers not usable")
    endif()
else()
    message(STATUS "mingw_ssl_diag skipped - OpenSSL not found")
endif()
