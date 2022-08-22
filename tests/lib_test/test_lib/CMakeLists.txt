project(test_lib)

add_library(${PROJECT_NAME} SHARED ${PROJECT_NAME}.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
 target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
 target_compile_options(${PROJECT_NAME} PRIVATE /W4)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE glaze)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${CMAKE_SOURCE_DIR}/bin" COMPONENT Development)
install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION "${CMAKE_SOURCE_DIR}/bin" COMPONENT Development)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND 
  ${CMAKE_COMMAND} -D "COMPONENT:STRING=Development" 
                   -D "CMAKE_INSTALL_LOCAL_ONLY:STRING=TRUE" 
                   -D "BUILD_TYPE:STRING=\$(Configuration)" 
                   -P "${PROJECT_BINARY_DIR}/cmake_install.cmake" VERBATIM)
