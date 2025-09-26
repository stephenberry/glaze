# HTTPS Test CMakeLists.txt
cmake_minimum_required(VERSION 3.21)
project(https_test)

# Only build if OpenSSL is available
find_package(OpenSSL REQUIRED)



# Test if we can actually compile with OpenSSL headers
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
check_cxx_source_compiles("
    #include <openssl/ssl.h>
    #include <openssl/opensslconf.h>
    #include <openssl/evp.h>
    #include <openssl/x509.h>
    #include <openssl/pem.h>
    int main() { return 0; }
" OPENSSL_HEADERS_AVAILABLE)

if(OPENSSL_HEADERS_AVAILABLE)
    add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        glz_test_exceptions
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE GLZ_ENABLE_SSL SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

    # Set C++ standard
    target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
    
    message(STATUS "Self-contained HTTPS test will be built with OpenSSL support")
    message(STATUS " Certificates will be generated automatically at runtime")
else()
    message(FATAL_ERROR "❌ OpenSSL headers missing - install libssl-dev/openssl-devel")
endif()