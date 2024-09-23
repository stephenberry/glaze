include(CheckCXXSourceCompiles)

set(GLZ_HAS_BMI2 FALSE)

# Set the required compiler flags for BMI2 detection
if(MSVC)
    set(CMAKE_REQUIRED_FLAGS "/arch:AVX2")
else()
    set(CMAKE_REQUIRED_FLAGS "-mbmi2")
endif()

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