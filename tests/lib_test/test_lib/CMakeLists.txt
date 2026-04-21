project(test_lib)

# Remove stale dylibs from previous builds (e.g. different compiler/config)
# to prevent lib_loader from loading ABI-incompatible libraries.
file(GLOB stale_libs "${CMAKE_SOURCE_DIR}/bin/*test_lib*${CMAKE_SHARED_LIBRARY_SUFFIX}")
if(stale_libs)
   file(REMOVE ${stale_libs})
endif()

add_library(${PROJECT_NAME} SHARED ${PROJECT_NAME}.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "_d")
set_target_properties(${PROJECT_NAME} PROPERTIES RELWITHDEBINFO_POSTFIX "_rd")

target_link_libraries(${PROJECT_NAME} PRIVATE glz_test_common)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_SOURCE_DIR}/bin")
    set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_SOURCE_DIR}/bin")
    set_target_properties(${PROJECT_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_SOURCE_DIR}/bin")
endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

target_sources(test_lib
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
    test_lib.ixx
)
