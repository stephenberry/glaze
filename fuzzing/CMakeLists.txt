
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

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(fuzz_${testname} PUBLIC -fsanitize=address,undefined -fno-sanitize-recover=all )
        target_link_options(fuzz_${testname} PUBLIC -fsanitize=address,undefined -fno-sanitize-recover=all )
    else()
        target_link_libraries(fuzz_${testname} PRIVATE main)
    endif()
endmacro()

create_fuzztest(binary_reflection)
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
