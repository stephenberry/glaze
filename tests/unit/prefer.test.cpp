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

template <typename T>
consteval void assert_range_adaptor_types() noexcept
{
   static_assert(std::input_iterator<typename T::iterator>);
   static_assert(std::input_iterator<typename T::const_iterator>);
   static_assert(glz::detail::array_t<T>);
   static_assert(glz::detail::writable_array_t<T>);
   if constexpr (std::same_as<typename T::iterator, typename T::const_iterator>) {
      // readable_array_t does not check the readability of elements, merely that glaze can read it
      // static_assert(!glz::detail::readable_array_t<T>);
      static_assert(glz::detail::array_t<T>);
   }
   else {
      static_assert(glz::detail::readable_array_t<T>);
      static_assert(std::output_iterator<typename T::iterator, typename T::value_type>);
   }
}

// consteval void assert_pair_adaptor_deduced_types() noexcept
// {
//    // Ensure type deduction of pair type produces proper reference members in adaptors
//    { // pair
//       std::pair<int, int> pair{1, 2};
//       glz::prefer_array_adapter a{pair};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a)::tuple_type>, int&>);
//
//       glz::prefer_array_adapter a_r{std::move(pair)};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_r)::tuple_type>, int&&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_r)::tuple_type>, int&&>);
//    }
//    { // const-pair
//       const std::pair<int, int> const_pair{1, 2};
//       glz::prefer_array_adapter a{const_pair};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a)::tuple_type>, const int&>);
//    }
//    { // pair of const
//       std::pair<int, const int> pair_of_const_1{1, 2};
//       std::pair<const int, int> pair_of_const_2{1, 2};
//       glz::prefer_array_adapter a_1{pair_of_const_1};
//       glz::prefer_array_adapter a_2{pair_of_const_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, int&>);
//    }
//    { // pair of ref
//       int i{1};
//       std::pair<int&, int> pair_of_ref_1{i, 2};
//       std::pair<int, int&> pair_of_ref_2{2, i};
//       glz::prefer_array_adapter a_1{pair_of_ref_1};
//       glz::prefer_array_adapter a_2{pair_of_ref_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, int&>);
//
//       glz::prefer_array_adapter a_r_1{std::move(pair_of_ref_1)};
//       glz::prefer_array_adapter a_r_2{std::move(pair_of_ref_2)};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_r_1)::tuple_type>, int&&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_r_1)::tuple_type>, int&&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_r_2)::tuple_type>, int&&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_r_2)::tuple_type>, int&&>);
//    }
//    { // cosnt pair of ref
//       int i{1};
//       const std::pair<int&, int> const_pair_of_ref_1{i, 2};
//       const std::pair<int, int&> const_pair_of_ref_2{2, i};
//       glz::prefer_array_adapter a_1{const_pair_of_ref_1};
//       glz::prefer_array_adapter a_2{const_pair_of_ref_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, const int&>);
//    }
//    { // const pair of const ref
//       int i{1};
//       const std::pair<const int&, int> const_pair_of_cref_1{i, 2};
//       const std::pair<int, const int&> const_pair_of_cref_2{2, i};
//       glz::prefer_array_adapter a_1{const_pair_of_cref_1};
//       glz::prefer_array_adapter a_2{const_pair_of_cref_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, const int&>);
//    }
//    { // rref-pair
//       std::pair<int, int> pair{1, 2};
//       glz::prefer_array_adapter a{std::move(pair)};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a)::tuple_type>, int&&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a)::tuple_type>, int&&>);
//    }
//    { // pair of rref
//       int first{1};
//       int second{2};
//       std::pair<int&&, int> pair_of_rref_1{std::move(first), 2};
//       std::pair<int, int&&> pair_of_rref_2{1, std::move(second)};
//       glz::prefer_array_adapter a_1{pair_of_rref_1};
//       glz::prefer_array_adapter a_2{pair_of_rref_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, const int&>);
//    }
// }
//
// consteval void assert_pair_adaptor_explicit_types() noexcept
// {
//    std::pair<int, int> pair{1, 2};
//    { // type
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<int, int>> a{pair};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a)::tuple_type>, int&>);
//    }
//    { // ref
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<int, int>&> a{pair};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a)::tuple_type>, int&>);
//    }
//    { // const
//       [[maybe_unused]] glz::prefer_array_adapter<const std::pair<int, int>> a{pair};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a)::tuple_type>, const int&>);
//    }
//    { // const-ref
//       [[maybe_unused]] glz::prefer_array_adapter<const std::pair<int, int>&> a{pair};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a)::tuple_type>, const int&>);
//    }
//    { // r-ref
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<int, int>&&> a{std::move(pair)};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a)::tuple_type>, int&&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a)::tuple_type>, int&&>);
//    }
//
//    std::pair<int, const int> pair_of_const_1{1, 2};
//    std::pair<const int, int> pair_of_const_2{1, 2};
//    { // type
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<int, const int>> a_1{pair_of_const_1};
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<const int, int>> a_2{pair_of_const_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, int&>);
//    }
//
//    { // ref
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<int, const int>&> a_1{pair_of_const_1};
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<const int, int>&> a_2{pair_of_const_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, int&>);
//    }
//
//    { // const
//       [[maybe_unused]] glz::prefer_array_adapter<const std::pair<int, const int>> a_1{pair_of_const_1};
//       [[maybe_unused]] glz::prefer_array_adapter<const std::pair<const int, int>> a_2{pair_of_const_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, const int&>);
//    }
//
//    { // const-ref
//       [[maybe_unused]] glz::prefer_array_adapter<const std::pair<int, const int>> a_1{pair_of_const_1};
//       [[maybe_unused]] glz::prefer_array_adapter<const std::pair<const int, int>> a_2{pair_of_const_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, const int&>);
//    }
//
//    int pair_of_ref_i{1};
//    std::pair<int&, int> pair_of_ref_1{pair_of_ref_i, 2};
//    std::pair<int, int&> pair_of_ref_2{2, pair_of_ref_i};
//    { // type
//       [[maybe_unused]] [[maybe_unused]] glz::prefer_array_adapter<std::pair<int&, int>> a_1{pair_of_ref_1};
//       [[maybe_unused]] [[maybe_unused]] glz::prefer_array_adapter<std::pair<int, int&>> a_2{pair_of_ref_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, int&>);
//    }
//    { // ref
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<int&, int>&> a_1{pair_of_ref_1};
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<int, int&>&> a_2{pair_of_ref_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, int&>);
//    }
//    { // const
//       [[maybe_unused]] glz::prefer_array_adapter<const std::pair<int&, int>> a_1{pair_of_ref_1};
//       [[maybe_unused]] glz::prefer_array_adapter<const std::pair<int, int&>> a_2{pair_of_ref_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, const int&>);
//    }
//    { // const-ref
//       [[maybe_unused]] glz::prefer_array_adapter<const std::pair<int&, int>&> a_1{pair_of_ref_1};
//       [[maybe_unused]] glz::prefer_array_adapter<const std::pair<int, int&>&> a_2{pair_of_ref_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, const int&>);
//    }
//    { // r-ref
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<int&, int>&&> a_1{std::move(pair_of_ref_1)};
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<int, int&>&&> a_2{std::move(pair_of_ref_2)};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, int&&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, int&&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, int&&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, int&&>);
//    }
//
//    int pair_of_cref_i{1};
//    std::pair<const int&, int> pair_of_cref_1{pair_of_cref_i, 2};
//    std::pair<int, const int&> pair_of_cref_2{2, pair_of_cref_i};
//    { // type
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<const int&, int>> a_1{pair_of_cref_1};
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<int, const int&>> a_2{pair_of_cref_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, const int&>);
//    }
//    { // ref
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<const int&, int>&> a_1{pair_of_cref_1};
//       [[maybe_unused]] glz::prefer_array_adapter<std::pair<int, const int&>&> a_2{pair_of_cref_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, const int&>);
//    }
//    { // const
//       [[maybe_unused]] glz::prefer_array_adapter<const std::pair<const int&, int>> a_1{pair_of_cref_1};
//       [[maybe_unused]] glz::prefer_array_adapter<const std::pair<int, const int&>> a_2{pair_of_cref_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, const int&>);
//    }
//    { // const-ref
//       [[maybe_unused]] glz::prefer_array_adapter<const std::pair<const int&, int>&> a_1{pair_of_cref_1};
//       [[maybe_unused]] glz::prefer_array_adapter<const std::pair<int, const int&>&> a_2{pair_of_cref_2};
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_1)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<0, decltype(a_2)::tuple_type>, const int&>);
//       static_assert(std::same_as<std::tuple_element_t<1, decltype(a_2)::tuple_type>, const int&>);
//    }
// }
template <typename Pair>
void pair_construction_assertions(Pair test_pair)
{
   { // CTAD
      glz::prefer_array_adapter as_array(test_pair);
      ut::expect(as_array == test_pair);
      ut::expect(get<0>(as_array) == test_pair.first);
      ut::expect(get<1>(as_array) == test_pair.second);

      const glz::prefer_array_adapter as_const_array(test_pair);
      ut::expect(as_const_array == test_pair);
      ut::expect(get<0>(as_const_array) == test_pair.first);
      ut::expect(get<1>(as_const_array) == test_pair.second);

      glz::prefer_array_adapter as_array_clone{as_array};
      ut::expect(get<0>(as_array_clone) == get<0>(as_array));
      ut::expect(get<1>(as_array_clone) == get<1>(as_array));

      glz::prefer_array_adapter as_const_array_clone{as_const_array};
      ut::expect(get<0>(as_const_array_clone) == get<0>(as_const_array));
      ut::expect(get<1>(as_const_array_clone) == get<1>(as_const_array));
   }
   { // explicit template type
      glz::prefer_array_adapter<Pair> as_array(test_pair);
      ut::expect(as_array == test_pair);
      ut::expect(get<0>(as_array) == test_pair.first);
      ut::expect(get<1>(as_array) == test_pair.second);

      const glz::prefer_array_adapter<Pair> as_const_array(test_pair);
      ut::expect(as_const_array == test_pair);
      ut::expect(get<0>(as_const_array) == test_pair.first);
      ut::expect(get<1>(as_const_array) == test_pair.second);

      glz::prefer_array_adapter<Pair> as_array_clone{as_array};
      ut::expect(get<0>(as_array_clone) == get<0>(as_array));
      ut::expect(get<1>(as_array_clone) == get<1>(as_array));

      glz::prefer_array_adapter<Pair> as_const_array_clone{as_const_array};
      ut::expect(get<0>(as_const_array_clone) == get<0>(as_const_array));
      ut::expect(get<1>(as_const_array_clone) == get<1>(as_const_array));
   }
}

