include(FetchContent)

set(BOOST_UT_ENABLE_RUN_AFTER_BUILD OFF CACHE INTERNAL "")
set(BOOST_UT_DISABLE_MODULE ON CACHE INTERNAL "")

FetchContent_Declare(
  ut
  GIT_REPOSITORY https://github.com/boost-ext/ut.git
  GIT_TAG 265199e173b16a75670fae62fc2446b9dffad39e
  FIND_PACKAGE_ARGS
)

message(STATUS "Fetching dependencies...")
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL TRUE)
set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ut)
set(CMAKE_SKIP_INSTALL_RULES OFF CACHE BOOL "" FORCE)
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL FALSE)
message(STATUS "...finished fetching dependencies.")

include(../cmake/code-coverage.cmake)
add_code_coverage_all_targets()


add_library(glz_test_common INTERFACE)
target_compile_features(glz_test_common INTERFACE cxx_std_20)
target_link_libraries(glz_test_common INTERFACE Boost::ut glaze::glaze)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(glz_test_common INTERFACE -fno-exceptions -fno-rtti)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(glz_test_common INTERFACE -fsanitize=address)
      target_link_options(glz_test_common INTERFACE -fsanitize=address)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(glz_test_common INTERFACE /GR- /bigobj) #/fsanitize=address
endif()

add_subdirectory(api_test)
add_subdirectory(binary_test)
add_subdirectory(eigen_test)
add_subdirectory(json_test)
add_subdirectory(lib_test)
