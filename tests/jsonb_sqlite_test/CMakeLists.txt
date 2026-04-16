project(jsonb_sqlite_test)

# Require SQLite >= 3.45 (first version with JSONB support). Skip this target silently if
# SQLite is not available, so developers without SQLite can still build the rest of the tests.
#
# Skip cross-compile / non-native builds:
# - 32-bit builds on 64-bit hosts (CMAKE_SIZEOF_VOID_P != pointer size of system SQLite)
# - explicit cross-compile (CMAKE_CROSSCOMPILING)
# - explicit user opt-out (glaze_DISABLE_SQLITE_TESTS)
# In these cases find_package may locate the host's SQLite but it won't link — the symptom is
# "file in wrong format" at link time.

set(_skip_reason "")

if(glaze_DISABLE_SQLITE_TESTS)
    set(_skip_reason "glaze_DISABLE_SQLITE_TESTS is ON")
elseif(CMAKE_CROSSCOMPILING)
    set(_skip_reason "cross-compiling")
elseif(DEFINED CMAKE_CXX_FLAGS AND CMAKE_CXX_FLAGS MATCHES "(^| )-m32( |$)")
    set(_skip_reason "32-bit build (-m32)")
else()
    find_package(SQLite3 3.45 QUIET)
    if(NOT SQLite3_FOUND)
        set(_skip_reason "SQLite3 >= 3.45 not found")
    endif()
endif()

if(_skip_reason STREQUAL "")
    add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common SQLite::SQLite3)
    add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
    target_code_coverage(${PROJECT_NAME} AUTO ALL)
    message(STATUS "jsonb_sqlite_test: SQLite ${SQLite3_VERSION} found — test enabled")
else()
    message(STATUS "jsonb_sqlite_test: disabled (${_skip_reason})")
endif()