template <typename Map>
void container_construction_assertions(Map test_map)
{
   { // CTAD
      glz::prefer_array_adapter as_array(test_map);
      assert_range_adaptor_types<decltype(as_array)>();
      ut::expect(ut::that % as_array.size() == test_map.size());
      ut::expect((as_array.size() == test_map.size()) >> ut::fatal);
      ut::expect(std::equal(test_map.begin(), test_map.end(), as_array.begin()));

      const glz::prefer_array_adapter as_const_array(test_map);
      assert_range_adaptor_types<decltype(as_const_array)>();
      ut::expect(ut::that % as_const_array.size() == test_map.size());
      ut::expect((as_const_array.size() == test_map.size()) >> ut::fatal);
      ut::expect(std::equal(test_map.begin(), test_map.end(), as_const_array.begin()));

      glz::prefer_array_adapter as_array_clone{as_array};
      ut::expect(std::ranges::equal(as_array_clone, as_array_clone));

      glz::prefer_array_adapter as_const_array_clone{as_const_array};
      ut::expect(std::ranges::equal(as_const_array_clone, as_const_array));
   }
   { // explicit template type
      glz::prefer_array_adapter<Map> as_array(test_map);
      assert_range_adaptor_types<decltype(as_array)>();
      ut::expect(ut::that % as_array.size() == test_map.size());
      ut::expect((as_array.size() == test_map.size()) >> ut::fatal);
      ut::expect(std::equal(test_map.begin(), test_map.end(), as_array.begin()));

      const glz::prefer_array_adapter<Map> as_const_array(test_map);
      assert_range_adaptor_types<decltype(as_const_array)>();
      ut::expect(ut::that % as_const_array.size() == test_map.size());
      ut::expect((as_const_array.size() == test_map.size()) >> ut::fatal);
      ut::expect(std::equal(test_map.begin(), test_map.end(), as_const_array.begin()));

      glz::prefer_array_adapter<Map> as_array_clone{as_array};
      ut::expect(std::ranges::equal(as_array_clone, as_array_clone));

      glz::prefer_array_adapter<Map> as_const_array_clone{as_const_array};
      ut::expect(std::ranges::equal(as_const_array_clone, as_const_array));
   }
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
   } | std::tuple(map, empty_map, pair_vec, pair_list);
