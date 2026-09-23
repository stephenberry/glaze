#[=======================================================================[.rst:
FindErlang
----------

Finds the Erlang/OTP runtime and the ``erl_interface`` (EI) library, for
building NIFs and EI-based programs.

Two discovery modes are supported:

* **Auto-detection** (default): locate the ``erl`` executable and query it for
  the OTP layout. Requires an ``erl`` that can run on the build host.
* **Explicit paths**: supply ``ERLANG_EI_LIB`` and ``ERLANG_EI_INCLUDE`` and
  ``erl`` is never invoked. This is the supported path for cross-compiling to
  an embedded target, where the target's ``erl`` cannot run on the host.

Hint Variables
^^^^^^^^^^^^^^

``Erlang_ROOT``
  Root of the Erlang installation. Honored automatically by CMake's ``find_``
  commands (policy ``CMP0074``); this is the preferred spelling.
``ERLANG_ROOT``
  Legacy spelling of the above. Adds ``<root>/bin`` to the search path for
  ``erl``.
``ERLANG_EI_LIB``
  The EI library itself (e.g. ``.../lib/libei.a``) or the directory containing
  it. Both spellings are accepted.
``ERLANG_EI_INCLUDE``
  Directory containing ``ei.h``.
``ERLANG_INCLUDE``
  Directory containing ``erl_nif.h``. Only needed when the NIF headers do not
  sit at ``<otp-root>/usr/include``, which is where OTP installs them.
``ENV{ERLANG_HOME}``
  Environment variable naming the Erlang install root.

Search Paths for ``erl``
^^^^^^^^^^^^^^^^^^^^^^^^

``$ENV{ERLANG_HOME}/bin``, ``${ERLANG_ROOT}/bin``, ``/opt/bin``, ``/sw/bin``,
``/usr/bin``, ``/usr/local/bin``, ``/opt/local/bin``, plus CMake's default
program search paths (which include ``${Erlang_ROOT}``).

Result Variables
^^^^^^^^^^^^^^^^

``Erlang_FOUND``
  True if the EI library and its headers were located.
``Erlang_RUNTIME``
  Path to the ``erl`` executable. Unset when explicit paths are supplied.
``Erlang_OTP_ROOT_DIR``
  Root of the OTP installation. Unset when explicit paths are supplied and
  ``ERLANG_ROOT`` is not given.
``Erlang_EI_INCLUDE_DIRS``
  Directory containing ``ei.h``.
``Erlang_EI_LIBRARY_PATH``
  Directory containing the EI library.
``Erlang_EI_LIBRARY``
  Full path to the EI library.
``Erlang_INCLUDE_DIRS``
  Directory containing ``erl_nif.h``, when found.
``Erlang_OTP_VERSION``
  OTP release string (e.g. ``26``). Unset when explicit paths are supplied.

Imported Targets
^^^^^^^^^^^^^^^^

``Erlang::Erlang``
  Interface target carrying the NIF headers (``erl_nif.h``). Carries no include
  directory if those headers could not be located, so that EI-only builds still
  link cleanly.
``Erlang::EI``
  The ``erl_interface`` library and its headers (``ei.h``).

Usage
^^^^^

.. code-block:: cmake

  find_package(Erlang REQUIRED)
  target_link_libraries(my_nif PRIVATE Erlang::Erlang)
  target_link_libraries(my_ei_client PRIVATE Erlang::EI)

Cross-compiling, with no runnable ``erl`` on the host::

  cmake -B build \
    -DERLANG_EI_INCLUDE=<sysroot>/lib/erlang/lib/erl_interface-5.5/include \
    -DERLANG_EI_LIB=<sysroot>/lib/erlang/lib/erl_interface-5.5/lib

Removed in this revision
^^^^^^^^^^^^^^^^^^^^^^^^

