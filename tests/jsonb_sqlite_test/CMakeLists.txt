project(jsonb_sqlite_test)

# Require SQLite >= 3.45 (first version with JSONB support). Skip this target silently if
# SQLite is not available, so developers without SQLite can still build the rest of the tests.
find_package(SQLite3 3.45 QUIET)

if(SQLite3_FOUND)
    add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common SQLite::SQLite3)
    add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
    target_code_coverage(${PROJECT_NAME} AUTO ALL)
    message(STATUS "jsonb_sqlite_test: SQLite ${SQLite3_VERSION} found — test enabled")
else()
    message(STATUS "jsonb_sqlite_test: SQLite3 >= 3.45 not found — test disabled")
endif()
