CPMAddPackage(
  NAME ut
  GIT_REPOSITORY https://github.com/boost-ext/ut.git
  GIT_TAG v1.1.9
  DOWNLOAD_ONLY YES # They use Werror.
)

if (ut_ADDED)
  add_library(ut INTERFACE)
  add_library(Boost::ut ALIAS ut)
  target_include_directories(ut INTERFACE "${ut_SOURCE_DIR}/include")
  target_compile_definitions(ut INTERFACE BOOST_UT_DISABLE_MODULE)
endif()

add_subdirectory(api_test)
add_subdirectory(csv_test)
add_subdirectory(eigen_test)
add_subdirectory(json_test)
add_subdirectory(read_test)
add_subdirectory(write_test)