The ``Erlang::ERTS`` target and the ``Erlang_COMPILE``, ``Erlang_EI_PATH``,
``Erlang_ERTS_PATH``, ``Erlang_ERTS_INCLUDE_DIRS`` and
``Erlang_ERTS_LIBRARY_PATH`` variables are gone. ``liberts.a`` is not shipped by
OTP as a linkable library, so ``Erlang::ERTS`` never resolved on a stock
install, and the ERTS include directory duplicates what OTP already installs
into ``<otp-root>/usr/include``.

#]=======================================================================]

include(FindPackageHandleStandardArgs)
include(CMakePrintHelpers)

# Evaluate one Erlang expression. OUT_RESULT is left empty when erl exits
# non-zero or prints nothing, so callers can detect an unusable runtime instead
# of building paths out of garbage.
function(_erlang_eval IN_ERL IN_EXPRESSION OUT_RESULT)
  execute_process(
    COMMAND ${IN_ERL} -noshell -eval "io:format(\"~s\", [${IN_EXPRESSION}])" -s erlang halt
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT status EQUAL 0)
    set(output "")
  endif()
  set(${OUT_RESULT} "${output}" PARENT_SCOPE)
endfunction()

# Query a runnable erl for the OTP layout. Leaves the OUT_* variables untouched
# if erl cannot be run or answers unexpectedly -- a find module must report
# "not found", never abort the caller's configure.
function(_erlang_query IN_ERL OUT_OTP_ROOT OUT_EI_INCLUDE OUT_EI_LIBRARY_DIR OUT_OTP_VERSION)
  _erlang_eval("${IN_ERL}" "code:root_dir()" root_dir)
  _erlang_eval("${IN_ERL}" "code:lib_dir()" lib_dir)
  _erlang_eval("${IN_ERL}" "filename:basename(code:lib_dir('erl_interface'))" ei_dir)
  _erlang_eval("${IN_ERL}" "erlang:system_info(otp_release)" otp_version)

  # A missing answer means we cannot trust any of the paths, so give up on the
  # whole query rather than synthesize half a layout.
  if("${root_dir}" STREQUAL "" OR "${lib_dir}" STREQUAL "" OR "${ei_dir}" STREQUAL "")
    return()
  endif()

  set(${OUT_OTP_ROOT} "${root_dir}" PARENT_SCOPE)
  set(${OUT_EI_INCLUDE} "${lib_dir}/${ei_dir}/include" PARENT_SCOPE)
  set(${OUT_EI_LIBRARY_DIR} "${lib_dir}/${ei_dir}/lib" PARENT_SCOPE)
  set(${OUT_OTP_VERSION} "${otp_version}" PARENT_SCOPE)
endfunction()

if(CMAKE_VERBOSE_MAKEFILE)
  cmake_print_variables(Erlang_ROOT ERLANG_ROOT ERLANG_EI_LIB ERLANG_EI_INCLUDE ERLANG_INCLUDE)
endif()

# Candidate directories, either handed to us or discovered via erl.
set(_erlang_ei_include_hint "")
set(_erlang_ei_library_hint "")
set(_erlang_nif_include_hint "${ERLANG_INCLUDE}")

if(ERLANG_EI_LIB AND ERLANG_EI_INCLUDE)
  set(_erlang_ei_include_hint "${ERLANG_EI_INCLUDE}")
  # Accept either the library file or the directory holding it.
  if(IS_DIRECTORY "${ERLANG_EI_LIB}")
    set(_erlang_ei_library_hint "${ERLANG_EI_LIB}")
  else()
    get_filename_component(_erlang_ei_library_hint "${ERLANG_EI_LIB}" DIRECTORY)
  endif()
  # The OTP root cannot be derived reliably from a hand-assembled sysroot
  # layout, so only trust it when it was given explicitly.
  if(ERLANG_ROOT)
    set(Erlang_OTP_ROOT_DIR "${ERLANG_ROOT}")
  elseif(Erlang_ROOT)
    set(Erlang_OTP_ROOT_DIR "${Erlang_ROOT}")
  endif()
