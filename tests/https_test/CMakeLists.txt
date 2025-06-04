# HTTPS Test CMakeLists.txt

cmake_minimum_required(VERSION 3.21)
project(https_test)

# Add the parent directory to find glaze
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# Only build if OpenSSL is available
find_package(OpenSSL QUIET)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-32-0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(asio)

if(OpenSSL_FOUND)
    add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
    
    target_link_libraries(${PROJECT_NAME} 
        PRIVATE 
        glaze::glaze
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    
    target_compile_definitions(${PROJECT_NAME} PRIVATE GLZ_ENABLE_SSL)

    target_include_directories(${PROJECT_NAME} PRIVATE include ${asio_SOURCE_DIR}/asio/include)
    
    # Set C++ standard
    target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
    
    message(STATUS "HTTPS test will be built with OpenSSL support")
else()
    message(STATUS "OpenSSL not found - HTTPS test will be skipped")
endif()
