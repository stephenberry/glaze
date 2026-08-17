#[=======================================================================[.rst:
FindErlang
-------

CMake Find module that locates the Erlang/OTP runtime and erlang_interface (EI) library for building NIFs
and EI-based projects.

Input Variables (Hints)
^^^^^^^^^^^^^^^^^^^^^^^
┌───────────────────┬─────────────────────────────────────────────────────────────────────────┐
│Variable           │Description                                                              │
├───────────────────┼─────────────────────────────────────────────────────────────────────────┤
│ERLANG_ROOT        │Root path of Erlang installation. If set, overrides erl auto-detection.  │
├───────────────────┼─────────────────────────────────────────────────────────────────────────┤
│ERLANG_EI_LIB      │Direct path to libei.a (erlang_interface library).                       │
├───────────────────┼─────────────────────────────────────────────────────────────────────────┤
│ERLANG_EI_INCLUDE  │Direct path to erlang_interface include directory.                       │
├───────────────────┼─────────────────────────────────────────────────────────────────────────┤
│$ENV{ERLANG_HOME}  │Environment variable for Erlang install root.                            │
└───────────────────┴─────────────────────────────────────────────────────────────────────────┘

If ERLANG_EI_LIB and ERLANG_EI_INCLUDE are both provided, auto-detection via erl is skipped entirely.
Otherwise, the module searches for the erl executable and queries it for paths.

Search Paths for erl
^^^^^^^^^^^^^^^^^^^^
$ENV{ERLANG_HOME}/bin, ${ERLANG_ROOT}/bin, /opt/bin, /sw/bin, /usr/bin, /usr/local/bin, /opt/local/bin


Output Variables
^^^^^^^^^^^^^^^^
┌───────────────────────────────────────┬─────────────────────────────────────────────────────┐
│Variable                               │Description                                          │
├───────────────────────────────────────┼─────────────────────────────────────────────────────┤
│Erlang_FOUND                           │TRUE if all components found                         │
├───────────────────────────────────────┼─────────────────────────────────────────────────────┤
│Erlang_RUNTIME                         │Path to erl executable                               │
├───────────────────────────────────────┼─────────────────────────────────────────────────────┤
│Erlang_OTP_ROOT_DIR                    │Root of the OTP installation                         │
├───────────────────────────────────────┼─────────────────────────────────────────────────────┤
│Erlang_EI_INCLUDE_DIRS                 │Include path for ei.h                                │
├───────────────────────────────────────┼─────────────────────────────────────────────────────┤
│Erlang_EI_LIBRARY_PATH                 │Directory containing libei.a                         │
├───────────────────────────────────────┼─────────────────────────────────────────────────────┤
│Erlang_OTP_VERSION                     │OTP release version string (e.g., 26)                │
└───────────────────────────────────────┴─────────────────────────────────────────────────────┘

Imported Targets
^^^^^^^^^^^^^^^^
┌─────────────────┬────────────────────┬──────────────────────────────────────────────────────┐
│Target           │Type                │Description                                           │
├─────────────────┼────────────────────┼──────────────────────────────────────────────────────┤
│Erlang::Erlang   │INTERFACE IMPORTED  │Base headers for NIF development (erl_nif.h)          │
├─────────────────┼────────────────────┼──────────────────────────────────────────────────────┤
│Erlang::EI       │INTERFACE IMPORTED  │erlang_interface library and headers (ei.h, libei.a)  │
└─────────────────┴────────────────────┴──────────────────────────────────────────────────────┘

Usage
^^^^^
find_package(Erlang REQUIRED)
target_link_libraries(my_nif PRIVATE Erlang::Erlang)
target_link_libraries(my_ei_client PRIVATE Erlang::EI)

#]=======================================================================]

include(FindPackageHandleStandardArgs)
include(CMakePrintHelpers)

