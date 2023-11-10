#include <glaze/json/prefer.hpp>
//
#include <algorithm>
#include <concepts>
#include <list>
#include <map>
#include <ostream>
#include <ranges>
#include <tuple>
#include <utility>
//
#include <boost/ut.hpp>

namespace ut = boost::ut;
using ut::operator|;
using ut::operator>>;

namespace std
{
   template <typename... Ts>
   std::ostream& operator<<(std::ostream& os, const std::tuple<Ts...>& tuple)
   {
      return std::apply(
         [&os](const auto& first, const auto&... rest) {
            os << '(' << first;
            ((os << ',' << rest) << ...);
            return os << ')';
         },
         tuple);
   }
}

int main()
{
   std::pair<int, int> pair{4, 5};
   std::pair<int&, int> ref_pair{pair.first, 8};
   std::pair<const int&, int> const_ref_pair{pair.first, 8};
   std::pair<const int, int> map_value_pair{pair.first, 8};

   ut::test("construct array adapter from pair") = []<typename Test_pair>(Test_pair& pair) {
      // accepting a reference to the pair does not modify the first/second_type.
      static_assert(!std::is_reference_v<Test_pair>);
      static_assert(glz::detail::pair_t<Test_pair>);
      static_assert(glz::detail::pair_t<Test_pair&&>);
      static_assert(glz::detail::pair_t<const Test_pair&>);
      static_assert(!glz::detail::writable_map_t<Test_pair>);
      static_assert(!glz::detail::readable_map_t<Test_pair>);

      auto assertions = [](auto pair_under_test) {
         glz::prefer_array_adapter as_array(pair_under_test);
         ut::expect(as_array == pair_under_test);
         ut::expect(as_array == pair_under_test);

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
   } | std::tuple(pair, ref_pair, const_ref_pair, map_value_pair);

   std::map<int, int> map{{4, 5}, {6, 7}};
   std::map<int, int> empty_map{};
   std::list<std::pair<int, int>> pair_list{{4, 5}, {4, 5}};

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

   ut::test("construct array adapter from containers") = [&assertions]<typename Test_map>(Test_map& map) {
      static_assert(!std::is_reference_v<Test_map>);
      static_assert(glz::detail::readable_map_t<Test_map> || glz::detail::writable_map_t<Test_map>);
      static_assert(glz::detail::readable_map_t<Test_map&&> || glz::detail::writable_map_t<Test_map&&>);
      static_assert(glz::detail::readable_map_t<const Test_map&> || glz::detail::writable_map_t<const Test_map&>);
      static_assert(!glz::detail::pair_t<Test_map>);
      static_assert(!glz::detail::pair_t<Test_map>);

      ut::test("across qualifications") = assertions | std::tuple<Test_map&, const Test_map&, Test_map>{map, map, map};

      ut::test("as r-ref (can't parameterize)") = [&map, &assertions] {
         Test_map&& rref_map{Test_map{map}};
         assertions(rref_map);
      };
   } | std::tuple(map, empty_map);

#ifdef __cpp_lib_ranges
   ut::test("construct array adapter from range") = [&assertions] {
      // reading ranges is a non-const operation, so the tests are split out
      auto sized_range = std::ranges::iota_view{0, 5} | std::views::transform([](auto i) { return std::pair(i, i); });
      assertions(sized_range);

      auto unsized_range = sized_range | std::views::filter([](const auto& pair) { return pair.first % 2 == 0; });
      glz::prefer_array_adapter as_array{unsized_range};
      static_assert(std::ranges::input_range<decltype(unsized_range)>);
      static_assert(std::ranges::input_range<decltype(as_array)>);
      ut::expect(std::equal(unsized_range.begin(), unsized_range.end(), as_array.begin()));
      ut::expect(std::equal(unsized_range.begin(), unsized_range.end(), as_array.begin()));
   };
#endif
}