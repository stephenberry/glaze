# Project is configured with no languages, so tell GNUInstallDirs the lib dir
set(CMAKE_INSTALL_LIBDIR lib CACHE PATH "")

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# find_package(<package>) call for consumers to find this project
set(package glaze)

install(
    DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT glaze_Development
)

install(
    TARGETS glaze_glaze
    EXPORT glazeTargets
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

write_basic_package_version_file(
    "${package}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
)

# Allow package maintainers to freely override the path for the configs
set(
    glaze_INSTALL_CMAKEDIR "${CMAKE_INSTALL_DATADIR}/${package}"
    CACHE PATH "CMake package config location relative to the install prefix"
)
mark_as_advanced(glaze_INSTALL_CMAKEDIR)

# Read FindErlang.cmake content for embedding into config file
# This avoids installing a separate FindErlang.cmake that could collide with other projects
if(glaze_EETF_FORMAT)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindErlang.cmake" glaze_FIND_ERLANG_SCRIPT)
else()
    set(glaze_FIND_ERLANG_SCRIPT "")
endif()

# Generate config file from template with dependency information
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/install-config.cmake.in"
    "${PROJECT_BINARY_DIR}/${package}Config.cmake"
    INSTALL_DESTINATION "${glaze_INSTALL_CMAKEDIR}"
)

install(
    FILES "${PROJECT_BINARY_DIR}/${package}Config.cmake"
    DESTINATION "${glaze_INSTALL_CMAKEDIR}"
    COMPONENT glaze_Development
)


install(
    FILES "${PROJECT_BINARY_DIR}/${package}ConfigVersion.cmake"
    DESTINATION "${glaze_INSTALL_CMAKEDIR}"
    COMPONENT glaze_Development
)

install(
    EXPORT glazeTargets
    NAMESPACE glaze::
    DESTINATION "${glaze_INSTALL_CMAKEDIR}"
    COMPONENT glaze_Development
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
