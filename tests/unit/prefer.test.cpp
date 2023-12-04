#include <glaze/json/prefer.hpp>
//
#include <algorithm>
#include <concepts>
#include <iterator>
#include <list>
#include <map>
#include <ostream>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
//
#include <boost/ut.hpp>

namespace ut = boost::ut;
using ut::operator|;
using ut::operator>>;

template <typename T>
consteval void assert_pair_adapter_types() noexcept
{
   static_assert(!std::is_reference_v<T>, "Assertions internally test reference variaions. Provide value type");
   static_assert(glz::detail::tuple_t<T>);
   static_assert(glz::detail::tuple_t<T&>);
   static_assert(glz::detail::tuple_t<T&&>);
   static_assert(!glz::detail::pair_t<T>);
   static_assert(!glz::detail::writable_map_t<T>);
   static_assert(!glz::detail::writable_map_t<const T>);
   static_assert(!glz::detail::writable_map_t<const T&>);
   static_assert(!glz::detail::writable_map_t<T&>);
   static_assert(!glz::detail::writable_map_t<T&&>);
   static_assert(!glz::detail::readable_map_t<T>);
   static_assert(!glz::detail::readable_map_t<T&>);
   static_assert(!glz::detail::readable_map_t<T&&>);
}

template <typename T>
consteval void assert_range_adapter_types() noexcept
{
   static_assert(!std::is_reference_v<T>, "Assertions internally test reference variaions. Provide value type");
   static_assert(glz::detail::array_t<T>);
   static_assert(glz::detail::writable_array_t<T>);
   static_assert(glz::detail::writable_array_t<const T>);
   static_assert(glz::detail::writable_array_t<const T&>);
   static_assert(glz::detail::writable_array_t<T&>);
   static_assert(glz::detail::writable_array_t<T&&>);
   static_assert(!glz::detail::writable_map_t<T>);
   static_assert(!glz::detail::writable_map_t<const T>);
   static_assert(!glz::detail::writable_map_t<const T&>);
   static_assert(!glz::detail::writable_map_t<T&>);
   static_assert(!glz::detail::writable_map_t<T&&>);
   static_assert(!glz::detail::readable_map_t<T>);
   static_assert(!glz::detail::readable_map_t<T&>);
   static_assert(!glz::detail::readable_map_t<T&&>);

   static_assert(std::ranges::input_range<T>);
   static_assert(std::input_iterator<typename T::iterator>);
   static_assert(std::input_iterator<typename T::const_iterator>);
   if constexpr (std::same_as<typename T::iterator, typename T::const_iterator>) {
      // glz::readable_array_t does not check the readability of elements. It merely that glaze is allowed to read it.
      // static_assert(!glz::detail::readable_array_t<T>);
      static_assert(!std::output_iterator<typename T::iterator, typename T::value_type>);
   }
   else {
      static_assert(glz::detail::readable_array_t<T>);
      static_assert(glz::detail::readable_array_t<T&>);
      static_assert(glz::detail::readable_array_t<T&&>);
      static_assert(std::output_iterator<typename T::iterator, typename T::value_type>);
   }
}

// TODO: test prefer_arrays_t
// TODO: update assertion functions to construct from r-value, not the x-value
// TODO: maybe move into members of adapters

template <typename Pair>
void pair_construction_assertions(Pair test_pair)
{
   glz::prefer_array_adapter as_array(test_pair);
   assert_pair_adapter_types<decltype(as_array)>();
   ut::expect(as_array == test_pair);
   ut::expect(get<0>(as_array) == test_pair.first);
   ut::expect(get<1>(as_array) == test_pair.second);

   const glz::prefer_array_adapter as_const_array(test_pair);
   assert_pair_adapter_types<decltype(as_const_array)>();
   ut::expect(as_const_array == test_pair);
   ut::expect(get<0>(as_const_array) == test_pair.first);
   ut::expect(get<1>(as_const_array) == test_pair.second);

   glz::prefer_array_adapter as_array_clone{as_array};
   assert_pair_adapter_types<decltype(as_array_clone)>();
   ut::expect(get<0>(as_array_clone) == get<0>(as_array));
   ut::expect(get<1>(as_array_clone) == get<1>(as_array));

   glz::prefer_array_adapter as_const_array_clone{as_const_array};
   assert_pair_adapter_types<decltype(as_const_array_clone)>();
   ut::expect(get<0>(as_const_array_clone) == get<0>(as_const_array));
   ut::expect(get<1>(as_const_array_clone) == get<1>(as_const_array));
}

template <typename Map>
void container_construction_assertions(Map test_map)
{
   glz::prefer_array_adapter as_array(test_map);
   assert_range_adapter_types<decltype(as_array)>();
   ut::expect(ut::that % as_array.size() == test_map.size());
   ut::expect((as_array.size() == test_map.size()) >> ut::fatal);
   ut::expect(std::equal(test_map.begin(), test_map.end(), as_array.begin()));

   const glz::prefer_array_adapter as_const_array(test_map);
   assert_range_adapter_types<decltype(as_const_array)>();
   ut::expect(ut::that % as_const_array.size() == test_map.size());
   ut::expect((as_const_array.size() == test_map.size()) >> ut::fatal);
   ut::expect(std::equal(test_map.begin(), test_map.end(), as_const_array.begin()));

   glz::prefer_array_adapter as_array_clone{as_array};
   assert_range_adapter_types<decltype(as_array_clone)>();
   ut::expect(std::ranges::equal(as_array_clone, as_array_clone));

   glz::prefer_array_adapter as_const_array_clone{as_const_array};
   assert_range_adapter_types<decltype(as_const_array_clone)>();
   ut::expect(std::ranges::equal(as_const_array_clone, as_const_array));
}

