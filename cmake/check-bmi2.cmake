include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)

set(GLZ_HAS_BMI2 FALSE)

set(BMI2_FLAG "")

if (MSVC)
    set(BMI2_FLAG "/arch:AVX2")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
        CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(BMI2_FLAG "-mbmi2")
endif()

# Check if the compiler supports the BMI2 flag
if (BMI2_FLAG)
    check_cxx_compiler_flag("${BMI2_FLAG}" COMPILER_SUPPORTS_BMI2_FLAG)
    if (COMPILER_SUPPORTS_BMI2_FLAG)
        # Append BMI2 flag to CMAKE_REQUIRED_FLAGS without overwriting existing flags
        set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${BMI2_FLAG}")
    else()
        message(STATUS "BMI2 instructions: not supported and not used.")
    endif()
endif()

if (BMI2_FLAG)
    # BMI2 detection
    check_cxx_source_compiles("
    #include <immintrin.h>
    int main() {
        unsigned long long src = 0xFF, mask = 0xF0;
        unsigned long long result = _pext_u64(src, mask);
        (void)result;
        return 0;
    }
    " GLZ_HAS_BMI2)

    string(REGEX REPLACE "${BMI2_FLAG}" "" CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
endif()

# Reset CMAKE_REQUIRED_FLAGS to avoid affecting other checks
if(MSVC)
string(REGEX REPLACE "/arch:AVX2" "" CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
else()
string(REGEX REPLACE "-mbmi2" "" CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
endif()

if(GLZ_HAS_BMI2)
    message(STATUS "BMI2 instructions are supported.")
    if(MSVC)
        target_compile_options(glaze_glaze PRIVATE "/arch:AVX2")
    else()
        target_compile_options(glaze_glaze PRIVATE "-mbmi2")
    endif()
    target_compile_definitions(glaze_glaze PRIVATE GLZ_HAS_BMI2)
    message(STATUS "BMI2 instructions are being used. 
    If you are cross-compiling for a 32bit platform or older processors turn glaze_ENABLE_BMI2 off.")
else()
    message(STATUS "BMI2 instructions: not supported and not used.")
endif()