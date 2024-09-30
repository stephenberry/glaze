include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)

set(AVX2_SUPPORTED FALSE)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|Intel")
    check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2_FLAG)
    if (COMPILER_SUPPORTS_AVX2_FLAG)
        set(AVX2_SUPPORTED TRUE)
    endif()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    check_cxx_compiler_flag("/arch:AVX2" COMPILER_SUPPORTS_AVX2_FLAG)
    if (COMPILER_SUPPORTS_AVX2_FLAG)
        set(AVX2_SUPPORTED TRUE)
    endif()
endif()

if (AVX2_SUPPORTED)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|Intel")
        set(CMAKE_REQUIRED_FLAGS "-mavx2")
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set(CMAKE_REQUIRED_FLAGS "/arch:AVX2")
    endif()

    # Test code that uses AVX2 intrinsics
    check_cxx_source_compiles("
        #if defined(_MSC_VER)
        #include <intrin.h>
        #else
        #include <immintrin.h>
        #endif

        int main() {
            __m256i a = _mm256_set1_epi32(0);
            return 0;
        }
    " AVX2_CODE_COMPILES)

    if (AVX2_CODE_COMPILES)
        message(STATUS "Glaze: using AVX2 intrinsics. Set glaze_ENABLE_AVX2 to OFF to avoid use.")
    else()
        set(AVX2_SUPPORTED FALSE)
        message(STATUS "Glaze: AVX2 not supported/not used.")
    endif()

    # Reset CMAKE_REQUIRED_FLAGS
    set(CMAKE_REQUIRED_FLAGS "")
endif()

if (AVX2_SUPPORTED)
    if(MSVC)
        target_compile_options(glaze_glaze INTERFACE "/arch:AVX2")
    else()
        target_compile_options(glaze_glaze INTERFACE "-mavx2")
    endif()
    target_compile_definitions(glaze_glaze INTERFACE GLZ_USE_AVX2)
endif()