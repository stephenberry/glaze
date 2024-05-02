// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <any>
#include <charconv>

#include "glaze/core/common.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/json/read.hpp"
#include "glaze/util/parse.hpp"
#include "glaze/util/string_literal.hpp"

namespace glz
{
   namespace detail
   {
      template <class F, class T>
         requires glaze_value_t<T>
      bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept;

      template <class F, class T>
         requires glaze_array_t<T> || tuple_t<std::decay_t<T>> || array_t<std::decay_t<T>> ||
                  is_std_tuple<std::decay_t<T>>
      bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept;

      template <class F, class T>
         requires nullable_t<std::decay_t<T>>
      bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept;

      template <class F, class T>
         requires readable_map_t<std::decay_t<T>> || glaze_object_t<T> || reflectable<T>
      bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept;

      template <class F, class T>
      bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept
      {
         if (json_ptr.empty()) {
            func(value);
            return true;
         }
         return false;
      }

      // TODO: compile time search for `~` and optimize if escape does not exist
      template <class F, class T>
         requires readable_map_t<std::decay_t<T>> || glaze_object_t<T> || reflectable<T>
      bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept
      {
         if (json_ptr.empty()) {
            func(value);
            return true;
         }
         if (json_ptr[0] != '/' || json_ptr.size() < 2) return false;

         static thread_local auto key = []() {
            if constexpr (writable_map_t<std::decay_t<T>>) {
               return typename std::decay_t<T>::key_type{};
            }
            else {
               return std::string{};
            }
         }();
         using key_t = std::decay_t<decltype(key)>;
         static_assert(std::is_same_v<key_t, std::string> || num_t<key_t>);

         if constexpr (std::is_same_v<key_t, std::string>) {
            key.clear();
            size_t i = 1;
            for (; i < json_ptr.size(); ++i) {
               auto c = json_ptr[i];
               if (c == '/')
                  break;
               else if (c == '~') {
                  if (++i == json_ptr.size()) return false;
                  c = json_ptr[i];
                  if (c == '0')
                     c = '~';
                  else if (c == '1')
                     c = '/';
                  else
                     return false;
               }
               key.push_back(c);
            }
            json_ptr = json_ptr.substr(i);
         }
         else if constexpr (std::is_floating_point_v<key_t>) {
            auto it = reinterpret_cast<const uint8_t*>(json_ptr.data());
            auto s = parse_float<key_t>(key, it);
            if (!s) return false;
            json_ptr = json_ptr.substr(reinterpret_cast<const char*>(it) - json_ptr.data());
         }
         else {
            auto [p, ec] = std::from_chars(&json_ptr[1], json_ptr.data() + json_ptr.size(), key);
            if (ec != std::errc{}) return false;
            json_ptr = json_ptr.substr(p - json_ptr.data());
         }

         if constexpr (glaze_object_t<T> || reflectable<T>) {
            decltype(auto) frozen_map = [&]() -> decltype(auto) {
               if constexpr (reflectable<T>) {
#if ((defined _MSC_VER) && (!defined __clang__))
                  static thread_local auto cmap = make_map<T>();
#else
                  static thread_local constinit auto cmap = make_map<T>();
#endif
                  populate_map(value, cmap); // Function required for MSVC to build
                  return cmap;
               }
               else {
                  static constexpr auto cmap = make_map<T>();
                  return cmap;
               }
            }();

            const auto& member_it = frozen_map.find(key);
            if (member_it != frozen_map.end()) [[likely]] {
               return std::visit(
                  [&](auto&& member_ptr) {
                     return seek_impl(std::forward<F>(func), get_member(value, member_ptr), json_ptr);
                  },
                  member_it->second);
            }
            else [[unlikely]] {
               return false;
            }
         }
         else {
            return seek_impl(std::forward<F>(func), value[key], json_ptr);
         }
      }

