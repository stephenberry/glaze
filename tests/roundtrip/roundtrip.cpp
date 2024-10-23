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

template <glz::opts Opts, class T>
void roundtrip(T& v)
{
   std::string buffer{};
   expect(not write<default_opts>(v, buffer));
   expect(not buffer.empty());
   expect(not read<default_opts>(v, buffer));
   std::string buffer2{};
   expect(not write<default_opts>(v, buffer2));
   expect(buffer == buffer2);
}

struct memory_struct
{
   std::unique_ptr<int> i = std::make_unique<int>(287);
   std::shared_ptr<double> d = std::make_shared<double>(3.14);
};

suite tests = [] {
   "my_struct"_test = [] {
      my_struct v{};
      roundtrip<default_opts>(v);
   };
   
   "memory_struct"_test = [] {
      memory_struct v{};
      roundtrip<default_opts>(v);
   };
};

int main() { return 0; }
