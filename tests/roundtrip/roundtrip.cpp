// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include <ut/ut.hpp>

#include "glaze/glaze.hpp"

using namespace ut;

// These tests only do roundtrip testing so that the tests can be format agnostic.
// By changing the GLZ_TEST_FORMAT macro we are able to change what format is
// tested in CMake.

template <glz::opts Opts, class T, class Buffer>
decltype(auto) read(T&& value, Buffer&& buffer)
{
   return glz::read<Opts>(std::forward<T>(value), std::forward<Buffer>(buffer));
}

template <glz::opts Opts, class T, class Buffer>
decltype(auto) write(T&& value, Buffer&& buffer)
{
   return glz::write<Opts>(std::forward<T>(value), std::forward<Buffer>(buffer));
}

static constexpr glz::opts default_opts{.format = GLZ_TEST_FORMAT};

struct my_struct
{
  int i = 287;
  double d = 3.14;
  std::string hello = "Hello World";
  std::array<uint64_t, 3> arr = { 1, 2, 3 };
  std::map<std::string, int> map{{"one", 1}, {"two", 2}};
};

suite my_struct_test = [] {
   "my_struct"_test = [] {
      my_struct obj{};
      std::string buffer{};
      expect(not write<default_opts>(obj, buffer));
      expect(not read<default_opts>(obj, buffer));
   };
};

int main() { return 0; }
