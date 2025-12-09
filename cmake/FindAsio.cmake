# Distributed under the OSI-approved BSD 3-Clause License.
# See accompanying file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindAsio
--------

Finds the standalone Asio library (header-only).

Search Order
^^^^^^^^^^^^

1. Direct header search (find_path for asio.hpp)
2. pkg-config fallback (if direct search fails)

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``Asio::Asio``
  Header-only interface library for standalone Asio.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Asio_FOUND``
  True if standalone Asio was found.
``Asio_INCLUDE_DIRS``
  Include directories needed to use Asio.
``Asio_VERSION``
  The version of Asio found (if detectable).

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Asio_INCLUDE_DIR``
  The directory containing ``asio.hpp``.

#]=======================================================================]

# Look for the main asio.hpp header
find_path(Asio_INCLUDE_DIR
    NAMES asio.hpp
    DOC "Standalone Asio include directory"
)

# Fallback to pkg-config if header not found directly
# This helps Linux distro users where Asio may only provide a .pc file
if(NOT Asio_INCLUDE_DIR)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(_Asio QUIET asio)
        if(_Asio_FOUND)
            # pkg-config returns a list; use the first include directory
            list(GET _Asio_INCLUDE_DIRS 0 _Asio_INCLUDE_DIR_FIRST)
            set(Asio_INCLUDE_DIR "${_Asio_INCLUDE_DIR_FIRST}"
                CACHE PATH "Standalone Asio include directory")
            if(_Asio_VERSION)
                set(Asio_VERSION "${_Asio_VERSION}")
            endif()
            unset(_Asio_INCLUDE_DIR_FIRST)
        endif()
    endif()
endif()

# Try to extract version from asio/version.hpp
if(Asio_INCLUDE_DIR AND EXISTS "${Asio_INCLUDE_DIR}/asio/version.hpp")
    file(STRINGS "${Asio_INCLUDE_DIR}/asio/version.hpp" _asio_version_line
         REGEX "^#define[ \t]+ASIO_VERSION[ \t]+[0-9]+")
    if(_asio_version_line)
        string(REGEX REPLACE "^#define[ \t]+ASIO_VERSION[ \t]+([0-9]+).*$" "\\1"
               _asio_version_num "${_asio_version_line}")
        # ASIO_VERSION is encoded as (major * 100000) + (minor * 100) + patch
        math(EXPR _asio_major "${_asio_version_num} / 100000")
        math(EXPR _asio_minor "(${_asio_version_num} / 100) % 1000")
        math(EXPR _asio_patch "${_asio_version_num} % 100")
        set(Asio_VERSION "${_asio_major}.${_asio_minor}.${_asio_patch}")
        unset(_asio_version_num)
        unset(_asio_major)
        unset(_asio_minor)
        unset(_asio_patch)
    endif()
    unset(_asio_version_line)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Asio
    REQUIRED_VARS Asio_INCLUDE_DIR
    VERSION_VAR Asio_VERSION
)

if(Asio_FOUND)
    set(Asio_INCLUDE_DIRS ${Asio_INCLUDE_DIR})

    if(NOT TARGET Asio::Asio)
        add_library(Asio::Asio INTERFACE IMPORTED)
        set_target_properties(Asio::Asio PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Asio_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(Asio_INCLUDE_DIR)
