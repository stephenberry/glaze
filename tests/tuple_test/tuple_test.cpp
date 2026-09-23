// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/core/tuple.hpp"

#include <array>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "glaze/glaze.hpp"
#include "glaze/util/attributes.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct empty_element
{};

struct other_empty_element
{};

suite construction = [] {
   "aggregate initialization"_test = [] {
      glz::tuple<int, double, std::string> t{1, 2.5, "three"};
      expect(glz::get<0>(t) == 1);
      expect(glz::get<1>(t) == 2.5);
      expect(glz::get<2>(t) == "three");
   };

   "class template argument deduction decays"_test = [] {
      glz::tuple t{1, 2.5, "three"};
      static_assert(std::same_as<decltype(t), glz::tuple<int, double, const char*>>);
   };

   "deduction unwraps reference_wrapper"_test = [] {
      int i = 7;
      glz::tuple t{std::ref(i)};
      static_assert(std::same_as<decltype(t), glz::tuple<int&>>);
      glz::get<0>(t) = 9;
      expect(i == 9);
   };

   "default construction value initializes"_test = [] {
      glz::tuple<int, double> t{};
      expect(glz::get<0>(t) == 0);
      expect(glz::get<1>(t) == 0.0);
   };

   "empty tuple"_test = [] {
      glz::tuple<> t{};
      static_assert(glz::tuple_size_v<glz::tuple<>> == 0);
      expect(std::is_empty_v<glz::tuple<>>);
      (void)t;
   };

   "copy and move"_test = [] {
      glz::tuple<std::string, std::vector<int>> a{"hello", std::vector<int>{1, 2, 3}};
      auto b = a;
      expect(glz::get<0>(b) == "hello");
      auto c = std::move(a);
      expect(glz::get<0>(c) == "hello");
      expect(glz::get<1>(c).size() == 3);
   };

   "move only elements"_test = [] {
      glz::tuple<std::unique_ptr<int>, int> t{std::make_unique<int>(5), 3};
      auto moved = std::move(t);
      expect(*glz::get<0>(moved) == 5);
      expect(glz::get<1>(moved) == 3);
   };
};

suite traits = [] {
   "tuple_size"_test = [] {
      static_assert(glz::tuple_size_v<glz::tuple<int, double, char>> == 3);
      static_assert(glz::tuple_size_v<const glz::tuple<int, double, char>> == 3);
      static_assert(glz::tuple_size_v<std::tuple<int, double>> == 2);
      static_assert(glz::tuple_size_v<std::array<int, 4>> == 4);
   };

   "tuple_element"_test = [] {
      using T = glz::tuple<int, double&, const char*>;
      static_assert(std::same_as<glz::tuple_element_t<0, T>, int>);
      static_assert(std::same_as<glz::tuple_element_t<1, T>, double&>);
      static_assert(std::same_as<glz::tuple_element_t<2, T>, const char*>);
      static_assert(std::same_as<glz::tuple_element_t<1, std::tuple<int, char>>, char>);
      static_assert(std::same_as<glz::tuple_element_t<0, std::pair<int, char>>, int>);
      static_assert(std::same_as<glz::tuple_element_t<1, std::pair<int, char>>, char>);
   };

   "repeated element types"_test = [] {
      glz::tuple<int, int, int> t{1, 2, 3};
      expect(glz::get<0>(t) == 1);
      expect(glz::get<1>(t) == 2);
      expect(glz::get<2>(t) == 3);
   };

   "GLZ_NO_UNIQUE_ADDRESS has a layout effect"_test = [] {
      // Guards the attribute selection in glaze/util/attributes.hpp. The MSVC ABI accepts the
      // standard [[no_unique_address]] but ignores it, so picking the wrong spelling there
      // costs storage silently rather than failing to build. This holds on every ABI.
      struct probe
      {
         GLZ_NO_UNIQUE_ADDRESS empty_element e;
         int i;
      };
      static_assert(sizeof(probe) == sizeof(int));
   };

   "empty elements occupy no space"_test = [] {
   // Empty element types collapse into their element bases. Two elements of the *same* empty
   // type still need distinct addresses, so the check uses two different ones.
   //
   // The MSVC ABI does not apply the empty base optimization across multiple bases, so it
   // keeps a byte (plus alignment) per empty element there. That is a property of the ABI
   // rather than of this tuple: the previous implementation, which used the same
   // one-base-per-element layout, measured identically.
#if defined(_MSC_VER)
      static_assert(sizeof(glz::tuple<empty_element, other_empty_element>) <= 2);
      static_assert(sizeof(glz::tuple<empty_element, other_empty_element, int>) <= 2 * sizeof(int));
#else
      static_assert(sizeof(glz::tuple<empty_element, other_empty_element>) == 1);
      static_assert(sizeof(glz::tuple<empty_element, other_empty_element, int>) == sizeof(int));
#endif
   };

   "satisfies glz::tuple_t"_test = [] { static_assert(glz::tuple_t<glz::tuple<int, std::string>>); };
};

