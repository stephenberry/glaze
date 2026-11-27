# Builds one translation unit that must be rejected, and checks that the diagnostic says what it is
# supposed to say.
#
# The member diagnostics are compile-time only, so the failure itself is the observable behaviour:
# a case is a small program that has to break, annotated with the fragments its output must contain.
# The build goes through the build system rather than through a compiler command line written here,
# so the case is rejected for the reason under test and not for a flag this file forgot to pass:
# clang-linux builds with -stdlib=libc++, and MSVC takes /Zs where the others take -fsyntax-only.
#
#   BUILD_DIR - the configured build directory
#   TARGET    - the case's target in it
#   CONFIG    - the configuration to build (may be empty for single-config generators)
#   SOURCE    - the case, read for its expectations
#   GNU_TRACE - "1" to also require the fragments marked TRACE (GCC renders the instantiation trace's
#               structural template arguments; the test only relies on that where it is known to hold)
#   ASSERT_MARKER - the compiler's wording for a failed static_assert, required in the output so that
#                   a build that failed for an unrelated reason cannot pass on a quote of the message
#
# In the case file:
#   // EXPECT: <fragment>   required in the output on every compiler
#   // TRACE:  <fragment>   required only when GNU_TRACE is set

foreach(required BUILD_DIR TARGET SOURCE)
   if(NOT DEFINED ${required})
      message(FATAL_ERROR "check_diagnostic.cmake: ${required} was not set")
   endif()
endforeach()

if(NOT DEFINED ASSERT_MARKER OR "${ASSERT_MARKER}" STREQUAL "")
   message(FATAL_ERROR "check_diagnostic.cmake: ASSERT_MARKER was not set")
endif()

if(NOT EXISTS "${SOURCE}")
   message(FATAL_ERROR "check_diagnostic.cmake: no such case: ${SOURCE}")
endif()

file(READ "${SOURCE}" case_source)
string(REGEX MATCHALL "// EXPECT:[^\n]*" expected "${case_source}")
string(REGEX MATCHALL "// TRACE:[^\n]*" traced "${case_source}")

if(NOT expected)
   message(FATAL_ERROR "check_diagnostic.cmake: ${SOURCE} has no '// EXPECT:' line")
endif()

set(build_command "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target "${TARGET}")
if(CONFIG)
   list(APPEND build_command --config "${CONFIG}")
endif()

execute_process(COMMAND ${build_command}
                RESULT_VARIABLE result
                OUTPUT_VARIABLE stdout
                ERROR_VARIABLE stderr
                TIMEOUT 900)
set(output "${stdout}${stderr}")

# A non-numeric result means the build could not be run at all (bad target, timeout, missing
# generator). That is not a rejected translation unit and must not be read as one.
if(NOT result MATCHES "^-?[0-9]+$")
   message(FATAL_ERROR "check_diagnostic.cmake: could not build ${TARGET}: ${result}"
                       "\n--- build output ---\n${output}")
endif()

if(result EQUAL 0)
   message(FATAL_ERROR "${SOURCE} compiled, but it has to be rejected")
endif()

set(missing "")

# The expected fragments are sentences from the static_assert message, so on their own they already
# imply the right assert fired. Requiring the compiler's marker for a failed static_assert as well
# keeps a build that died for some other reason from ever being read as this diagnostic.
if(ASSERT_MARKER)
   string(FIND "${output}" "${ASSERT_MARKER}" marker_position)
   if(marker_position EQUAL -1)
      string(APPEND missing "\n  missing the compiler's static-assert marker: ${ASSERT_MARKER}")
   endif()
endif()

foreach(line IN LISTS expected)
   string(REPLACE "// EXPECT:" "" fragment "${line}")
   string(STRIP "${fragment}" fragment)
   string(FIND "${output}" "${fragment}" position)
   if(position EQUAL -1)
      string(APPEND missing "\n  missing: ${fragment}")
   endif()
endforeach()

if(GNU_TRACE)
   foreach(line IN LISTS traced)
      string(REPLACE "// TRACE:" "" fragment "${line}")
      string(STRIP "${fragment}" fragment)
      string(FIND "${output}" "${fragment}" position)
      if(position EQUAL -1)
         string(APPEND missing "\n  missing from the instantiation trace: ${fragment}")
      endif()
   endforeach()
endif()

if(missing)
   message(FATAL_ERROR "${SOURCE} was rejected, but the expected diagnostic is missing:${missing}"
                       "\n--- build output ---\n${output}")
endif()

message(STATUS "${SOURCE}: rejected with the expected diagnostic")