else()
  find_program(Erlang_RUNTIME
    NAMES erl
    PATHS
      $ENV{ERLANG_HOME}/bin
      ${ERLANG_ROOT}/bin
      /opt/bin
      /sw/bin
      /usr/bin
      /usr/local/bin
      /opt/local/bin
  )
  mark_as_advanced(Erlang_RUNTIME)
  if(Erlang_RUNTIME)
    _erlang_query("${Erlang_RUNTIME}"
      Erlang_OTP_ROOT_DIR
      _erlang_ei_include_hint
      _erlang_ei_library_hint
      Erlang_OTP_VERSION
    )
  endif()
endif()

# OTP installs erl_nif.h under <root>/usr/include (see OTP's
# erts/emulator/Makefile.in RELEASE_INCLUDES); there is no <root>/include.
if(NOT _erlang_nif_include_hint AND Erlang_OTP_ROOT_DIR)
  set(_erlang_nif_include_hint "${Erlang_OTP_ROOT_DIR}/usr/include")
endif()

# Confirm the hints actually hold what we expect instead of assuming a layout.
find_path(Erlang_EI_INCLUDE_DIRS
  NAMES ei.h
  HINTS ${_erlang_ei_include_hint}
  NO_DEFAULT_PATH
)

find_library(Erlang_EI_LIBRARY
  NAMES ei
  HINTS ${_erlang_ei_library_hint}
  NO_DEFAULT_PATH
)

# NIF headers are optional: an EI-only consumer never includes erl_nif.h.
find_path(Erlang_INCLUDE_DIRS
  NAMES erl_nif.h
  HINTS ${_erlang_nif_include_hint}
  NO_DEFAULT_PATH
)

mark_as_advanced(Erlang_EI_INCLUDE_DIRS Erlang_EI_LIBRARY Erlang_INCLUDE_DIRS)

if(Erlang_EI_LIBRARY)
  get_filename_component(Erlang_EI_LIBRARY_PATH "${Erlang_EI_LIBRARY}" DIRECTORY)
endif()

# glaze embeds this module verbatim into glazeConfig.cmake rather than
# installing it as a file (see install-rules.cmake), so the call below can run
# while CMAKE_FIND_PACKAGE_NAME is "glaze". FPHSA flags that as a likely typo
# and warns every consumer at configure time; the mismatch is deliberate here.
set(FPHSA_NAME_MISMATCHED TRUE)
find_package_handle_standard_args(Erlang
  REQUIRED_VARS Erlang_EI_LIBRARY Erlang_EI_INCLUDE_DIRS
  VERSION_VAR Erlang_OTP_VERSION
)
unset(FPHSA_NAME_MISMATCHED)

if(Erlang_FOUND)
  if(CMAKE_VERBOSE_MAKEFILE)
    cmake_print_variables(Erlang_OTP_VERSION Erlang_OTP_ROOT_DIR Erlang_EI_INCLUDE_DIRS
                          Erlang_EI_LIBRARY Erlang_INCLUDE_DIRS)
  endif()

  if(NOT TARGET Erlang::Erlang)
    add_library(Erlang::Erlang INTERFACE IMPORTED)
    # Only publish the include directory when it really exists: CMake rejects an
    # imported target whose INTERFACE_INCLUDE_DIRECTORIES names a missing path.
    if(Erlang_INCLUDE_DIRS)
      set_property(TARGET Erlang::Erlang PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES "${Erlang_INCLUDE_DIRS}"
      )
    endif()
  endif()

  if(NOT TARGET Erlang::EI)
    # UNKNOWN rather than STATIC so a shared libei works too, and so no extra
    # non-namespaced target leaks into the consumer's global target namespace.
    add_library(Erlang::EI UNKNOWN IMPORTED)
    set_target_properties(Erlang::EI
      PROPERTIES
      IMPORTED_LOCATION "${Erlang_EI_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${Erlang_EI_INCLUDE_DIRS}"
    )
  endif()
endif()

unset(_erlang_ei_include_hint)
unset(_erlang_ei_library_hint)
unset(_erlang_nif_include_hint)
