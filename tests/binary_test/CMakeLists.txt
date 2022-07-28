project(binary_test)

add_executable(${PROJECT_NAME} main.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
 target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
 target_compile_options(${PROJECT_NAME} PRIVATE /W4)
endif()

include("${ut_SOURCE_DIR}/cmake/AddCustomCommandOrTest.cmake")

target_link_libraries(${PROJECT_NAME} PRIVATE glaze Boost::ut)

#ut_add_custom_command_or_test(TARGET ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