      template <class F, class T>
         requires glaze_array_t<T> || tuple_t<std::decay_t<T>> || array_t<std::decay_t<T>> ||
                  is_std_tuple<std::decay_t<T>>
      bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept
      {
         if (json_ptr.empty()) {
            func(value);
            return true;
         }
         if (json_ptr[0] != '/' || json_ptr.size() < 2) return false;

         size_t index{};
         auto [p, ec] = std::from_chars(&json_ptr[1], json_ptr.data() + json_ptr.size(), index);
         if (ec != std::errc{}) return false;
         json_ptr = json_ptr.substr(p - json_ptr.data());

         if constexpr (glaze_array_t<std::decay_t<T>>) {
            static constexpr auto member_array = glz::detail::make_array<decay_keep_volatile_t<T>>();
            if (index >= member_array.size()) return false;
            return std::visit(
               [&](auto&& member_ptr) {
                  return seek_impl(std::forward<F>(func), get_member(value, member_ptr), json_ptr);
               },
               member_array[index]);
         }
         else if constexpr (tuple_t<std::decay_t<T>> || is_std_tuple<std::decay_t<T>>) {
            if (index >= glz::tuple_size_v<std::decay_t<T>>) return false;
            auto tuple_element_ptr = get_runtime(value, index);
            return std::visit(
               [&](auto&& element_ptr) { return seek_impl(std::forward<F>(func), *element_ptr, json_ptr); },
               tuple_element_ptr);
         }
         else {
            return seek_impl(std::forward<F>(func), *std::next(value.begin(), index), json_ptr);
         }
      }

      template <class F, class T>
         requires nullable_t<std::decay_t<T>>
      bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept
      {
         if (json_ptr.empty()) {
            func(value);
            return true;
         }
         if (!value) return false;
         return seek_impl(std::forward<F>(func), *value, json_ptr);
      }

      template <class F, class T>
         requires glaze_value_t<T>
      bool seek_impl(F&& func, T&& value, sv json_ptr) noexcept
      {
         decltype(auto) member = get_member(value, meta_wrapper_v<std::remove_cvref_t<T>>);
         if (json_ptr.empty()) {
            func(member);
            return true;
         }
         return seek_impl(std::forward<F>(func), member, json_ptr);
      }
   } // namespace detail

   // Call a function on an value at the location of a json_ptr
   template <class F, class T>
   bool seek(F&& func, T&& value, sv json_ptr) noexcept
   {
      return detail::seek_impl(std::forward<F>(func), std::forward<T>(value), json_ptr);
   }

   template <class R>
   using call_result_t = std::conditional_t<std::is_reference_v<R> || std::is_pointer_v<R>, std::decay_t<R>*, R>;

   template <class R>
   using call_return_t =
      std::conditional_t<std::is_reference_v<R>, expected<std::reference_wrapper<std::decay_t<R>>, error_code>,
                         expected<R, error_code>>;

   // call a member function
   template <class R, class T, class... Args>
   call_return_t<R> call(T&& root_value, sv json_ptr, Args&&... args) noexcept
   {
      call_result_t<R> result{};

      error_code ec{};

      const auto valid = detail::seek_impl(
         [&ec, &result, &root_value, ... args = std::forward<Args>(args)](auto&& val) {
            using V = std::decay_t<decltype(val)>;
            if constexpr (std::is_member_function_pointer_v<V>) {
               auto f = std::mem_fn(val);
               using F = decltype(f);
               if constexpr (std::invocable<F, T, Args...>) {
                  if constexpr (!std::is_assignable_v<R, std::invoke_result_t<F, T, Args...>>) {
                     ec = error_code::invalid_call; // type not assignable
                  }
                  else {
                     if constexpr (std::is_reference_v<R>) {
                        result = &f(root_value, std::forward<Args>(args)...);
                     }
                     else {
                        result = f(root_value, std::forward<Args>(args)...);
                     }
                  }
               }
               else {
                  ec = error_code::invalid_call; // not invocable with given inputs
               }
            }
            else {
               ec = error_code::invalid_call; // seek did not find a function
            }
         },
         std::forward<T>(root_value), json_ptr);

      if (!valid) {
         return unexpected(error_code::get_nonexistent_json_ptr);
      }

      if (bool(ec)) {
         return unexpected(ec);
      }

      if constexpr (std::is_reference_v<R>) {
         return *result;
      }
      else {
         return result;
      }
   }

   // Get a refrence to a value at the location of a json_ptr. Will error if
   // value doesnt exist or is wrong type
   template <class V, class T>
   expected<std::reference_wrapper<V>, parse_error> get(T&& root_value, sv json_ptr) noexcept
   {
      V* result{};
      error_code ec{};
      detail::seek_impl(
         [&](auto&& val) {
            if constexpr (!std::is_same_v<V, decay_keep_volatile_t<decltype(val)>>) {
               ec = error_code::get_wrong_type;
            }
            else if constexpr (!std::is_lvalue_reference_v<decltype(val)>) {
               ec = error_code::cannot_be_referenced;
            }
            else {
               result = &val;
            }
         },
         std::forward<T>(root_value), json_ptr);
      if (!result) {
         return unexpected(parse_error{error_code::get_nonexistent_json_ptr});
      }
      else if (bool(ec)) {
         return unexpected(parse_error{ec});
      }
      return std::ref(*result);
   }

