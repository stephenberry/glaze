

#include "glaze/api/impl.hpp"

#include "glaze/api/std/deque.hpp"
#include "glaze/api/std/unordered_set.hpp"
#include "glaze/api/std/span.hpp"
#include <iostream>
#include <tuple>

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
      "s", &T::s);
};

GLAZE_SPECIALIZE(my_api, 0, 0, 1)

std::shared_ptr<glaze::api> glaze::create_api(const std::string_view) noexcept
{
   return glaze::make_impl<my_api>();
}

void tests()
{
   auto io = glaze::create_api();
   
   try {
      std::cout << glaze::name<bool> << '\n';
      std::cout << glaze::name<bool&> << '\n';
      std::cout << glaze::name<const bool&> << '\n';
      std::cout << glaze::name<bool*> << '\n';
      std::cout << glaze::name<const bool*> << '\n';
      std::cout << glaze::name<std::vector<std::vector<int>*>> << '\n';
      std::cout << glaze::name<std::unordered_set<std::vector<std::string>>> << '\n';
      std::cout << glaze::name<double*> << '\n';
      std::cout << glaze::name<std::vector<float>> << '\n';
      std::cout << glaze::name<std::deque<bool>> << '\n';
      std::cout << glaze::name<std::unordered_map<uint64_t, std::string_view>> << '\n';
      std::cout << glaze::name<const double&> << '\n';
      std::cout << glaze::name<std::span<double>> << '\n';
      
      std::cout << io->get<int>("/x") << '\n';
      std::cout << io->get<double>("/y") << '\n';
      auto& v = io->get<std::vector<double>>("/z");
      std::cout << '{' << v[0] << ',' << v[1] << "}\n";
      
      std::cout << glaze::name<std::function<double(const int&, const double&)>> << '\n';
      
      /*int x = 7;
      double y = 5.5;
      std::cout << io->call<double>("f", x, y) << '\n';
      io->call("init");*/
   }
   catch (const std::exception& e) {
      std::cout << e.what() << '\n';
   }
}

int main()
{
   tests();
   
   return 0;
}
