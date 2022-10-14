project(test_lib)

add_library(${PROJECT_NAME} SHARED ${PROJECT_NAME}.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "_d")
set_target_properties(${PROJECT_NAME} PROPERTIES RELWITHDEBINFO_POSTFIX "_rd")

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_link_libraries(${PROJECT_NAME} PRIVATE glaze::glaze)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${CMAKE_SOURCE_DIR}/bin" COMPONENT test_lib)
install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION "${CMAKE_SOURCE_DIR}/bin" COMPONENT test_lib)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND 
  ${CMAKE_COMMAND} -D "COMPONENT:STRING=test_lib" 
                   -D "CMAKE_INSTALL_LOCAL_ONLY:STRING=TRUE" 
                   -D "BUILD_TYPE:STRING=\$<CONFIG>"
                   -P "${PROJECT_BINARY_DIR}/cmake_install.cmake" VERBATIM)
