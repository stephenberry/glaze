cmake_minimum_required(VERSION 3.31)

include(cmake/prelude.cmake)

project(
    glaze
    VERSION 8.3.0
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

if (PROJECT_IS_TOP_LEVEL)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

add_library(glaze_glaze INTERFACE)
add_library(glaze::glaze ALIAS glaze_glaze)


if (MSVC)
  # glaze_glaze is an INTERFACE library, so these target_compile_options/target_link_options
  # calls set properties that get applied to whatever *consumes* glaze::glaze, evaluated at
  # each consumer's own generate time -- not to glaze's own build. A plain CMAKE_CXX_COMPILER_ID
  # check here would instead freeze the *current* (glaze's own configure-time) compiler identity
  # into those properties, which then leaks onto every consumer regardless of what compiler that
  # consumer itself uses (see #2814): a consumer built with clang-cl, or with any compiler other
  # than the one glaze happened to be configured with, would silently inherit MSVC-only flags.
  # $<CXX_COMPILER_ID:MSVC> is a generator expression, re-evaluated per consumer, so each
  # consumer only gets these flags if *it* is actually compiling with real MSVC. (Note
  # $<CXX_COMPILER_FRONTEND_VARIANT:MSVC> would not work here: it evaluates to true for
  # clang-cl too, since matching MSVC's driver interface is the entire premise of clang-cl.)
  # The compile options narrow further to CXX, for the same reason the else() branch below
  # guards on COMPILE_LANGUAGE: a consumer that compiles C, ISPC or CUDA alongside its C++
  # should not have /Zc:lambda handed to those compilers. Link options have no per-language
  # form, so they stay on CXX_COMPILER_ID alone.
  target_compile_options(glaze_glaze INTERFACE
    # for a C++ standards compliant preprocessor, not needed for clang-cl
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Zc:preprocessor>
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Zc:lambda>)

  # Whole-program optimization changes codegen quality, never correctness, and the
  # top-level build is glaze's own test suite -- correctness checks that gain nothing
  # from it. Because glaze is header-only, every test executable pays a full link-time
  # code generation pass, and there are ~70 of them: /GL /LTCG more than doubled the
  # MSVC Release CI build (32 min of compiling versus 13 min without). The benchmarks
  # in benchmarks/ configure as their own top-level project, so they never saw these
  # flags anyway, and published numbers are measured on GCC. Opt in when profiling
  # glaze itself on MSVC.
  option(glaze_ENABLE_LTCG "Build with MSVC whole-program optimization (/GL /LTCG)" OFF)
  if(PROJECT_IS_TOP_LEVEL AND glaze_ENABLE_LTCG)
    target_compile_options(glaze_glaze INTERFACE
      $<$<AND:$<COMPILE_LANG_AND_ID:CXX,MSVC>,$<CONFIG:Release>>:/GL>
      $<$<AND:$<COMPILE_LANG_AND_ID:CXX,MSVC>,$<CONFIG:MinSizeRel>>:/GL>)
    target_link_options(glaze_glaze INTERFACE
      $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/LTCG>
      $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/INCREMENTAL:NO>
      $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:MinSizeRel>>:/LTCG>
      $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:MinSizeRel>>:/INCREMENTAL:NO>)
  endif()
else()
  # Only apply -Wno-missing-braces for C and C++ compilation to avoid breaking ISPC and other language builds
  target_compile_options(${PROJECT_NAME}_${PROJECT_NAME} INTERFACE
    $<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-Wno-missing-braces>)
endif()

set_property(TARGET ${PROJECT_NAME}_${PROJECT_NAME} PROPERTY EXPORT_NAME glaze)

target_compile_features(${PROJECT_NAME}_${PROJECT_NAME} INTERFACE cxx_std_23)
target_include_directories(
    ${PROJECT_NAME}_${PROJECT_NAME} ${warning_guard}
    INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

option(glaze_INSTALL "Generate installation rules for glaze" "${PROJECT_IS_TOP_LEVEL}")
if(glaze_INSTALL AND NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

if (glaze_DEVELOPER_MODE)
  include(cmake/dev-mode.cmake)
endif()

option(glaze_DISABLE_SIMD_WHEN_SUPPORTED
       "disable SIMD optimizations even when targets support it (e.g. AVX2)" OFF)
if(glaze_DISABLE_SIMD_WHEN_SUPPORTED)
  target_compile_definitions(glaze_glaze INTERFACE GLZ_DISABLE_SIMD)
endif()

option(glaze_DISABLE_ALWAYS_INLINE
       "disable forced inlining to reduce binary size and compilation time" OFF)
if(glaze_DISABLE_ALWAYS_INLINE)
  target_compile_definitions(glaze_glaze INTERFACE GLZ_DISABLE_ALWAYS_INLINE)
endif()

option(glaze_BUILD_EXAMPLES "Build GLAZE examples" OFF)

if(glaze_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

option(glaze_EETF_FORMAT "Enable Erlang external term format parsing" OFF)

if(glaze_EETF_FORMAT)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
  find_package(Erlang REQUIRED)
  target_link_libraries(
    glaze_glaze ${warning_guard}
    INTERFACE
      Erlang::EI
      Erlang::Erlang
  )
  target_compile_definitions(glaze_glaze INTERFACE GLZ_ENABLE_EETF)
endif(glaze_EETF_FORMAT)

option(glaze_ENABLE_SSL "Enable SSL/TLS support for HTTPS servers" OFF)

if(glaze_ENABLE_SSL)
  find_package(OpenSSL REQUIRED)
  target_link_libraries(
    glaze_glaze ${warning_guard}
    INTERFACE
      OpenSSL::SSL
      OpenSSL::Crypto
  )
  target_compile_definitions(glaze_glaze INTERFACE GLZ_ENABLE_SSL)

  # crypt32 is needed to read the Windows ROOT certificate store; see
  # detail::load_os_ca_certificates in include/glaze/net/ssl.hpp for why that is necessary.
  #
  # This is the MinGW half of the requirement. MSVC and clang-cl get crypt32 from the
  # #pragma comment(lib) in ssl.hpp, which reaches consumers that define GLZ_ENABLE_SSL
  # without going through this option; MinGW ignores that pragma, so the option names the
  # library too. Consumers who set the macro by hand under MinGW must do the same.
  #
  # Consumers that supply their own trust bundle can define GLZ_DISABLE_WINDOWS_CERT_STORE
  # to compile that code out; crypt32 (a Windows system library) stays on the link line
  # either way, since a consumer-defined macro is not visible here.
  #
  # No ${warning_guard} here: it expands to SYSTEM, which target_link_libraries rejects on
  # an INTERFACE library.
  if(WIN32)
    target_link_libraries(glaze_glaze INTERFACE crypt32)
  endif()
endif(glaze_ENABLE_SSL)

option(glaze_ENABLE_REFLECTION26 "Enable C++26 P2996 reflection (requires Bloomberg clang-p2996 or compatible compiler)" OFF)

if(glaze_ENABLE_REFLECTION26)
  target_compile_definitions(glaze_glaze INTERFACE GLZ_REFLECTION26=1)
endif(glaze_ENABLE_REFLECTION26)

# Make `size` the build-wide default optimization level. Every call site that does not
# specify a level then drops the 40KB integer table, ~16KB float pow-10 tables, and
# per-struct hash tables, without having to pass glz::opts_size{} everywhere. Per-call
# options still win (an explicit optimization_level::normal keeps the fast tables).
option(glaze_DEFAULT_OPTIMIZATION_SIZE
  "Default all (de)serialization to the size-optimized level (drops 40KB int / ~16KB float / per-struct hash tables)" OFF)

if(glaze_DEFAULT_OPTIMIZATION_SIZE)
  target_compile_definitions(glaze_glaze INTERFACE GLZ_DEFAULT_OPTIMIZATION_SIZE)
endif(glaze_DEFAULT_OPTIMIZATION_SIZE)

