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
struct glaze::meta<my_api>
{
   using T = my_api;
   static constexpr auto value = glaze::object(
      "x", &T::x, //
      "y", &T::y, //
      "z", &T::z, //
      "s", &T::s, //
      "f", &T::f, //
      "init", &T::init);
};

GLAZE_SPECIALIZE(my_api, 0, 0, 1)

std::shared_ptr<glaze::api> glaze::create_api(const std::string_view) noexcept
{
   return glaze::make_api<my_api>();
}

void tests()
{
   using namespace boost::ut;

   auto io = glaze::create_api();
   
   "bool type name"_test = [] {
      {
         std::string_view b = glaze::name<bool>;
         expect(b == "bool");
      }
      {
         std::string_view b = glaze::name<bool&>;
         expect(b == "bool&");
      }
      {
         std::string_view b = glaze::name<const bool&>;
         expect(b == "const bool&");
      }
      {
         std::string_view b = glaze::name<bool*>;
         expect(b == "bool*");
      }
      {
         std::string_view b = glaze::name<const bool*>;
         expect(b == "const bool*");
      }
   };

   "vector type name"_test = [] {
      {
         std::string_view v =
            glaze::name<std::vector<std::vector<int>*>>;
         expect(v == "std::vector<std::vector<int32_t>*>");
      }
      {
         std::string_view v = glaze::name<std::vector<float>>;
         expect(v == "std::vector<float>");
      }
   };

   "unordered type name"_test = [] {
      {
         std::string_view u =
            glaze::name<std::unordered_set<std::vector<std::string>>>;
         expect(u == "std::unordered_set<std::vector<std::string>>");
      }
      {
         std::string_view u =
            glaze::name<std::unordered_map<uint64_t, std::string_view>>;
         expect(u == "std::unordered_map<uint64_t,std::string_view>");
      }
   };

   "double type name"_test = [] {
      {
         std::string_view d = glaze::name<double*>;
         expect(d == "double*");
      }
      {
         std::string_view d = glaze::name<const double&>;
         expect(d == "const double&");
      }
   };

   "deque type name"_test = [] {
      std::string_view d = glaze::name<std::deque<bool>>;
      expect(d == "std::vector<bool>");
   };

   "span type name"_test = [] {
      std::string_view s = glaze::name<std::span<double>>;
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
         glaze::name<std::function<double(const int&, const double&)>>;
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
