// Glaze Library
// For the license information refer to glaze.hpp

#include "boost/ut.hpp"

#include "glaze/glaze.hpp"

using namespace boost::ut;

// These tests only do roundtrip testing so that the tests can be format agnostic.
// By changing the GLZ_TEST_FORMAT macro we are able to change what format is
// tested in CMake.

template <auto Opts, class T, class Buffer>
decltype(auto) read(T&& value, Buffer&& buffer)
{
   return glz::read<Opts>(std::forward<T>(value), std::forward<Buffer>(buffer));
}

template <auto Opts, class T, class Buffer>
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
   std::array<uint64_t, 3> arr = {1, 2, 3};
   std::map<std::string, int> map{{"one", 1}, {"two", 2}};
};

template <auto Opts, class T>
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

struct manage_x
{
   std::vector<int> x{};
   std::vector<int> y{};

   bool read_x()
   {
      y = x;
      return true;
   }

   bool write_x()
   {
      x = y;
      return true;
   }
};

template <>
struct glz::meta<manage_x>
{
   using T = manage_x;
   static constexpr auto value = object("x", manage<&T::x, &T::read_x, &T::write_x>);
};

struct manage_x_lambda
{
   std::vector<int> x{};
   std::vector<int> y{};
};

template <>
struct glz::meta<manage_x_lambda>
{
   using T = manage_x_lambda;
   static constexpr auto read_x = [](auto& s) {
      s.y = s.x;
      return true;
   };
   static constexpr auto write_x = [](auto& s) {
      s.x = s.y;
      return true;
   };
   [[maybe_unused]] static constexpr auto value = object("x", manage<&T::x, read_x, write_x>);
};

struct manage_test_struct
{
   std::string a{};
   std::string b{};

   bool read_a() { return true; }
   bool write_a() { return false; }
};

template <>
struct glz::meta<manage_test_struct>
{
   using T = manage_test_struct;
   static constexpr auto value = object("a", manage<&T::a, &T::read_a, &T::write_a>, "b", &T::b);
};

suite manage_test = [] {
   "manage"_test = [] {
      manage_x obj{.x = {1, 2, 3}, .y = {1, 2, 3}};
      std::string s{};
      expect(not glz::write<default_opts>(obj, s));
      obj = {};
      expect(not glz::read<default_opts>(obj, s));
      expect(obj.y[0] == 1);
      expect(obj.y[1] == 2);
      expect(obj.y[2] == 3);
      obj.x.clear();
      s.clear();
      expect(not glz::write<default_opts>(obj, s));
      expect(obj.x[0] == 1);
      expect(obj.x[1] == 2);
      expect(obj.x[2] == 3);
   };

   "manage_lambdas"_test = [] {
      manage_x_lambda obj{.x = {1, 2, 3}, .y = {1, 2, 3}};
      std::string s{};
      expect(not glz::write<default_opts>(obj, s));
      obj = {};
      expect(not glz::read<default_opts>(obj, s));
      expect(obj.y[0] == 1);
      expect(obj.y[1] == 2);
      expect(obj.y[2] == 3);
      obj.x.clear();
      s.clear();
      expect(not glz::write<default_opts>(obj, s));
      expect(obj.x[0] == 1);
      expect(obj.x[1] == 2);
      expect(obj.x[2] == 3);
   };

   "manage_test_struct"_test = [] {
      manage_test_struct obj{.a = "aaa", .b = "bbb"};
      std::string s{};
      const auto ec = glz::write<default_opts>(obj, s);
      expect(bool(ec));
   };
};

int main() { return 0; }