suite element_access = [] {
   "get preserves value category and constness"_test = [] {
      glz::tuple<int, std::string> t{1, "a"};
      static_assert(std::same_as<decltype(glz::get<0>(t)), int&>);
      static_assert(std::same_as<decltype(glz::get<0>(std::as_const(t))), const int&>);
      static_assert(std::same_as<decltype(glz::get<0>(std::move(t))), int&&>);
   };

   "get on reference elements never adds const"_test = [] {
      int i = 3;
      std::string s = "x";
      const glz::tuple<int&, std::string&> t{i, s};
      static_assert(std::same_as<decltype(glz::get<0>(t)), int&>);
      glz::get<0>(t) = 5;
      expect(i == 5);
   };

   "get works on std::array"_test = [] {
      std::array<int, 3> a{1, 2, 3};
      expect(glz::get<1>(a) == 2);
      glz::get<1>(a) = 7;
      expect(a[1] == 7);
   };

   "get is usable at compile time"_test = [] {
      constexpr glz::tuple<int, double> t{4, 0.5};
      static_assert(glz::get<0>(t) == 4);
      static_assert(glz::get<1>(t) == 0.5);
   };
};

suite assignment = [] {
   "assign from another glz::tuple"_test = [] {
      glz::tuple<int, double> a{1, 2.0};
      glz::tuple<short, float> b{3, 4.0f};
      a = b;
      expect(glz::get<0>(a) == 3);
      expect(glz::get<1>(a) == 4.0);
   };

   "assign from std::tuple"_test = [] {
      glz::tuple<int, std::string> a{};
      a = std::tuple<int, std::string>{5, "five"};
      expect(glz::get<0>(a) == 5);
      expect(glz::get<1>(a) == "five");
   };

   "assign through a tie"_test = [] {
      int x{};
      std::string y{};
      glz::tie(x, y) = glz::tuple<int, std::string>{8, "eight"};
      expect(x == 8);
      expect(y == "eight");
   };

   "member assign"_test = [] {
      glz::tuple<int, std::string> t{};
      t.assign(2, "two");
      expect(glz::get<0>(t) == 2);
      expect(glz::get<1>(t) == "two");
   };
};

suite comparison = [] {
   "equality"_test = [] {
      expect(glz::tuple{1, 2.0, 'c'} == glz::tuple{1, 2.0, 'c'});
      expect(glz::tuple{1, 2.0, 'c'} != glz::tuple{1, 3.0, 'c'});
      expect(glz::tuple<>{} == glz::tuple<>{});
   };

   "ordering"_test = [] {
      expect(glz::tuple{1, 2} < glz::tuple{1, 3});
      expect(glz::tuple{2, 0} > glz::tuple{1, 9});
      expect(glz::tuple{1, 2} <= glz::tuple{1, 2});
      expect((glz::tuple{1, 2} <=> glz::tuple{1, 2}) == std::strong_ordering::equal);
   };

   "reference elements compare their referents"_test = [] {
      int a = 1, b = 1;
      expect(glz::tie(a) == glz::tie(b));
      b = 2;
      expect(glz::tie(a) < glz::tie(b));
   };

   "comparison is constexpr"_test = [] { static_assert(glz::tuple{1, 2} < glz::tuple{1, 3}); };
};

