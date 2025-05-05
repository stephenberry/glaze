project(inplace_vector_test)

include(FetchContent)

FetchContent_Declare(
  inplace_vector
  GIT_REPOSITORY https://github.com/bemanproject/inplace_vector.git
  GIT_TAG main
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(inplace_vector)

add_executable(${PROJECT_NAME} inplace_vector_test.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_exceptions beman::inplace_vector)

if (MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wa,-mbig-obj")
endif ()

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_code_coverage(${PROJECT_NAME} AUTO ALL)
