// Glaze Library
// For the license information refer to glaze.hpp

#include <iostream>
#include <tuple>

#include "boost/ut.hpp"
#include "glaze/api/impl.hpp"
#include "glaze/api/std/deque.hpp"
#include "glaze/api/std/unordered_set.hpp"
#include "glaze/api/std/span.hpp"

struct my_api
{
   int x = 7;
   double y = 5.5;
   std::vector<double> z = { 1.0, 2.0 };
   std::span<double> s = z;
   std::function<double(const int&, const double&)> f = [](const auto& i, const auto& d) { return i * d; };
   std::function<void()> init = []{ std::cout << "init!\n"; };
};

template <>
struct glz::meta<my_api>
{
   using T = my_api;
   static constexpr auto value = glz::object(
      "x", &T::x, //
      "y", &T::y, //
      "z", &T::z, //
      "s", &T::s, //
      "f", &T::f, //
      "init", &T::init);
   
   static constexpr std::string_view name = "my_api";
   
   static constexpr glz::version_t version{ 0, 0, 1 };
};

DLL_EXPORT glz::interface* glaze_interface() noexcept
{
   return new glz::interface{
      {"my_api", glz::make_api<my_api>},
      {"my_api2", glz::make_api<my_api>}
   };
}

void tests()
{
   using namespace boost::ut;
   
   std::unique_ptr<glz::interface> interface{ glaze_interface() };
   auto io = (*interface)["my_api"]();
   
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
         std::string_view v =
            glz::name_v<std::vector<std::vector<int>*>>;
         expect(v == "std::vector<std::vector<int32_t>*>");
      }
      {
         std::string_view v = glz::name_v<std::vector<float>>;
         expect(v == "std::vector<float>");
      }
   };

   "unordered type name"_test = [] {
      {
         std::string_view u =
            glz::name_v<std::unordered_set<std::vector<std::string>>>;
         expect(u == "std::unordered_set<std::vector<std::string>>");
      }
      {
         std::string_view u =
            glz::name_v<std::unordered_map<uint64_t, std::string_view>>;
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
      expect(d == "std::vector<bool>");
   };

   "span type name"_test = [] {
      std::string_view s = glz::name_v<std::span<double>>;
      expect(s == "std::span<double,18446744073709551615>");
   };

   "my_api type io"_test = [&] {
      auto& x = io->get<int>("/x");
      auto& y = io->get<double>("/y");
      auto& z = io->get<std::vector<double>>("/z");
      expect(x == 7);
      expect(y == 5.5);
      expect(z == std::vector<double>{1.0,2.0});
   };

   "function type name"_test = [] {
      std::string_view f =
         glz::name_v<std::function<double(const int&, const double&)>>;
      expect(f == "std::function<double(const int32_t&,const double&)");
   };

   "function type io"_test = [&] {
      int x = 7;
      double y = 5.5;
      auto& f = io->get<std::function<double(const int&, const double&)>>("/f");
      expect(f(x, y) == 38.5);
   };
}

int main()
{
   tests();
   
   return 0;
}
