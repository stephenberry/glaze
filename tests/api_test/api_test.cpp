// Glaze Library
// For the license information refer to glaze.hpp

#include <iostream>
#include <tuple>

#include "glaze/api/impl.hpp"
#include "glaze/api/std/deque.hpp"
#include "glaze/api/std/span.hpp"
#include "glaze/api/std/unordered_set.hpp"
#include "ut/ut.hpp"

struct my_struct
{
   int i = 42;
};

struct my_api
{
   int x = 7;
   double y = 5.5;
   std::vector<double> z = {1.0, 2.0};
   int* x_ptr = &x;
   std::unique_ptr<double> uptr = std::make_unique<double>(5.5);
   std::span<double> s = z;
   std::function<double(const int&, const double&)> f = [](const auto& i, const auto& d) { return i * d; };
   std::function<void()> init = [] { std::cout << "init!\n"; };
   std::function<int(const my_struct&)> my_struct_i = [](const my_struct& s) { return s.i; };
   int func() { return 5; };
   const int& func_ref()
   {
      static int a = 5;
      return a;
   };
   void inc(int& a) { ++a; };
   double sum(double a, double b) { return a + b; };
   double sum_lref(const double& a, const double& b) { return a + b; };
   double sum_rref(double&& a, double&& b) { return a + b; };
};

template <>
struct glz::meta<my_api>
{
   using T = my_api;
   static constexpr auto value = glz::object("x", &T::x, //
                                             "y", &T::y, //
                                             "z", &T::z, //
                                             "x_ptr", &T::x_ptr, //
                                             "uptr", &T::uptr, //
                                             "s", &T::s, //
                                             "f", &T::f, //
                                             "init", &T::init, //
                                             &T::my_struct_i, //
                                             "func", &T::func, //
                                             "func_ref", &T::func_ref, //
                                             "inc", &T::inc, //
                                             "sum", &T::sum, //
                                             "sum_lref", &T::sum_lref, //
                                             "sum_rref", &T::sum_rref //
   );

   static constexpr std::string_view name = "my_api";

   static constexpr glz::version_t version{0, 0, 1};
};

struct my_api2
{
   int x = 7;
   double y = 5.5;
   std::vector<double> z = {1.0, 2.0};
   std::span<double> s = z;
   std::function<double(const int&, const double&)> f = [](const auto& i, const auto& d) { return i * d; };
   std::function<void()> init = [] { std::cout << "init!\n"; };
   int func() { return 5; };
};

template <>
struct glz::meta<my_api2>
{
   using T = my_api2;
   static constexpr auto value = glz::object("x", &T::x, //
                                             "y", &T::y, //
                                             "z", &T::z, //
                                             "s", &T::s, //
                                             "f", &T::f, //
                                             "init", &T::init, //
                                             "func", &T::func //
   );

   static constexpr std::string_view name = "my_api2";

   static constexpr glz::version_t version{0, 0, 1};
};

glz::iface_fn glz_iface() noexcept { return glz::make_iface<my_api, my_api2>(); }

void tests()
{
   using namespace ut;

   std::shared_ptr<glz::iface> iface{glz_iface()()};
   auto io = (*iface)["my_api"]();
   auto io2 = (*iface)["my_api2"]();

   "calling functions"_test = [&] {
      my_struct obj{};
      auto my_struct_i = io->get_fn<std::function<int(const my_struct&)>>("/my_struct_i");
      expect(my_struct_i.value()(obj) == 42);
      // TODO: Fix this
      // expect(42 == io->call<int>("/func", obj));

      auto func = io->get_fn<std::function<int()>>("/func");
      expect(func.value()() == 5);
      expect(5 == io->call<int>("/func"));

      // auto func_ref = io->get_fn<std::function<const int&()>>("/func_ref");
      // expect(func_ref() == 5);
      expect(5 == io->call<const int&>("/func_ref"));

      auto sum = io->get_fn<std::function<double(double, double)>>("/sum");
      expect(sum.value()(7, 2) == 9);
      expect(9 == io->call<double>("/sum", 7.0, 2.0));

      auto sum_lref = io->get_fn<std::function<double(const double&, const double&)>>("/sum_lref");
      expect(sum_lref.value()(7, 2) == 9);
      expect(9 == io->call<double>("/sum_lref", 7.0, 2.0));

      auto sum_rref = io->get_fn<std::function<double(double&&, double&&)>>("/sum_rref");
      expect(sum_rref.value()(7, 2) == 9);
      expect(9 == io->call<double>("/sum_rref", 7.0, 2.0));

      auto inc = io->get_fn<std::function<void(int&)>>("/inc");
      int i = 0;
      inc.value()(i);
      expect(i == 1);
      io->call<void>("/inc", i);
      expect(i == 2);

      auto f = io->get_fn<std::function<double(const int&, const double&)>>("/f");
      expect(f.value()(7, 2) == 14);
   };

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
         std::string_view u = glz::name_v<std::unordered_set<std::vector<std::string>>>;
         expect(u == "std::unordered_set<std::vector<std::string>>");
      }
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

   "tuple type name"_test = [] {
      std::string_view t = glz::name_v<std::tuple<double, std::string>>;
      expect(t == "std::tuple<double,std::string>");
   };

   "my_api type io"_test = [&] {
      auto* x = io->get<int>("/x");
      auto* y = io->get<double>("/y");
      auto* z = io->get<std::vector<double>>("/z");
      expect(*x == 7);
      expect(*y == 5.5);
      expect(*z == std::vector<double>{1.0, 2.0});
   };

   "my_api type ptr unwrap io"_test = [&] {
      auto* x = io->get<int>("/x_ptr");
      auto* y = io->get<double>("/uptr");
      expect(*x == 7);
      expect(*y == 5.5);
   };

   "function type name"_test = [] {
      std::string_view f = glz::name_v<std::function<double(const int&, const double&)>>;
      expect(f == "std::function<double(const int32_t&,const double&)>");

      f = glz::name_v<std::function<int(const my_struct&)>>;
      expect(f == R"(std::function<int32_t(const my_struct&)>)") << f;
   };

   "function type io"_test = [&] {
      int x = 7;
      double y = 5.5;
      auto* f = io->get<std::function<double(const int&, const double&)>>("/f");
      expect((*f)(x, y) == 38.5);
   };

   "my_api binary io"_test = [&] {
      *io->get<int>("/x") = 1;
      *io2->get<int>("/x") = 5;
      std::string buffer{};
      io2->write(glz::BEVE, "", buffer);
      io->read(glz::BEVE, "", buffer);
      expect(*io->get<int>("/x") == 5);
   };
}

int main()
{
   tests();

   return 0;
}
