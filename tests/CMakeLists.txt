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
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL YES)
FetchContent_MakeAvailable(ut)
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL NO)
message(STATUS "...finished fetching dependencies.")

add_subdirectory(api_test)
add_subdirectory(binary_test)
add_subdirectory(eigen_test)
add_subdirectory(json_test)
add_subdirectory(lib_test)