#ifdef __cpp_lib_ranges
   ut::test("construct array adapter from view") = [] {
      // reading views are non-const operations. Split out from test parameterization to assert only non-const

      auto sized_view = std::ranges::iota_view{0, 5} | std::views::transform([](auto i) { return std::pair(i, i); });
      static_assert(std::ranges::sized_range<decltype(sized_view)>);

      ut::test("sized view") = [&sized_view]() mutable {
         glz::prefer_array_adapter as_array(sized_view);
         ut::expect(ut::that % as_array.size() == sized_view.size());
         ut::expect(std::equal(sized_view.begin(), sized_view.end(), as_array.begin()));

         const glz::prefer_array_adapter as_const_array(sized_view);
         ut::expect(ut::that % as_const_array.size() == sized_view.size());
         ut::expect(std::equal(sized_view.begin(), sized_view.end(), as_const_array.begin()));
      };

      ut::test("unsized view") = [&sized_view]() mutable {
         auto unsized_view = sized_view | std::views::filter([](const auto& pair) { return pair.first % 2 == 0; });
         static_assert(!std::ranges::sized_range<decltype(unsized_view)>);

         glz::prefer_array_adapter as_array{unsized_view};
         ut::expect(std::equal(unsized_view.begin(), unsized_view.end(), as_array.begin()));

         const glz::prefer_array_adapter as_const_array{unsized_view};
         ut::expect(std::equal(unsized_view.begin(), unsized_view.end(), as_const_array.begin()));
      };
   };
#endif
};

int main()
{
   // assert_pair_adaptor_deduced_types();
   // assert_pair_adaptor_explicit_types();
}