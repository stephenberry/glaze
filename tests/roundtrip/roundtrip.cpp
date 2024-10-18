// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include <ut/ut.hpp>

#include "glaze/glaze.hpp"

using namespace ut;

// These tests only do roundtrip testing so that the tests can be format agnostic.
// By changing the GLZ_DEFAULT_FORMAT macro we are able to change what format is
// tested in CMake.

int main() { return 0; }