function(execute_erlang IN_ERL_EXECUTABLE OUT_ERLROOT_DIR OUT_INCLUDE_DIR OUT_LIBRARY_DIR OUT_OTP_VERSION)
  execute_process(
    COMMAND ${IN_ERL_EXECUTABLE} -noshell -eval "io:format(\"~s\", [code:root_dir()])" -s erlang halt
    OUTPUT_VARIABLE Erlang_OTP_ROOT_DIR
  )

  execute_process(
    COMMAND ${IN_ERL_EXECUTABLE} -noshell -eval "io:format(\"~s\", [code:lib_dir()])" -s erlang halt
    OUTPUT_VARIABLE Erlang_OTP_LIB_DIR
  )

  execute_process(
    COMMAND ${IN_ERL_EXECUTABLE} -noshell -eval "io:format(\"~s\",[filename:basename(code:lib_dir('erl_interface'))])" -s erlang halt
    OUTPUT_VARIABLE Erlang_EI_DIR
  )

  execute_process(
    COMMAND ${IN_ERL_EXECUTABLE} -noshell -eval "io:format(\"~s\", [erlang:system_info(otp_release)])" -s erlang halt
    OUTPUT_VARIABLE Erlang_OTP_VERSION
  )

  set(${OUT_ERLROOT_DIR} ${Erlang_OTP_ROOT_DIR} PARENT_SCOPE)
  set(${OUT_INCLUDE_DIR} ${Erlang_OTP_LIB_DIR}/${Erlang_EI_DIR}/include PARENT_SCOPE)
  set(${OUT_LIBRARY_DIR} ${Erlang_OTP_LIB_DIR}/${Erlang_EI_DIR}/lib PARENT_SCOPE)
  set(${OUT_OTP_VERSION} ${Erlang_OTP_VERSION} PARENT_SCOPE)
endfunction(execute_erlang)

if(CMAKE_VERBOSE_MAKEFILE)
  cmake_print_variables(ERLANG_ROOT ERLANG_EI_LIB ERLANG_EI_INCLUDE)
endif()

if (ERLANG_EI_LIB AND ERLANG_EI_INCLUDE)
  get_filename_component(Erlang_OTP_ROOT_DIR "${ERLANG_EI_INCLUDE}/../.." ABSOLUTE)
  set(Erlang_EI_LIBRARY_PATH ${ERLANG_EI_LIB})
  set(Erlang_EI_INCLUDE_DIRS ${ERLANG_EI_INCLUDE})
  set(Erlang_OTP_VERSION "User Provided")
else()
  set(Erlang_BIN_PATHS
    $ENV{ERLANG_HOME}/bin
    ${ERLANG_ROOT}/bin
    /opt/bin
    /sw/bin
    /usr/bin
    /usr/local/bin
    /opt/local/bin
  )
  find_program(Erlang_RUNTIME
    NAMES erl
    PATHS ${Erlang_BIN_PATHS}
  )
  if(NOT Erlang_RUNTIME)
    message(FATAL_ERROR "erl executable not found! Please install Erlang or provide actual location")
  endif()
  execute_erlang(${Erlang_RUNTIME} Erlang_OTP_ROOT_DIR Erlang_EI_INCLUDE_DIRS Erlang_EI_LIBRARY_PATH Erlang_OTP_VERSION)
endif()

find_package_handle_standard_args(
  Erlang
  DEFAULT_MSG
  Erlang_OTP_ROOT_DIR
  Erlang_EI_LIBRARY_PATH
  Erlang_EI_INCLUDE_DIRS
)

if(NOT EXISTS "${Erlang_EI_LIBRARY_PATH}/libei.a")
  message(FATAL_ERROR "libei.a not found at ${Erlang_EI_LIBRARY_PATH}")
endif()

if(Erlang_FOUND)
  cmake_print_variables(Erlang_OTP_VERSION Erlang_EI_INCLUDE_DIRS Erlang_EI_LIBRARY_PATH)

  if(NOT TARGET Erlang::Erlang)
    add_library(Erlang::Erlang INTERFACE IMPORTED)
    set_target_properties(Erlang::Erlang
      PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES ${Erlang_OTP_ROOT_DIR}/include
    )
  endif()

  if(NOT TARGET Erlang::EI)
    add_library(erlang_ei STATIC IMPORTED)
    set_property(TARGET erlang_ei PROPERTY
      IMPORTED_LOCATION ${Erlang_EI_LIBRARY_PATH}/libei.a
    )

    add_library(Erlang::EI INTERFACE IMPORTED)
    set_property(TARGET Erlang::EI PROPERTY
      INTERFACE_INCLUDE_DIRECTORIES ${Erlang_EI_INCLUDE_DIRS}
    )
    set_property(TARGET Erlang::EI PROPERTY
      INTERFACE_LINK_LIBRARIES erlang_ei
    )
  endif()
endif()
