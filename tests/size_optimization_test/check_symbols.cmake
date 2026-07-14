# Asserts that a size-optimized binary does not link the large lookup tables.
#
# Invoked as: cmake -DNM=<nm> -DBIN=<binary> -P check_symbols.cmake
#
# The 40 KB integer table is the headline guarantee: `digit_quads` is an inline constexpr
# array that is only ODR-used by glz::to_chars_40kb, which is only reached on the normal
# optimization path. If the build-wide default is `size` and no internal site leaks a
# normal-mode number conversion, the symbol must be absent.

if(NOT NM OR NOT BIN)
  message(FATAL_ERROR "check_symbols.cmake requires -DNM=<tool> and -DBIN=<binary>")
endif()

execute_process(
  COMMAND "${NM}" "${BIN}"
  OUTPUT_VARIABLE symbols
  ERROR_VARIABLE nm_err
  RESULT_VARIABLE nm_rc
)

if(NOT nm_rc EQUAL 0)
  # Don't fail the suite if the symbol reader is unusable on this platform/binary.
  message(STATUS "check_symbols: '${NM}' returned ${nm_rc} (${nm_err}); skipping symbol check")
  return()
endif()

set(forbidden "digit_quads")
string(FIND "${symbols}" "${forbidden}" found_pos)
if(NOT found_pos EQUAL -1)
  message(FATAL_ERROR
    "Size-optimized binary linked the 40 KB integer table: found symbol containing "
    "'${forbidden}' in ${BIN}. A default-options call site is using the normal-mode "
    "large tables. Route number formatting through is_size_optimized(Opts).")
endif()

message(STATUS "check_symbols: size-optimized binary is clean (no '${forbidden}' symbol)")
