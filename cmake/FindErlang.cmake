# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindErlang
-------

Finds Erlang libraries.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``Erlang::Erlang``
  Header only interface library suitible for compiling NIFs.

``Erlang::EI``
  Erlang interface library.

``Erlang::ERTS``
  Erlang runtime system library.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Erlang_FOUND``
  True if the system has the Erlang library.
``Erlang_EI_PATH``
  The path to the Erlang erl_interface path.
``Erlang_ERTS_PATH``
  The path to the Erlang erts path.
``Erlang_EI_INCLUDE_DIRS``
  /include appended to Erlang_EI_PATH.
``Erlang_EI_LIBRARY_PATH``
  /lib appended to Erlang_EI_PATH.
``Erlang_ERTS_INCLUDE_DIRS``
  /include appended to Erlang_ERTS_PATH.
``Erlang_ERTS_LIBRARY_PATH``
  /lib appended to Erlang_ERTS_PATH.
``Erlang_OTP_VERSION``
  Current Erlang OTP version

#]=======================================================================]
include(FindPackageHandleStandardArgs)
include(CMakePrintHelpers)

function(execute_erlang IN_ERL_EXECUTABLE OUT_ERLROOT_DIR OUT_INCLUDE_DIR OUT_LIBRARY_DIR)
  EXECUTE_PROCESS(
    COMMAND ${IN_ERL_EXECUTABLE} -noshell -eval "io:format(\"~s\", [code:lib_dir()])" -s erlang halt
    OUTPUT_VARIABLE Erlang_OTP_LIB_DIR
  )

  EXECUTE_PROCESS(
    COMMAND ${IN_ERL_EXECUTABLE} -noshell -eval "io:format(\"~s\", [code:root_dir()])" -s erlang halt
    OUTPUT_VARIABLE Erlang_OTP_ROOT_DIR
  )

  EXECUTE_PROCESS(
    COMMAND ${IN_ERL_EXECUTABLE} -noshell -eval "io:format(\"~s\",[filename:basename(code:lib_dir('erl_interface'))])" -s erlang halt
    OUTPUT_VARIABLE Erlang_EI_DIR
  )

  SET(${OUT_ERLROOT_DIR} ${Erlang_OTP_ROOT_DIR} PARENT_SCOPE)
  SET(${OUT_INCLUDE_DIR} ${Erlang_OTP_LIB_DIR}/${Erlang_EI_DIR}/include PARENT_SCOPE)
  SET(${OUT_LIBRARY_DIR} ${Erlang_OTP_LIB_DIR}/${Erlang_EI_DIR}/lib PARENT_SCOPE)
endfunction(execute_erlang)

cmake_print_variables(ERLANG_ROOT ERLANG_EI_LIB ERLANG_EI_LIB)

if (NOT ERLANG_ROOT)
  if ((NOT ERLANG_EI_LIB) OR (NOT ERLANG_EI_INCLUDE))
    SET(Erlang_BIN_PATH
      /opt/bin
      /sw/bin
      /usr/bin
      /usr/local/bin
      /opt/local/bin
    )
    FIND_PROGRAM(Erlang_RUNTIME
      NAMES erl
      PATHS ${Erlang_BIN_PATH}
    )
    execute_erlang(${Erlang_RUNTIME} Erlang_OTP_ROOT_DIR Erlang_EI_INCLUDE_DIRS Erlang_EI_LIBRARY_PATH)
  else()
    SET(Erlang_OTP_ROOT_DIR ${CMAKE_FIND_ROOT_PATH}/${ERLANG_ROOT})
    SET(Erlang_EI_LIBRARY_PATH ${ERLANG_EI_LIB})
    SET(Erlang_EI_INCLUDE_DIRS ${ERLANG_EI_INCLUDE})
  endif()
else()
  execute_erlang(${ERLANG_ROOT}/bin/erl Erlang_OTP_ROOT_DIR Erlang_EI_INCLUDE_DIRS Erlang_EI_LIBRARY_PATH)
endif()

SET(Erlang_EI_LIBRARY_NAME   libei.a)
set(Erlang_FOUND ON)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(Erlang
  Erlang_EI_LIBRARY_PATH
  Erlang_EI_INCLUDE_DIRS
  Erlang_FOUND
)

if(NOT TARGET Erlang::Erlang)
  add_library(Erlang::Erlang INTERFACE IMPORTED)
  set_target_properties(Erlang::Erlang
    PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${Erlang_OTP_ROOT_DIR}/usr/include
    )
endif()

if(NOT TARGET Erlang::EI)
  add_library(erlang_ei STATIC IMPORTED)
  set_property(TARGET erlang_ei PROPERTY
    IMPORTED_LOCATION ${Erlang_EI_LIBRARY_PATH}/${Erlang_EI_LIBRARY_NAME}
  )

  add_library(Erlang::EI INTERFACE IMPORTED)
  set_property(TARGET Erlang::EI PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES ${Erlang_EI_INCLUDE_DIRS}
  )
  set_property(TARGET Erlang::EI PROPERTY
    INTERFACE_LINK_LIBRARIES erlang_ei
  )
endif()