suite algorithms = [] {
   "for_each visits in declaration order"_test = [] {
      glz::tuple<int, int, int> t{1, 2, 3};
      std::string order{};
      t.for_each([&](auto& v) { order += char('0' + v); });
      expect(order == "123");
   };

   "for_each can mutate"_test = [] {
      glz::tuple<int, int> t{1, 2};
      t.for_each([](auto& v) { v *= 10; });
      expect(glz::get<0>(t) == 10);
      expect(glz::get<1>(t) == 20);
   };

   "any short circuits"_test = [] {
      glz::tuple<int, int, int> t{1, 2, 3};
      int visited = 0;
      const bool found = t.any([&](auto v) {
         ++visited;
         return v == 2;
      });
      expect(found);
      expect(visited == 2);
   };

   "any on no match"_test = [] {
      glz::tuple<int, int> t{1, 2};
      expect(not t.any([](auto v) { return v == 9; }));
   };

   "all"_test = [] {
      glz::tuple<int, int> t{2, 4};
      expect(t.all([](auto v) { return v % 2 == 0; }));
      expect(not t.all([](auto v) { return v > 2; }));
   };

   "empty tuple algorithms"_test = [] {
      glz::tuple<> t{};
      t.for_each([](auto&&) { expect(false); });
      expect(not t.any([](auto&&) { return true; }));
      expect(t.all([](auto&&) { return false; }));
   };

   "apply"_test = [] {
      const auto sum = glz::apply([](int a, double b, int c) { return a + b + c; }, glz::tuple{1, 2.5, 3});
      expect(sum == 6.5);
   };

   "apply forwards rvalues"_test = [] {
      auto taken = glz::apply([](std::unique_ptr<int> p) { return *p; },
                              glz::tuple<std::unique_ptr<int>>{std::make_unique<int>(11)});
      expect(taken == 11);
   };
};

suite factories = [] {
   "make_tuple decays"_test = [] {
      int i = 3;
      auto t = glz::make_tuple(i, std::string("s"), std::ref(i));
      static_assert(std::same_as<decltype(t), glz::tuple<int, std::string, int&>>);
      expect(glz::get<0>(t) == 3);
   };

   "forward_as_tuple keeps references"_test = [] {
      int i = 3;
      auto t = glz::forward_as_tuple(i, 4);
      static_assert(std::same_as<decltype(t), glz::tuple<int&, int&&>>);
      glz::get<0>(t) = 5;
      expect(i == 5);
   };

   "tie"_test = [] {
      int a{};
      std::string b{};
      auto t = glz::tie(a, b);
      static_assert(std::same_as<decltype(t), glz::tuple<int&, std::string&>>);
      glz::get<0>(t) = 42;
      expect(a == 42);
   };

   "tuple_cat"_test = [] {
      auto t = glz::tuple_cat(glz::tuple{1, 2.5}, glz::tuple<>{}, glz::tuple{std::string("x"), 'c'});
      static_assert(std::same_as<decltype(t), glz::tuple<int, double, std::string, char>>);
      expect(glz::get<0>(t) == 1);
      expect(glz::get<1>(t) == 2.5);
      expect(glz::get<2>(t) == "x");
      expect(glz::get<3>(t) == 'c');
   };

   "tuple_cat of nothing"_test = [] { static_assert(std::same_as<decltype(glz::tuple_cat()), glz::tuple<>>); };

   "tuple_cat preserves element types"_test = [] {
      int i = 1;
      auto t = glz::tuple_cat(glz::tie(i), glz::tuple{2.0});
      static_assert(std::same_as<decltype(t), glz::tuple<int&, double>>);
      glz::get<0>(t) = 4;
      expect(i == 4);
   };

   "tuple_cat moves from rvalues"_test = [] {
      auto t = glz::tuple_cat(glz::tuple<std::unique_ptr<int>>{std::make_unique<int>(6)}, glz::tuple{7});
      expect(*glz::get<0>(t) == 6);
      expect(glz::get<1>(t) == 7);
   };

   "convert"_test = [] {
      struct target
      {
         int a{};
         double b{};
      };
      const target v = glz::tuplet::convert{glz::tuple{1, 2.5}};
      expect(v.a == 1);
      expect(v.b == 2.5);
   };
};

suite serialization = [] {
   "json round trip"_test = [] {
      glz::tuple<int, std::string, double> t{1, "two", 3.5};
      const auto json = glz::write_json(t).value();
      expect(json == R"([1,"two",3.5])") << json;

      glz::tuple<int, std::string, double> parsed{};
      expect(not glz::read_json(parsed, json));
      expect(parsed == t);
   };

   "beve round trip"_test = [] {
      glz::tuple<int, std::string, double> t{1, "two", 3.5};
      std::string buffer{};
      expect(not glz::write_beve(t, buffer));

      glz::tuple<int, std::string, double> parsed{};
      expect(not glz::read_beve(parsed, buffer));
      expect(parsed == t);
   };

   "type name"_test = [] { expect(glz::name_v<glz::tuple<int32_t, double>> == "glz::tuple<int32_t,double>"); };
};

int main() { return 0; }
