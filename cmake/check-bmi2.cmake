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
    check_cxx_compiler_flag("${BMI2_FLAG}" GLZ_HAS_BMI2)
    if (GLZ_HAS_BMI2)
        if(MSVC)
            target_compile_options(glaze_glaze INTERFACE "/arch:AVX2")
        else()
            target_compile_options(glaze_glaze INTERFACE "-mbmi2")
        endif()
        target_compile_definitions(glaze_glaze INTERFACE GLZ_HAS_BMI2)
        message(STATUS "BMI2 instructions are being used. 
        If you are cross-compiling for a 32bit platform or older processors turn glaze_ENABLE_BMI2 off.")
    else()
        set(BMI2_FLAG "")
    endif()
endif()

if (GLZ_HAS_BMI2)
    message(STATUS "BMI2 instructions: not supported and not used.")
endif()