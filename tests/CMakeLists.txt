include(FetchContent)

FetchContent_Declare(
  ut
  GIT_REPOSITORY https://github.com/openalgz/ut
  GIT_TAG v1.1.0
  GIT_SHALLOW TRUE
)

message(STATUS "Fetching dependencies...")
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL TRUE)
set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ut)
set(CMAKE_SKIP_INSTALL_RULES OFF CACHE BOOL "" FORCE)
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL FALSE)
message(STATUS "...finished fetching dependencies.")

# Setup the Asio backend (Boost / standalone / bundled) used by networking tests.
# Selection plus the GLZ_USE_BOOST_ASIO bridge live in cmake/glaze-asio.cmake, the
# single source of truth shared with the benchmarks, so glaze/ext/glaze_asio.hpp and
# the backend CMake links can never disagree (issue #2599). Must run before
# glz_test_common/glz_test_exceptions, which link glaze::asio.
#
# Distro maintainers can set -Dglaze_USE_BUNDLED_ASIO=OFF to require system packages.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
include(glaze-asio)
glaze_setup_asio()

include(../cmake/code-coverage.cmake)
add_code_coverage_all_targets()

option(glaze_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer for tests" ON)

add_library(glz_test_common INTERFACE)
target_compile_features(glz_test_common INTERFACE cxx_std_23)
target_link_libraries(glz_test_common INTERFACE ut::ut glaze::glaze glaze::asio)

if(MSVC)
    target_compile_options(glz_test_common INTERFACE /GR- /bigobj)
    target_compile_options(glz_test_common INTERFACE /W4 /wd4459 /wd4805)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(glz_test_common INTERFACE -fno-exceptions -fno-rtti)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
        target_compile_options(glz_test_common INTERFACE -Wall -Wextra -pedantic)
    else()
        target_compile_options(glz_test_common INTERFACE -Wall -Wextra -pedantic $<$<CONFIG:Debug>:-Werror>)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
    target_compile_options(glz_test_common INTERFACE -fno-exceptions -fno-rtti)
    target_compile_options(glz_test_common INTERFACE -Wall -Wextra -pedantic $<$<CONFIG:Debug>:-Werror>)
    target_compile_options(glz_test_common INTERFACE -ftime-trace)
    if(glaze_ENABLE_SANITIZERS)
        target_compile_options(glz_test_common INTERFACE -fsanitize=address -fsanitize-address-use-after-scope -fsanitize=undefined)
        target_link_options(glz_test_common INTERFACE -fsanitize=address -fsanitize-address-use-after-scope -fsanitize=undefined)
    endif()
endif()

add_library(glz_test_exceptions INTERFACE)
target_compile_features(glz_test_exceptions INTERFACE cxx_std_23)
target_link_libraries(glz_test_exceptions INTERFACE ut::ut glaze::glaze glaze::asio)
if(MSVC)
    target_compile_options(glz_test_exceptions INTERFACE /GR- /bigobj)
    target_compile_options(glz_test_exceptions INTERFACE /W4 /wd4459 /wd4805)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(glz_test_exceptions INTERFACE -Wall -Wextra -pedantic)
endif()

add_subdirectory(api_test)
add_subdirectory(beve_test)
add_subdirectory(bson_test)
add_subdirectory(cbor_test)
add_subdirectory(jsonb_test)
add_subdirectory(jsonb_sqlite_test)
add_subdirectory(buffer_pool_test)
add_subdirectory(cli_menu_test)
add_subdirectory(compare_test)
add_subdirectory(ordered_small_map_test)
add_subdirectory(ordered_map_test)
add_subdirectory(csv_test)
add_subdirectory(eigen_test)
add_subdirectory(example_json)
add_subdirectory(exceptions_test)
add_subdirectory(float_write_test)
add_subdirectory(size_optimization_test)
add_subdirectory(inplace_vector)
option(glaze_BUILD_PERFORMANCE_TESTS "Build performance tests" ON)
if(glaze_BUILD_PERFORMANCE_TESTS)
    add_subdirectory(int_parsing)
endif()
add_subdirectory(jmespath)
# jsonrpc_test triggers a libc++ std::expected constraint recursion bug in P2996 clang
if(NOT glaze_ENABLE_REFLECTION26)
    add_subdirectory(jsonrpc_test)
endif()
add_subdirectory(key_transformers_test)
add_subdirectory(lib_test)
add_subdirectory(mock_json_test)
add_subdirectory(reflection)
add_subdirectory(roundtrip)
add_subdirectory(stencil)
add_subdirectory(threading_test)
add_subdirectory(toml_test)
add_subdirectory(utility_formats)
add_subdirectory(yaml_test)

add_subdirectory(yaml_conformance)
add_subdirectory(msgpack_test)
add_subdirectory(ostream_buffer_test)
add_subdirectory(istream_buffer_test)

# simple_float_test runs exhaustive tests over all 2^32 float values
# Disabled by default - run via dedicated CI workflow in Release mode
option(glaze_SIMPLE_FLOAT_TEST "Build simple_float exhaustive tests (slow, Release mode recommended)" OFF)
if(glaze_SIMPLE_FLOAT_TEST)
    add_subdirectory(simple_float_test)
endif()

# JSON Tests
add_subdirectory(chrono_test)
add_subdirectory(json_conformance)
if(glaze_BUILD_PERFORMANCE_TESTS)
    add_subdirectory(json_performance)
endif()
add_subdirectory(json_reflection_test)
add_subdirectory(json_test)

option(glaze_BUILD_NETWORKING_TESTS "Build networking tests (requires OpenSSL)" ON)
if(glaze_BUILD_NETWORKING_TESTS)
    add_subdirectory(networking_tests)
endif()

if(glaze_EETF_FORMAT)
    add_subdirectory(eetf_test)
endif(glaze_EETF_FORMAT)

if(glaze_ENABLE_REFLECTION26)
    add_subdirectory(p2996_test)
endif()

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
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DBUILD_TESTING=ON"
    "$<$<BOOL:${glaze_EETF_FORMAT}>:-Dglaze_EETF_FORMAT=ON>"
    "$<$<BOOL:${glaze_EETF_FORMAT}>:-DEXTERNAL_MODULE_PATH=${PROJECT_SOURCE_DIR}/cmake>"
    --test-command "${CMAKE_CTEST_COMMAND}" --verbose --output-on-failure # inner ctest command
    )

    set_tests_properties(glaze-install_test PROPERTIES FIXTURES_SETUP glaze-install-fixture)
    set_tests_properties(find_package_test PROPERTIES FIXTURES_REQUIRED glaze-install-fixture)
endif()
