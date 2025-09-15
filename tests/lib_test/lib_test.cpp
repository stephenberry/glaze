// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/api/lib.hpp"

#include <iostream>
#include <tuple>

#include "glaze/api/api.hpp"
#include "interface.hpp"
#include "boost/ut.hpp"

glz::iface_fn glz_iface() noexcept { return glz::make_iface<>(); }

void tests()
{
   using namespace boost::ut;
   glz::lib_loader lib(GLZ_TEST_DIRECTORY);
   auto io = lib["my_api"]();

   "bool type name"_test = [] {
      {
         std::string_view b = glz::name_v<bool>;
         expect(b == "bool");
      }
      {
         std::string_view b = glz::name_v<bool&>;
         expect(b == "bool&");
      }
      {
         std::string_view b = glz::name_v<const bool&>;
         expect(b == "const bool&");
      }
      {
         std::string_view b = glz::name_v<bool*>;
         expect(b == "bool*");
      }
      {
         std::string_view b = glz::name_v<const bool*>;
         expect(b == "const bool*");
      }
   };

   "vector type name"_test = [] {
      {
         std::string_view v = glz::name_v<std::vector<std::vector<int>*>>;
         expect(v == "std::vector<std::vector<int32_t>*>");
      }
      {
         std::string_view v = glz::name_v<std::vector<float>>;
         expect(v == "std::vector<float>");
      }
   };

   "unordered type name"_test = [] {
      {
         std::string_view u = glz::name_v<std::unordered_map<uint64_t, std::string_view>>;
         expect(u == "std::unordered_map<uint64_t,std::string_view>");
      }
   };

   "double type name"_test = [] {
      {
         std::string_view d = glz::name_v<double*>;
         expect(d == "double*");
      }
      {
         std::string_view d = glz::name_v<const double&>;
         expect(d == "const double&");
      }
   };

   "deque type name"_test = [] {
      std::string_view d = glz::name_v<std::deque<bool>>;
      expect(d == "std::deque<bool>");
   };

   "span type name"_test = [] {
      std::string_view s = glz::name_v<std::span<double>>;
      if constexpr (sizeof(size_t) == sizeof(uint64_t)) {
         expect(s == "std::span<double,18446744073709551615>");
      }
      else if constexpr (sizeof(size_t) == sizeof(uint32_t)) {
         expect(s == "std::span<double,4294967295>");
      }
   };

   "my_api type io"_test = [&] {
      auto* x = io->get<int>("/x");
      auto* y = io->get<double>("/y");
      auto* z = io->get<std::vector<double>>("/z");
      expect(*x == 7);
      expect(*y == 5.5);
      expect(*z == std::vector<double>{1.0, 2.0});
   };

   "function type name"_test = [] {
      std::string_view f = glz::name_v<std::function<double(const int&, const double&)>>;
      expect(f == "std::function<double(const int32_t&,const double&)>");
   };

   "function type io"_test = [&] {
      int x = 7;
      double y = 5.5;
      auto* f = io->get<std::function<double(const int&, const double&)>>("/f");
      expect((*f)(x, y) == 38.5);
   };
}

int main()
{
   tests();

   return 0;
}