   // Get a pointer to a value at the location of a json_ptr. Will return
   // nullptr if value doesnt exist or is wrong type
   template <class V, class T>
   V* get_if(T&& root_value, sv json_ptr) noexcept
   {
      V* result{};
      detail::seek_impl(
         [&](auto&& val) {
            if constexpr (std::is_same_v<V, std::decay_t<decltype(val)>>) {
               result = &val;
            }
         },
         std::forward<T>(root_value), json_ptr);
      return result;
   }

   // Get a value at the location of a json_ptr. Will error if
   // value doesnt exist or is not asignable or is a narrowing conversion.
   template <class V, class T>
   expected<V, error_code> get_value(T&& root_value, sv json_ptr) noexcept
   {
      V result{};
      bool found{};
      error_code ec{};
      detail::seek_impl(
         [&](auto&& val) {
            if constexpr (std::is_assignable_v<V, decltype(val)> &&
                          detail::non_narrowing_convertable<std::decay_t<decltype(val)>, V>) {
               found = true;
               result = val;
            }
            else {
               ec = error_code::get_wrong_type;
            }
         },
         std::forward<T>(root_value), json_ptr);
      if (!found) {
         return unexpected(error_code::get_nonexistent_json_ptr);
      }
      else if (bool(ec)) {
         return unexpected(ec);
      }

      return result;
   }

   // Assign to a value at the location of a json_ptr with respect to the root_value
   // if assignable and not a narrowing conversion
   template <class T, class V>
   bool set(T&& root_value, const sv json_ptr, V&& value) noexcept
   {
      bool result{};
      detail::seek_impl(
         [&](auto&& val) {
            if constexpr (std::is_assignable_v<decltype(val), decltype(value)> &&
                          detail::non_narrowing_convertable<std::decay_t<decltype(value)>,
                                                            std::decay_t<decltype(val)>>) {
               result = true;
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
#endif
               val = value;
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
            }
         },
         std::forward<T>(root_value), json_ptr);
      return result;
   }

   inline constexpr size_t json_ptr_depth(const auto s)
   {
      size_t count = 0;
      for (size_t i = 0; (i = s.find('/', i)) != std::string::npos; ++i) {
         ++count;
      }
      return count;
   }

   // TODO: handle ~ and / characters for full JSON pointer support
   inline constexpr std::pair<sv, sv> tokenize_json_ptr(sv s)
   {
      if (s.empty()) {
         return {"", ""};
      }
      s.remove_prefix(1);
      if (s.find('/') == std::string::npos) {
         return {s, ""};
      }
      const auto i = s.find_first_of('/');
      return {s.substr(0, i), s.substr(i, s.size() - i)};
   }

   inline constexpr auto first_key(sv s) { return tokenize_json_ptr(s).first; }

   inline constexpr auto remove_first_key(sv s) { return tokenize_json_ptr(s).second; }

   inline constexpr std::pair<sv, sv> parent_last_json_ptrs(const sv s)
   {
      const auto i = s.find_last_of('/');
      return {s.substr(0, i), s.substr(i, s.size())};
   }

   inline auto split_json_ptr(sv s, std::vector<sv>& v)
   {
      const auto n = std::count(s.begin(), s.end(), '/');
      v.resize(n);
      for (auto i = 0; i < n; ++i) {
         std::tie(v[i], s) = tokenize_json_ptr(s);
      }
   }

   // TODO: handle ~ and / characters for full JSON pointer support
   template <auto& Str>
   inline constexpr auto split_json_ptr()
   {
      constexpr auto str = Str;
      constexpr auto N = std::count(str.begin(), str.end(), '/');
      std::array<sv, N> arr;
      sv s = str;
      for (auto i = 0; i < N; ++i) {
         std::tie(arr[i], s) = tokenize_json_ptr(s);
      }
      return arr;
   }

   inline constexpr auto json_ptrs(auto&&... args) { return std::array{sv{args}...}; }

   // must copy to allow mutation in constexpr context
   inline constexpr auto sort_json_ptrs(auto arr)
   {
      std::sort(arr.begin(), arr.end());
      return arr;
   }

