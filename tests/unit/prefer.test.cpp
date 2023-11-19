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
//
#include <boost/ut.hpp>

namespace ut = boost::ut;
using ut::operator|;
using ut::operator>>;

// namespace glaze
// {
//    template <typename... Ts>
//    std::ostream& operator<<(std::ostream& os, const std::tuple<Ts...>& tuple)
//    {
//       return std::apply(
//          [&os](const auto& first, const auto&... rest) {
//             os << '(' << first;
//             ((os << ',' << rest) << ...);
//             return os << ')';
//          },
//          tuple);
//    }
// }

// template <typename T>
// consteval void assert_range_adaptor_types(const T&) noexcept
// {
//    static_assert(std::input_iterator<typename T::iterator>);
//    static_assert(std::input_iterator<typename T::const_iterator>);
//    static_assert(glz::detail::array_t<T>);
//    static_assert(glz::detail::readable_array_t<T>);
//    static_assert(!glz::detail::writable_array_t<T>);
// }

template <typename T>
consteval void assert_range_adaptor_types(T&) noexcept
{
   if constexpr (!std::same_as<typename T::iterator, typename T::const_iterator>) {
      static_assert(std::output_iterator<typename T::iterator, typename T::value_type>);
   }
   static_assert(std::input_iterator<typename T::const_iterator>);
   static_assert(glz::detail::array_t<T>);
   static_assert(glz::detail::readable_array_t<T>);
   static_assert(glz::detail::writable_array_t<T>);
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

      auto assertions = []<typename PUT>(PUT pair_under_test) {
         glz::prefer_array_adapter as_array(pair_under_test);
         ut::expect(as_array == pair_under_test);
         ut::expect(as_array == pair_under_test);

         int different_first_int{909};
         int different_second_int{808};
         const PUT another_pair{different_first_int, different_second_int};
         glz::prefer_array_adapter<const PUT> constructed_from_adapter{another_pair};
         ut::expect(std::get<0>(constructed_from_adapter) == 909);
         ut::expect(std::get<1>(constructed_from_adapter) == 808);

         const glz::prefer_array_adapter as_const_array(pair_under_test);
         ut::expect(as_const_array == pair_under_test);
         ut::expect(as_const_array == pair_under_test);
      };

      ut::test("across qualifications") =
         assertions | std::tuple<Test_pair&, const Test_pair&, Test_pair>{pair, pair, pair};

      ut::test("as r-ref (can't parameterize)") = [&pair, &assertions] {
         Test_pair&& rref_pair{Test_pair{pair}};
         assertions(rref_pair);
      };

      ut::test("mutate through adapter") = [&pair] {
         auto pair_copy = pair;
         glz::prefer_array_adapter as_array(pair_copy);
         ut::expect(as_array == pair_copy);

         std::get<1>(as_array) = 7777;
         ut::expect(std::get<1>(as_array) = 7777);
         ut::expect(std::get<1>(pair_copy) = 7777);
      };
   } | std::tuple(pair, ref_pair, const_ref_pair, map_value_pair);

   ut::test("assign pair adapter") = []<typename Test_pair>(Test_pair pair) {
      const glz::prefer_array_adapter as_array{pair};

      int different_first_int{909};
      int different_second_int{808};
      Test_pair another_pair{different_first_int, different_second_int};

      glz::prefer_array_adapter assignee{pair};
      assignee = another_pair;
      ut::expect(std::get<0>(assignee) == 909);
      ut::expect(std::get<1>(assignee) == 808);
      ut::expect(std::get<0>(pair) == 909);
      ut::expect(std::get<1>(pair) == 808);
   } | std::tuple(pair, ref_pair);
};

ut::suite range_array_adaptors = [] {
   std::map<int, int> map{{4, 5}, {6, 7}};
   std::map<int, int> empty_map{};
   std::list<std::pair<int, int>> pair_list{{4, 5}, {4, 5}};

   ut::test("construct array adapter from containers") = []<typename Test_map>(Test_map& map) {
      static_assert(!std::is_reference_v<Test_map>);
      static_assert(glz::detail::readable_map_t<Test_map> || glz::detail::writable_map_t<Test_map>);
      static_assert(glz::detail::readable_map_t<Test_map&&> || glz::detail::writable_map_t<Test_map&&>);
      static_assert(glz::detail::readable_map_t<const Test_map&> || glz::detail::writable_map_t<const Test_map&>);
      static_assert(!glz::detail::pair_t<Test_map>);
      static_assert(!glz::detail::pair_t<Test_map>);

      auto assertions = [](auto map_under_test) {
         glz::prefer_array_adapter as_array(map_under_test);
         ut::expect(ut::that % as_array.size() == map_under_test.size());
         ut::expect((as_array.size() == map_under_test.size()) >> ut::fatal);
         ut::expect(std::equal(map_under_test.begin(), map_under_test.end(), as_array.begin()));

         const glz::prefer_array_adapter as_const_array(map_under_test);
         ut::expect(ut::that % as_const_array.size() == map_under_test.size());
         ut::expect((as_const_array.size() == map_under_test.size()) >> ut::fatal);
         ut::expect(std::equal(map_under_test.begin(), map_under_test.end(), as_const_array.begin()));
      };

      ut::test("across qualifications") = assertions | std::tuple<Test_map&, const Test_map&, Test_map>{map, map, map};

      ut::test("as r-ref (can't parameterize)") = [&map, &assertions] {
         Test_map&& rref_map{Test_map{map}};
         assertions(rref_map);
      };
   } | std::tuple(map, empty_map, pair_list);

#ifdef __cpp_lib_ranges
   ut::test("construct array adapter from view") = [] {
      // reading views are non-const operations. Split out from test parameterization to assert only non-const

      auto as_tuples = std::views::transform([](const auto& pair) { return std::tie(pair.first, pair.second); });
      auto sized_view = std::ranges::iota_view{0, 5} | std::views::transform([](auto i) { return std::pair(i, i); });
      static_assert(std::ranges::sized_range<decltype(sized_view)>);

      ut::test("sized view") = [&as_tuples, &sized_view]() mutable {
         auto view_tuples = sized_view | as_tuples; // for comparisons

         glz::prefer_array_adapter as_array(sized_view);
         ut::expect(ut::that % as_array.size() == sized_view.size());
         ut::expect(std::ranges::equal(view_tuples, as_array));

         const glz::prefer_array_adapter as_const_array(sized_view);
         ut::expect(ut::that % as_const_array.size() == sized_view.size());
         ut::expect(std::ranges::equal(view_tuples, as_const_array));
      };

      ut::test("unsized view") = [&as_tuples, &sized_view]() mutable {
         auto unsized_view = sized_view | std::views::filter([](const auto& pair) { return pair.first % 2 == 0; });
         auto view_tuples = unsized_view | as_tuples; // for comparisons
         static_assert(!std::ranges::sized_range<decltype(unsized_view)>);

         glz::prefer_array_adapter as_array{unsized_view};
         ut::expect(std::ranges::equal(view_tuples, as_array));

         const glz::prefer_array_adapter as_const_array{unsized_view};
         ut::expect(std::ranges::equal(view_tuples, as_const_array));
      };
   };
#endif

   // TODO: assign array adapter
};

int main() {}