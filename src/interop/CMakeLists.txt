# Interop library for language bindings
add_library(glaze_interop SHARED
    interop.cpp
)

target_include_directories(glaze_interop PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(glaze_interop PUBLIC cxx_std_23)
target_compile_definitions(glaze_interop PRIVATE GLZ_EXPORTS)
target_link_libraries(glaze_interop PUBLIC glaze::glaze)

set_target_properties(glaze_interop PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# C++ client library for consuming interop interface
add_library(glaze_interop_client STATIC
    client.cpp
    glaze.cpp
)

target_include_directories(glaze_interop_client PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(glaze_interop_client PUBLIC cxx_std_23)

# Platform-specific linking for dynamic loading
if(UNIX AND NOT APPLE)
    target_link_libraries(glaze_interop_client PUBLIC dl)
endif()

# Installation
install(TARGETS glaze_interop glaze_interop_client
    EXPORT glaze-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES 
    ${PROJECT_SOURCE_DIR}/include/glaze/interop/interop.hpp
    ${PROJECT_SOURCE_DIR}/include/glaze/interop/client.hpp
    ${PROJECT_SOURCE_DIR}/include/glaze/interop/glaze.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/glaze/interop
)