   // input array must be sorted
   inline constexpr auto group_json_ptrs_impl(const auto& arr)
   {
      constexpr auto N = glz::tuple_size_v<std::decay_t<decltype(arr)>>;

      std::array<sv, N> first_keys;
      std::transform(arr.begin(), arr.end(), first_keys.begin(), first_key);

      std::array<sv, N> unique_keys{};
      const auto it = std::unique_copy(first_keys.begin(), first_keys.end(), unique_keys.begin());

      const auto n_unique = static_cast<size_t>(std::distance(unique_keys.begin(), it));

      std::array<size_t, N> n_items_per_group{};

      for (size_t i = 0; i < n_unique; ++i) {
         n_items_per_group[i] = std::count(first_keys.begin(), first_keys.end(), unique_keys[i]);
      }

      return glz::tuplet::tuple{n_items_per_group, n_unique, unique_keys};
   }

   template <auto Arr, std::size_t... Is>
   inline constexpr auto make_arrays(std::index_sequence<Is...>)
   {
      return glz::tuplet::make_tuple(std::pair{sv{}, std::array<sv, Arr[Is]>{}}...);
   }

   template <size_t N, auto& Arr>
   inline constexpr auto sub_group(const auto start)
   {
      std::array<sv, N> ret;
      std::transform(Arr.begin() + start, Arr.begin() + start + N, ret.begin(), remove_first_key);
      return ret;
   }

   template <auto& Arr>
   inline constexpr auto group_json_ptrs()
   {
      constexpr auto group_info = group_json_ptrs_impl(Arr);
      constexpr auto n_items_per_group = glz::get<0>(group_info);
      constexpr auto n_unique = glz::get<1>(group_info);
      constexpr auto unique_keys = glz::get<2>(group_info);

      auto arrs = make_arrays<n_items_per_group>(std::make_index_sequence<n_unique>{});
      size_t start{};

      for_each<n_unique>([&](auto I) {
         constexpr size_t n_items = n_items_per_group[I];

         glz::get<I>(arrs).first = unique_keys[I];
         glz::get<I>(arrs).second = glz::sub_group<n_items, Arr>(start);
         start += n_items;
      });

      return arrs;
   }

   template <class T, string_literal key_str>
   struct member_getter
   {
      static constexpr auto frozen_map = detail::make_map<T>();
      static constexpr auto member_it = frozen_map.find(key_str.sv());
   };

   // TODO support custom types
   template <class Root_t, string_literal ptr, class Expected_t = void>
   constexpr bool valid()
   {
      using V = std::decay_t<Root_t>;
      if constexpr (ptr.sv() == sv{""}) {
         return std::same_as<Expected_t, void> || std::same_as<V, Expected_t>;
      }
      else {
         constexpr auto tokens = tokenize_json_ptr(ptr.sv());
         constexpr auto key_str = tokens.first;
         constexpr auto rem_ptr = glz::string_literal_from_view<tokens.second.size()>(tokens.second);
         if constexpr (glz::detail::glaze_object_t<V>) {
            constexpr auto string_literal_key = glz::string_literal_from_view<key_str.size()>(key_str);
            using G = member_getter<V, string_literal_key>;
            if constexpr (G::member_it != G::frozen_map.end()) {
               constexpr auto& element = G::member_it->second;
               constexpr auto I = element.index();
               constexpr auto& member_ptr = get<I>(element);

               using mptr_t = std::decay_t<decltype(member_ptr)>;
               using T = detail::member_t<V, mptr_t>;
               if constexpr (is_specialization_v<T, includer>) {
                  return valid<file_include, rem_ptr, Expected_t>();
               }
               else {
                  return valid<T, rem_ptr, Expected_t>();
               }
            }
            else {
               return false;
            }
         }
         else if constexpr (glz::detail::writable_map_t<V>) {
            return valid<typename V::mapped_type, rem_ptr, Expected_t>();
         }
         else if constexpr (glz::detail::glaze_array_t<V>) {
            constexpr auto member_array = glz::detail::make_array<std::decay_t<V>>();
            constexpr auto optional_index = glz::detail::stoui(key_str); // TODO: Will not build if not int
            if constexpr (optional_index) {
               constexpr auto index = *optional_index;
               if constexpr (index >= 0 && index < member_array.size()) {
                  constexpr auto member = member_array[index];
                  constexpr auto member_ptr = std::get<member.index()>(member);
                  using sub_t = decltype(glz::detail::get_member(std::declval<V>(), member_ptr));
                  return valid<sub_t, rem_ptr, Expected_t>();
               }
               else {
                  return false;
               }
            }
            else {
               return false;
            }
         }
         else if constexpr (glz::detail::array_t<V>) {
            if (glz::detail::stoui(key_str)) {
               return valid<range_value_t<V>, rem_ptr, Expected_t>();
            }
            return false;
         }
         else if constexpr (glz::detail::nullable_t<V>) {
            using sub_t = decltype(*std::declval<V>());
            return valid<sub_t, ptr, Expected_t>();
         }
         else {
            return false;
         }
      }
   }

