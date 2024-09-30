include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)

if(MSVC)
    target_compile_options(glaze_glaze INTERFACE "/arch:AVX2")
else()
    target_compile_options(glaze_glaze INTERFACE "-mavx2")
endif()
target_compile_definitions(glaze_glaze INTERFACE GLZ_USE_AVX)