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

FetchContent_MakeAvailable(ut)

message(STATUS "...finished fetching dependencies.")

include(cmake/code-coverage.cmake)
add_code_coverage_all_targets()

add_subdirectory(api_test)
add_subdirectory(binary_test)
add_subdirectory(eigen_test)
add_subdirectory(json_test)
add_subdirectory(lib_test)
