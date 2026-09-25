
# this library provides main() for compilers that do not have libfuzzer (everyone except clang)
# so the source code is compiled regardless of compiler, which prevents code rot.
# also, the library provides a way to invoke the fuzzer on external data which is
# useful to run the fuzz corpus through code also when not using clang.
add_library(main STATIC main.cpp)
target_compile_features(main PRIVATE cxx_std_23)

# see if libfuzzer is supported - this gets more complicated than just checking
# for clang, because apple clang does not seem to support it and windows also
# support clang nowadays (and perhaps libfuzzer)

include(CheckSourceCompiles)
set(CMAKE_REQUIRED_LINK_OPTIONS -fsanitize=fuzzer)
check_source_compiles(CXX "
    #include <cstddef>
    #include <cstdint>
    extern \"C\" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
    { return 0;}
    " haslibfuzzer)
# The probe above is the only thing that should see that flag: left set, it would be added to
# every later check in this directory, and a check for anything else would then be asking about
# libfuzzer too.
unset(CMAKE_REQUIRED_LINK_OPTIONS)

# ASan and UBSan are not part of every toolchain that parses the flags. MinGW's GCC accepts
# -fsanitize=address and then fails at link with "cannot find -lasan", because libsanitizer is
# not built for that target and MSYS2 ships no sanitizer runtime; the flags would turn every
# fuzzer below into a link error. Ask the toolchain to link what it just compiled, the way
# libfuzzer is asked above, so a toolchain without a sanitizer runtime builds an uninstrumented
# fuzzer instead of not building one at all.
set(CMAKE_REQUIRED_LINK_OPTIONS -fsanitize=address,undefined -fno-sanitize-recover=all)
check_source_compiles(CXX "int main() { return 0; }" has_sanitizer_runtime)
unset(CMAKE_REQUIRED_LINK_OPTIONS)
# Reported only where the flags would have been applied: every other compiler reached the
# fallback that links main() before this probe existed, and still does.
if(NOT has_sanitizer_runtime AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    message(STATUS "glaze: this toolchain cannot link -fsanitize=address,undefined, "
                   "so the fuzz targets build uninstrumented")
endif()


macro(create_fuzztest testname)
    add_executable(fuzz_${testname} ${testname}.cpp)
    target_link_libraries(fuzz_${testname} PRIVATE glaze::glaze)

    if(haslibfuzzer)
        target_compile_options(fuzz_${testname} PUBLIC -fsanitize=fuzzer)
        target_link_options(fuzz_${testname} PUBLIC -fsanitize=fuzzer)
    else()
        # supply a separate main
        target_link_libraries(fuzz_${testname} PRIVATE main)
    endif()

    if(MSVC)
        target_link_libraries(fuzz_${testname} PRIVATE main)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
        # Nested rather than folded into the condition above: having a runtime is a property of
        # the toolchain, not of the compiler family, and a family that lacks one must still take
        # this branch instead of falling through to a second main() beside libFuzzer's own.
        if(has_sanitizer_runtime)
            target_compile_options(fuzz_${testname} PUBLIC -fsanitize=address,undefined -fno-sanitize-recover=all)
            target_link_options(fuzz_${testname} PUBLIC -fsanitize=address,undefined -fno-sanitize-recover=all)
        endif()
    else()
        target_link_libraries(fuzz_${testname} PRIVATE main)
    endif()
endmacro()

create_fuzztest(binary_reflection)
create_fuzztest(cbor_reflection)
create_fuzztest(cbor_roundtrip_floating)
create_fuzztest(cbor_roundtrip_int)
create_fuzztest(cbor_roundtrip_string)
create_fuzztest(msgpack_reflection)
create_fuzztest(msgpack_roundtrip_floating)
create_fuzztest(msgpack_roundtrip_int)
create_fuzztest(msgpack_roundtrip_string)
create_fuzztest(csv_parsing)
create_fuzztest(json_generic)
create_fuzztest(json_jmespath)
create_fuzztest(json_minify)
create_fuzztest(json_prettify)
create_fuzztest(json_reflection)
create_fuzztest(json_roundtrip_floating)
create_fuzztest(json_roundtrip_int)
create_fuzztest(json_roundtrip_string)
create_fuzztest(json_with_comments)
create_fuzztest(jsonb_reflection)
create_fuzztest(yaml_generic)
create_fuzztest(repe_registry)
create_fuzztest(jsonrpc_registry)

macro(create_exhaustive_test testname)
    add_executable(${testname} ${testname}.cpp)
    target_link_libraries(${testname} PRIVATE glaze::glaze)
    # do not set sanitizers here - CI needs to run as fast as possible,
    # manual running may want sanitizers and debug.
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        #target_compile_options(${testname} PRIVATE -O2)
        #target_compile_options(${testname} PRIVATE -fsanitize=address,undefined  -fno-sanitize-recover=all )
        #target_link_options(${testname} PRIVATE -fsanitize=address,undefined  -fno-sanitize-recover=all )
    endif()
endmacro()

create_exhaustive_test(json_exhaustive_roundtrip_int)
create_exhaustive_test(json_exhaustive_roundtrip_float)
