include(FetchContent)

FetchContent_Declare(
  ut
  GIT_REPOSITORY https://github.com/openalgz/ut
  GIT_TAG v1.0.0
  GIT_SHALLOW TRUE
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
target_compile_features(glz_test_common INTERFACE cxx_std_23)
target_link_libraries(glz_test_common INTERFACE ut::ut glaze::glaze glz_asio)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(glz_test_common INTERFACE -fno-exceptions -fno-rtti)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
            target_compile_options(glz_test_common INTERFACE -Wall -Wextra -pedantic)
        else()
            target_compile_options(glz_test_common INTERFACE -Wall -Wextra -pedantic $<$<CONFIG:Debug>:-Werror>)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(glz_test_common INTERFACE -Wall -Wextra -pedantic $<$<CONFIG:Debug>:-Werror>)
    else()
        target_compile_options(glz_test_common INTERFACE -Wall -Wextra -pedantic)
    endif()

        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(glz_test_common INTERFACE -ftime-trace -fsanitize=address -fsanitize-address-use-after-scope -fsanitize=undefined)
      target_link_options(glz_test_common INTERFACE -ftime-trace -fsanitize=address -fsanitize-address-use-after-scope -fsanitize=undefined)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(glz_test_common INTERFACE /GR- /bigobj) #/fsanitize=address
    target_compile_options(glz_test_common INTERFACE /W4 /wd4459 /wd4805)
endif()

add_library(glz_test_exceptions INTERFACE)
target_compile_features(glz_test_exceptions INTERFACE cxx_std_23)
target_link_libraries(glz_test_exceptions INTERFACE ut::ut glaze::glaze glz_asio)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(glz_test_exceptions INTERFACE)
    target_compile_options(glz_test_exceptions INTERFACE -Wall -Wextra -pedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(glz_test_exceptions INTERFACE /GR- /bigobj) #/fsanitize=address
    target_compile_options(glz_test_exceptions INTERFACE /W4 /wd4459 /wd4805)
endif()

add_subdirectory(api_test)
add_subdirectory(beve_test)
add_subdirectory(cli_menu_test)
add_subdirectory(compare_test)
add_subdirectory(csv_test)
add_subdirectory(eigen_test)
add_subdirectory(example_json)
add_subdirectory(exceptions_test)
add_subdirectory(inplace_vector)
add_subdirectory(int_parsing)
add_subdirectory(jmespath)
add_subdirectory(jsonrpc_test)
add_subdirectory(key_transformers_test)
add_subdirectory(lib_test)
add_subdirectory(mock_json_test)
add_subdirectory(reflection)
add_subdirectory(roundtrip)
add_subdirectory(stencil)
add_subdirectory(threading_test)
add_subdirectory(toml_test)
add_subdirectory(utility_formats)

# JSON Tests
add_subdirectory(json_conformance)
add_subdirectory(json_performance)
add_subdirectory(json_reflection_test)
add_subdirectory(json_test)

add_subdirectory(networking_tests)

if(glaze_EETF_FORMAT)
    add_subdirectory(eetf_test)
endif(glaze_EETF_FORMAT)

# We don't run find_package_test or glaze-install_test with MSVC/Windows, because the Github action runner often chokes
# Don't run find_package on Clang, because Linux runs with Clang try to use GCC standard library and have errors before Clang 18
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_test(
    NAME glaze-install_test
    COMMAND
    "${CMAKE_COMMAND}"
    --install "${PROJECT_BINARY_DIR}"
    --prefix "${CMAKE_CURRENT_BINARY_DIR}/install"
    --config $<CONFIG>
    --verbose
    )

    add_test(
    NAME find_package_test
    COMMAND
    "${CMAKE_CTEST_COMMAND}"
    --verbose
    --output-on-failure
    --build-noclean
    --build-project "${PROJECT_NAME}" # helps msvc when --build-target
    --build-generator "${CMAKE_GENERATOR}"
    --build-config $<CONFIG>
    --build-and-test
    "${CMAKE_CURRENT_SOURCE_DIR}/find_package"
    "${CMAKE_CURRENT_BINARY_DIR}/find_package"
    --build-options
    "-Dglaze_ROOT:PATH=${CMAKE_CURRENT_BINARY_DIR}/install"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DBUILD_TESTING=ON"
    "$<$<BOOL:${glaze_EETF_FORMAT}>:-Dglaze_EETF_FORMAT=ON>"
    "$<$<BOOL:${glaze_EETF_FORMAT}>:-DEXTERNAL_MODULE_PATH=${PROJECT_SOURCE_DIR}/cmake>"
    --test-command "${CMAKE_CTEST_COMMAND}" --verbose --output-on-failure # inner ctest command
    )

    set_tests_properties(glaze-install_test PROPERTIES FIXTURES_SETUP glaze-install-fixture)
    set_tests_properties(find_package_test PROPERTIES FIXTURES_REQUIRED glaze-install-fixture)
endif()