ut::suite pair_array_adaptors = [] {
   std::pair<int, int> pair{4, 5};
   std::pair<int&, int> ref_pair{pair.first, 8};
   std::pair<const int&, int> const_ref_pair{pair.first, 8};
   std::pair<const int, int> map_value_pair{pair.first, 8};

   // Note: accepting a reference to the pair does not modify the first/second_type.
   ut::test("construct & mutate array adapter") = []<typename Test_pair>(Test_pair& pair) {
      static_assert(!std::is_reference_v<Test_pair>);
      static_assert(glz::detail::pair_t<Test_pair>);
      static_assert(glz::detail::pair_t<Test_pair&&>);
      static_assert(glz::detail::pair_t<const Test_pair&>);
      static_assert(!glz::detail::writable_map_t<Test_pair>);
      static_assert(!glz::detail::readable_map_t<Test_pair>);

      pair_construction_assertions<Test_pair&>(pair);
      pair_construction_assertions<const Test_pair&>(pair);
      pair_construction_assertions<Test_pair>(pair);
      pair_construction_assertions<Test_pair&&>(std::move(pair));

      ut::test("mutate through adapter") = [&pair] {
         auto pair_copy = pair;
         glz::prefer_array_adapter as_array(pair_copy);
         ut::expect(as_array == pair_copy);
         ut::expect(get<0>(as_array) == pair_copy.first);
         ut::expect(get<1>(as_array) == pair_copy.second);

         get<1>(as_array) = 7777;
         ut::expect(get<1>(as_array) = 7777);
         ut::expect(get<1>(pair_copy) = 7777);
      };
   } | std::tuple(pair, ref_pair, const_ref_pair, map_value_pair);
};

ut::suite range_array_adaptors = [] {
   std::map<int, int> map{{4, 5}, {6, 7}};
   std::map<std::string, int> str_map{{"four", 5}, {"hello", 7}};
   std::map<int, int> empty_map{};
   std::vector<std::pair<int, int>> pair_vec{{4, 5}, {4, 5}};
   std::list<std::pair<int, int>> pair_list{{4, 5}, {4, 5}};

   ut::test("construct array adapter from containers") = []<typename Test_map>(Test_map& map) {
      static_assert(!std::is_reference_v<Test_map>);
      static_assert(glz::detail::readable_map_t<Test_map> || glz::detail::writable_map_t<Test_map>);
      static_assert(glz::detail::readable_map_t<Test_map&&> || glz::detail::writable_map_t<Test_map&&>);
      static_assert(glz::detail::readable_map_t<const Test_map&> || glz::detail::writable_map_t<const Test_map&>);
      static_assert(!glz::detail::pair_t<Test_map>);
      static_assert(!glz::detail::pair_t<Test_map>);

      container_construction_assertions<Test_map&>(map);
      container_construction_assertions<const Test_map&>(map);
      container_construction_assertions<Test_map>(map);
      container_construction_assertions<Test_map&&>(std::move(map));
   } | std::tuple(map, str_map, empty_map, pair_vec, pair_list);
#ifdef __cpp_lib_ranges
   ut::test("construct array adapter from view") = [] {
      // reading views are non-const operations. Split out from test parameterization to assert only non-const

      auto sized_view = std::ranges::iota_view{0, 5} | std::views::transform([](auto i) { return std::pair(i, i); });
      static_assert(std::ranges::sized_range<decltype(sized_view)>);

      ut::test("sized view") = [&sized_view]() mutable {
         glz::prefer_array_adapter as_array(sized_view);
         assert_range_adapter_types<decltype(as_array)>();
         ut::expect(ut::that % as_array.size() == sized_view.size());
         ut::expect(std::equal(sized_view.begin(), sized_view.end(), as_array.begin()));

         const glz::prefer_array_adapter as_const_array(sized_view);
         assert_range_adapter_types<decltype(as_const_array)>();
         ut::expect(ut::that % as_const_array.size() == sized_view.size());
         ut::expect(std::equal(sized_view.begin(), sized_view.end(), as_const_array.begin()));
      };

      ut::test("unsized view") = [&sized_view]() mutable {
         auto unsized_view = sized_view | std::views::filter([](const auto& pair) { return pair.first % 2 == 0; });
         static_assert(!std::ranges::sized_range<decltype(unsized_view)>);

         glz::prefer_array_adapter as_array{unsized_view};
         assert_range_adapter_types<decltype(as_array)>();
         ut::expect(std::equal(unsized_view.begin(), unsized_view.end(), as_array.begin()));

         const glz::prefer_array_adapter as_const_array{unsized_view};
         assert_range_adapter_types<decltype(as_const_array)>();
         ut::expect(std::equal(unsized_view.begin(), unsized_view.end(), as_const_array.begin()));
      };
   };
#endif
};

int main()
{
   using Common = std::common_type_t<
            std::pair<const std::string_view, int>,
            std::pair<const std::string_view, int>
        >;
    static_assert(std::same_as<
        Common
        ,
            std::pair<const std::string_view, int>
    >);

   std::map<std::string, int> m;
   glz::prefer_array_adapter a{m};
   // assert_range_adapter_types<decltype(a)>();
   static_assert(std::ranges::input_range<decltype(a)>);
}