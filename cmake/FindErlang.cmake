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
``Erlang_RUNTIME``
  The path to the Erlang runtime.
``Erlang_COMPILE``
  The path to the Erlang compiler.
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

SET(Erlang_BIN_PATH
  $ENV{ERLANG_HOME}/bin
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

FIND_PROGRAM(Erlang_COMPILE
  NAMES erlc
  PATHS ${Erlang_BIN_PATH}
  )

EXECUTE_PROCESS(
  COMMAND         erl -noshell -eval "io:format(\"~s\", [code:lib_dir()])" -s erlang halt
  OUTPUT_VARIABLE Erlang_OTP_LIB_DIR
  )

EXECUTE_PROCESS(
  COMMAND         erl -noshell -eval "io:format(\"~s\", [code:root_dir()])" -s erlang halt
  OUTPUT_VARIABLE Erlang_OTP_ROOT_DIR
  )

EXECUTE_PROCESS(
  COMMAND         erl -noshell -eval "io:format(\"~s\",[filename:basename(code:lib_dir('erl_interface'))])" -s erlang halt
  OUTPUT_VARIABLE Erlang_EI_DIR
  )

EXECUTE_PROCESS(
  COMMAND         erl -noshell -eval "io:format(\"~s\",[filename:basename(code:lib_dir('erts'))])" -s erlang halt
  OUTPUT_VARIABLE Erlang_ERTS_DIR
  )

EXECUTE_PROCESS(
  COMMAND         erl -noshell -eval "io:format(\"~s\", [erlang:system_info(otp_release)])" -s erlang halt
  OUTPUT_VARIABLE Erlang_OTP_VERSION
  )

SET(Erlang_EI_PATH           ${Erlang_OTP_LIB_DIR}/${Erlang_EI_DIR})
SET(Erlang_EI_INCLUDE_DIRS   ${Erlang_OTP_LIB_DIR}/${Erlang_EI_DIR}/include)
SET(Erlang_EI_LIBRARY_PATH   ${Erlang_OTP_LIB_DIR}/${Erlang_EI_DIR}/lib)

SET(Erlang_ERTS_PATH         ${Erlang_OTP_ROOT_DIR}/${Erlang_ERTS_DIR})
SET(Erlang_ERTS_INCLUDE_DIRS ${Erlang_OTP_ROOT_DIR}/${Erlang_ERTS_DIR}/include)
SET(Erlang_ERTS_LIBRARY_PATH ${Erlang_OTP_ROOT_DIR}/${Erlang_ERTS_DIR}/lib)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(
  Erlang
  DEFAULT_MSG
  Erlang_RUNTIME
  Erlang_COMPILE
  Erlang_OTP_LIB_DIR
  Erlang_OTP_ROOT_DIR
  Erlang_EI_DIR
  Erlang_ERTS_DIR
  Erlang_OTP_VERSION
  )

if(Erlang_FOUND)
  if(NOT TARGET Erlang::Erlang)
    add_library(Erlang::Erlang INTERFACE IMPORTED)
    set_target_properties(Erlang::Erlang
      PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES ${Erlang_OTP_ROOT_DIR}/usr/include
      )
  endif()

  if(NOT TARGET Erlang::ERTS)
    add_library(Erlang::ERTS STATIC IMPORTED)
    set_target_properties(Erlang::ERTS
      PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES ${Erlang_ERTS_INCLUDE_DIRS}
      IMPORTED_LOCATION             ${Erlang_ERTS_LIBRARY_PATH}/liberts.a
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
endif(Erlang_FOUND)



#[[
https://gist.github.com/JayKickliter/dd0016bd5545de466e7ca158d4b19417
How I compile Erlang from source on macOS

CFLAGS="-Og -ggdb3 -fno-omit-frame-pointer" \
CXXFLAGS="-Og -ggdb3 -fno-omit-frame-pointer" \
./configure \
      --prefix=$HOME/.local \
      --disable-silent-rules \
      --enable-dynamic-ssl-lib \
      --with-ssl=/usr/local/opt/openssl \
      --disable-hipe \
      --enable-sctp \
      --enable-shared-zlib \
      --enable-smp-support \
      --enable-threads \
      --enable-wx \
      --without-javac \
      --enable-darwin-64bit
]]