   [[nodiscard]] inline constexpr bool maybe_numeric_key(const sv key)
   {
      return key.find_first_not_of("0123456789") == std::string_view::npos;
   }

   template <string_literal Str, auto Opts = opts{}>
   [[nodiscard]] inline auto get_view_json(contiguous auto&& buffer)
   {
      static constexpr auto s = chars<Str>;

      static constexpr auto tokens = split_json_ptr<s>();
      static constexpr auto N = tokens.size();

      context ctx{};
      auto p = read_iterators<Opts>(ctx, buffer);

      auto it = p.first;
      auto end = p.second;

      // using span_t = std::span<std::remove_pointer_t<std::remove_reference_t<decltype(it)>>>;
      using span_t = std::span<const char>; // TODO: should be more generic, but currently broken with mingw
      using result_t = expected<span_t, parse_error>;

      auto start = it;

      if (bool(ctx.error)) [[unlikely]] {
         return result_t{unexpected(parse_error{ctx.error, 0})};
      }

      if constexpr (N == 0) {
         return result_t{span_t{it, end}};
      }
      else {
         using namespace glz::detail;

         skip_ws<Opts>(ctx, it, end);

         result_t ret;

         for_each<N>([&](auto I) {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            static constexpr auto key = std::get<I>(tokens);
            if constexpr (maybe_numeric_key(key)) {
               switch (*it) {
               case '{': {
                  ++it;
                  while (true) {
                     GLZ_SKIP_WS;
                     const auto k = parse_key(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     if (cx_string_cmp<key>(k)) {
                        GLZ_SKIP_WS;
                        GLZ_MATCH_COLON;
                        GLZ_SKIP_WS;

                        if constexpr (I == (N - 1)) {
                           ret = parse_value<Opts>(ctx, it, end);
                        }
                        return;
                     }
                     else {
                        skip_value<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]] {
                           return;
                        }
                        if (*it != ',') {
                           ctx.error = error_code::key_not_found;
                           return;
                        }
                        ++it;
                     }
                  }
               }
               case '[': {
                  ++it;
                  // Could optimize by counting commas
                  static constexpr auto n = stoui(key);
                  if constexpr (n) {
                     for_each<n.value()>([&](auto) {
                        skip_value<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]] {
                           return;
                        }
                        if (*it != ',') {
                           ctx.error = error_code::array_element_not_found;
                           return;
                        }
                        ++it;
                     });

                     GLZ_SKIP_WS;

                     if constexpr (I == (N - 1)) {
                        ret = parse_value<Opts>(ctx, it, end);
                     }
                     return;
                  }
                  else {
                     ctx.error = error_code::array_element_not_found;
                     return;
                  }
               }
               }
            }
            else {
               match<'{'>(ctx, it);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               while (it < end) {
                  GLZ_SKIP_WS;
                  const auto k = parse_key(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }

                  if (cx_string_cmp<key>(k)) {
                     GLZ_SKIP_WS;
                     GLZ_MATCH_COLON;
                     GLZ_SKIP_WS;

                     if constexpr (I == (N - 1)) {
                        ret = parse_value<Opts>(ctx, it, end);
                     }
                     return;
                  }
                  else {
                     skip_value<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     if (*it != ',') {
                        ctx.error = error_code::key_not_found;
                        return;
                     }
                     ++it;
                  }
               }
            }
         });

         if (bool(ctx.error)) [[unlikely]] {
            return result_t{unexpected(parse_error{ctx.error, size_t(it - start)})};
         }

         return ret;
      }
   }

   template <class T, string_literal Str, auto Opts = opts{}>
   [[nodiscard]] inline expected<T, parse_error> get_as_json(contiguous auto&& buffer)
   {
      const auto str = glz::get_view_json<Str>(buffer);
      if (str) {
         return glz::read_json<T>(*str);
      }
      return unexpected(str.error());
   }

   template <string_literal Str, auto Opts = opts{}>
   [[nodiscard]] inline expected<sv, parse_error> get_sv_json(contiguous auto&& buffer)
   {
      const auto s = glz::get_view_json<Str>(buffer);
      if (s) {
         return sv{reinterpret_cast<const char*>(s->data()), s->size()};
      }
      return unexpected(s.error());
   }
}
