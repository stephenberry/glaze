CPMFindPackage(
  NAME ut
  OPTIONS "BOOST_UT_ENABLE_RUN_AFTER_BUILD OFF" "BOOST_UT_DISABLE_MODULE ON"
  GIT_REPOSITORY https://github.com/boost-ext/ut.git
  GIT_TAG v1.1.9
)

add_subdirectory(api_test)
add_subdirectory(binary_test)
add_subdirectory(csv_test)
add_subdirectory(eigen_test)
add_subdirectory(json_test)
add_subdirectory(lib_test)
