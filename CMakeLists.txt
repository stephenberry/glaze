cmake_minimum_required(VERSION 4.4) 

set(CACHE{CMAKE_EXPERIMENTAL_CXX_IMPORT_STD} VALUE "f35a9ac6-8463-4d38-8eec-5d6008153e7d")

include(cmake/prelude.cmake)

project(
    glaze
    VERSION 8.1.0
    LANGUAGES CXX
)

option(GLAZE_ENABLE_MODULES "Enable modules with import std" ON) # or GLAZE_GENERATE_HEADERS ??

if (GLAZE_ENABLE_MODULES)
   set(CMAKE_CXX_MODULE_STD 1)
endif()

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

if (PROJECT_IS_TOP_LEVEL)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

if(GLAZE_ENABLE_MODULES)
  add_library(glaze_glaze STATIC)
  target_compile_definitions(glaze_glaze PRIVATE GLZ_BUILDING_MODULES)
  if (PROJECT_IS_TOP_LEVEL)
    set_target_properties(glaze_glaze PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
  set(GLAZE_LIB_TYPE PUBLIC)
else()
  add_library(glaze_glaze INTERFACE)
  set(GLAZE_LIB_TYPE INTERFACE)
endif()
add_library(glaze::glaze ALIAS glaze_glaze)

if (MSVC)
  string(REGEX MATCH "\/cl(.exe)?$" matched_cl ${CMAKE_CXX_COMPILER})
  if (matched_cl)
    # for a C++ standards compliant preprocessor, not needed for clang-cl
    target_compile_options(glaze_glaze INTERFACE "/Zc:preprocessor" /permissive- /Zc:lambda)
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
        $<$<CONFIG:Release>:/GL>
        $<$<CONFIG:MinSizeRel>:/GL>)
      target_link_options(glaze_glaze INTERFACE
        $<$<CONFIG:Release>:/LTCG /INCREMENTAL:NO>
        $<$<CONFIG:MinSizeRel>:/LTCG /INCREMENTAL:NO>)
    endif()
  endif()
else()
  # Only apply -Wno-missing-braces for C and C++ compilation to avoid breaking ISPC and other language builds
  target_compile_options(${PROJECT_NAME}_${PROJECT_NAME} INTERFACE
    $<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-Wno-missing-braces>)
endif()

set_property(TARGET ${PROJECT_NAME}_${PROJECT_NAME} PROPERTY EXPORT_NAME glaze)

target_compile_features(glaze_glaze ${GLAZE_LIB_TYPE} cxx_std_23)
target_include_directories(
    glaze_glaze ${warning_guard}
    PUBLIC
      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/modules>"
)

include(cmake/glaze-asio.cmake)
glaze_setup_asio()
target_link_libraries(glaze_glaze PRIVATE "$<BUILD_INTERFACE:glaze::asio>")

include(cmake/modules-files.cmake)

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

option(glaze_EETF_FORMAT "Enable Erlang external term format parsing" OFF) # TODO: Convert

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
    ${GLAZE_LIB_TYPE}
      OpenSSL::SSL
      OpenSSL::Crypto
  )
  target_compile_definitions(glaze_glaze ${GLAZE_LIB_TYPE} GLZ_ENABLE_SSL)

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

