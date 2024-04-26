// Glaze Library
// For the license information refer to glaze.hpp
#pragma once

// We include naming metas for standard types for consistency across compilers
#include "glaze/api/std/array.hpp"
#include "glaze/api/std/deque.hpp"
#include "glaze/api/std/functional.hpp"
#include "glaze/api/std/list.hpp"
#include "glaze/api/std/map.hpp"
#include "glaze/api/std/optional.hpp"
#include "glaze/api/std/shared_ptr.hpp"
#include "glaze/api/std/string.hpp"
#include "glaze/api/std/tuple.hpp"
#include "glaze/api/std/unique_ptr.hpp"
#include "glaze/api/std/unordered_map.hpp"
#include "glaze/api/std/variant.hpp"
#include "glaze/api/std/vector.hpp"
#include "glaze/api/tuplet.hpp"
#include "glaze/api/type_support.hpp"
#include "glaze/json/quoted.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   struct schema final
   {
      bool reflection_helper{}; // needed to support automatic reflection, because ref is a std::optional
      std::optional<std::string_view> ref{};
      using schema_number = std::optional<std::variant<std::int64_t, std::uint64_t, double>>;
      using schema_any = std::variant<bool, std::int64_t, std::uint64_t, double, std::string_view>;
      // meta data keywords, ref: https://www.learnjsonschema.com/2020-12/meta-data/
      std::optional<std::string_view> title{};
      std::optional<std::string_view> description{};
      std::optional<schema_any> defaultValue{};
      std::optional<bool> deprecated{};
      std::optional<std::span<const std::string_view>> examples{};
      std::optional<bool> readOnly{};
      std::optional<bool> writeOnly{};
      // hereafter validation keywords, ref: https://www.learnjsonschema.com/2020-12/validation/
      std::optional<std::variant<bool, std::string_view>> constant{};
      // string only keywords
      std::optional<std::uint64_t> minLength{};
      std::optional<std::uint64_t> maxLength{};
      std::optional<std::string_view> pattern{};
      // number only keywords
      schema_number minimum{};
      schema_number maximum{};
      schema_number exclusiveMinimum{};
      schema_number exclusiveMaximum{};
      schema_number multipleOf{};
      // object only keywords
      std::optional<std::uint64_t> minProperties{};
      std::optional<std::uint64_t> maxProperties{};
      //      std::optional<std::map<std::string_view, std::vector<std::string_view>>> dependent_required{};
      std::optional<std::span<const std::string_view>> required{};
      // array only keywords
      std::optional<std::uint64_t> minItems{};
      std::optional<std::uint64_t> maxItems{};
      std::optional<std::uint64_t> minContains{};
      std::optional<std::uint64_t> maxContains{};
      std::optional<bool> uniqueItems{};
      // properties
      std::optional<std::span<const std::string_view>> enumeration{}; // enum

      static constexpr auto schema_attributes{true}; // allowance flag to indicate metadata within glz::object(...)

      // TODO switch to using variants when we have write support to get rid of nulls
      // TODO We should be able to generate the json schema compiletime
      struct glaze
      {
         using T = schema;
         static constexpr auto value = glz::object("$ref", &T::ref, //
                                                   "title", &T::title, //
                                                   "description", &T::description, //
                                                   "default", &T::defaultValue, //
                                                   "deprecated", &T::deprecated, //
                                                   "examples", raw<&T::examples>, //
                                                   "readOnly", &T::readOnly, //
                                                   "writeOnly", &T::writeOnly, //
                                                   "const", &T::constant, //
                                                   "minLength", &T::minLength, //
                                                   "maxLength", &T::maxLength, //
                                                   "pattern", &T::pattern, //
                                                   "minimum", &T::minimum, //
                                                   "maximum", &T::maximum, //
                                                   "exclusiveMinimum", &T::exclusiveMinimum, //
                                                   "exclusiveMaximum", &T::exclusiveMaximum, //
                                                   "multipleOf", &T::multipleOf, //
                                                   "minProperties", &T::minProperties, //
                                                   "maxProperties", &T::maxProperties, //
                                                   //               "dependentRequired", &T::dependent_required, //
                                                   "required", &T::required, //
                                                   "minItems", &T::minItems, //
                                                   "maxItems", &T::maxItems, //
                                                   "minContains", &T::minContains, //
                                                   "maxContains", &T::maxContains, //
                                                   "uniqueItems", &T::uniqueItems, //
                                                   "enum", &T::enumeration);
      };
   };

   namespace detail
   {
      struct schematic final
      {
         std::optional<std::vector<std::string_view>> type{};
         std::optional<std::string_view> constant{};
         std::optional<std::string_view> description{};
         std::optional<std::map<std::string_view, schema, std::less<>>> properties{}; // glaze_object
         std::optional<schema> items{}; // array
         std::optional<std::variant<bool, schema>> additionalProperties{}; // map
         std::optional<std::map<std::string_view, schematic, std::less<>>> defs{};
         std::optional<std::vector<std::string_view>> enumeration{}; // enum
         std::optional<std::vector<schematic>> oneOf{};
         std::optional<std::vector<std::string_view>> required{};
         std::optional<std::vector<std::string_view>> examples{};
      };
   }
}

template <>
struct glz::meta<glz::detail::schematic>
{
   static constexpr std::string_view name = "glz::detail::schema";
   using T = detail::schematic;
   static constexpr auto value = glz::object("type", &T::type, //
                                             "description", &T::description, //
                                             "properties", &T::properties, //
                                             "items", &T::items, //
                                             "additionalProperties", &T::additionalProperties, //
                                             "$defs", &T::defs, //
                                             "enum", &T::enumeration, //
                                             "oneOf", &T::oneOf, //
                                             "const", &T::constant, //
                                             "required", &T::required, //
                                             "examples", raw<&T::examples>);
};

namespace glz
{
   namespace detail
   {
      // The reflection schema map makes a map of all schema types within a glz::json_schema
      // and returns a map of these schemas with their reflected names.
      template <class T>
      consteval auto make_reflection_schema_map()
      {
         auto schema_instance = json_schema_v<T>;
         auto tuple = to_tuple(schema_instance);
         using V = std::decay_t<decltype(tuple)>;
         constexpr auto N = glz::tuple_size_v<V>;
         if constexpr (N > 0) {
            constexpr auto& names = member_names<json_schema_type<T>>;
            return [&]<size_t... I>(std::index_sequence<I...>) {
               return detail::normal_map<sv, schema, N>(
                  std::array<std::pair<sv, schema>, N>{std::pair{names[I], std::get<I>(tuple)}...});
            }(std::make_index_sequence<N>{});
         }
         else {
            return detail::normal_map<sv, schema, 0>(std::array<std::pair<sv, schema>, 0>{});
         }
      }

      template <class T = void>
      struct to_json_schema
      {
         template <auto Opts>
         static void op(auto& s, auto&) noexcept
         {
            s.type = {"number", "string", "boolean", "object", "array", "null"};
         }
      };

      template <class T>
         requires(std::same_as<T, bool> || std::same_as<T, std::vector<bool>::reference> ||
                  std::same_as<T, std::vector<bool>::const_reference>)
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto&) noexcept
         {
            s.type = {"boolean"};
         }
      };

      template <num_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto&) noexcept
         {
            if constexpr (std::integral<T>) {
               s.type = {"integer"};
            }
            else {
               s.type = {"number"};
            }
         }
      };

      template <class T>
         requires str_t<T> || char_t<T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto&) noexcept
         {
            s.type = {"string"};
         }
      };

      template <always_null_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto&) noexcept
         {
            s.type = {"null"};
         }
      };

      template <glaze_enum_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto&) noexcept
         {
            s.type = {"string"};

            // TODO use oneOf instead of enum to handle doc comments
            using V = std::decay_t<T>;
            static constexpr auto N = glz::tuple_size_v<meta_t<V>>;
            // s.enumeration = std::vector<std::string_view>(N);
            // for_each<N>([&](auto I) {
            //    static constexpr auto item = std::get<I>(meta_v<V>);
            //    (*s.enumeration)[I] = std::get<0>(item);
            // });
            s.oneOf = std::vector<schematic>(N);
            for_each<N>([&](auto I) {
               static constexpr auto item = get<I>(meta_v<V>);
               using T0 = std::decay_t<decltype(get<0>(item))>;
               auto& enumeration = (*s.oneOf)[I];
               enumeration.constant = get_enum_key<V, I>();
               static constexpr size_t member_index = std::is_enum_v<T0> ? 0 : 1;
               static constexpr size_t comment_index = member_index + 1;
               constexpr auto Size = glz::tuple_size_v<decltype(item)>;
               if constexpr (Size > comment_index) {
                  enumeration.description = get<comment_index>(item);
               }
            });
         }
      };

      template <class T>
      struct to_json_schema<basic_raw_json<T>>
      {
         template <auto Opts>
         static void op(auto& s, auto&) noexcept
         {
            s.type = {"number", "string", "boolean", "object", "array", "null"};
         }
      };

      template <array_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto& defs) noexcept
         {
            using V = std::decay_t<range_value_t<std::decay_t<T>>>;
            s.type = {"array"};
            auto& def = defs[name_v<V>];
            if (!def.type) {
               to_json_schema<V>::template op<Opts>(def, defs);
            }
            s.items = schema{true, join_v<chars<"#/$defs/">, name_v<V>>};
         }
      };

      template <writable_map_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto& defs) noexcept
         {
            using V = std::decay_t<glz::tuple_element_t<1, range_value_t<std::decay_t<T>>>>;
            s.type = {"object"};
            auto& def = defs[name_v<V>];
            if (!def.type) {
               to_json_schema<V>::template op<Opts>(def, defs);
            }
            s.additionalProperties = schema{true, join_v<chars<"#/$defs/">, name_v<V>>};
         }
      };

      template <nullable_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto& defs) noexcept
         {
            using V = std::decay_t<decltype(*std::declval<std::decay_t<T>>())>;
            to_json_schema<V>::template op<Opts>(s, defs);
            auto& type = *s.type;
            auto it = std::find_if(type.begin(), type.end(), [&](const auto& str) { return str == "null"; });
            if (it == type.end()) {
               type.emplace_back("null");
            }
         }
      };

      template <is_variant T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto& defs) noexcept
         {
            static constexpr auto N = std::variant_size_v<T>;
            using type_counts = variant_type_count<T>;
            s.type = std::vector<sv>{};
            if constexpr (type_counts::n_number) {
               (*s.type).emplace_back("number");
            }
            if constexpr (type_counts::n_string) {
               (*s.type).emplace_back("string");
            }
            if constexpr (type_counts::n_bool) {
               (*s.type).emplace_back("boolean");
            }
            if constexpr (type_counts::n_object) {
               (*s.type).emplace_back("object");
            }
            if constexpr (type_counts::n_array) {
               (*s.type).emplace_back("array");
            }
            if constexpr (type_counts::n_null) {
               (*s.type).emplace_back("null");
            }
            s.oneOf = std::vector<schematic>(N);

            for_each<N>([&](auto I) {
               using V = std::decay_t<std::variant_alternative_t<I, T>>;
               auto& schema_val = (*s.oneOf)[I];
               to_json_schema<V>::template op<Opts>(schema_val, defs);
               if constexpr ((glaze_object_t<V> || reflectable<V>)&&!tag_v<T>.empty()) {
                  if (!schema_val.required) {
                     schema_val.required = std::vector<sv>{};
                  }
                  schema_val.required->emplace_back(tag_v<T>);
                  auto& tag = (*schema_val.properties)[tag_v<T>];
                  tag.constant = ids_v<T>[I];
               }
            });
         }
      };

      template <class T>
         requires glaze_array_t<std::decay_t<T>> || tuple_t<std::decay_t<T>>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto&) noexcept
         {
            // TODO: Actually handle this. We can specify a schema per item in items
            //      We can also do size restrictions on static arrays
            s.type = {"array"};
         }
      };

      template <class T>
      inline constexpr auto glaze_names = []() {
         constexpr auto N = reflection_count<T>;
         std::array<sv, N> names{};
         for_each<N>([&](auto I) {
            using Element = glaze_tuple_element<I, N, T>;
            names[I] = key_name<I, T, Element::use_reflection>;
         });
         return names;
      }();

      template <class T>
      consteval bool json_schema_matches_object_keys()
      {
         if constexpr (json_schema_t<T> && (count_members<json_schema_type<T>> > 0)) {
            constexpr auto& json_schema_names = member_names<json_schema_type<T>>;
            auto fields = glaze_names<T>;
            std::sort(fields.begin(), fields.end());

            for (const auto& key : json_schema_names) {
               if (!std::binary_search(fields.begin(), fields.end(), key)) {
                  return false;
               }
            }
            return true;
         }
         else {
            return true;
         }
      }

      template <class T>
         requires((glaze_object_t<T> || reflectable<T>))
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto& defs) noexcept
         {
            static_assert(json_schema_matches_object_keys<T>());

            s.type = {"object"};

            using V = std::decay_t<T>;

            if constexpr (requires { meta<V>::required; }) {
               if (!s.required) {
                  s.required = std::vector<sv>{};
               }
               for (auto& v : meta<V>::required) {
                  s.required->emplace_back(v);
               }
            }

            if constexpr (requires { meta<V>::examples; }) {
               if (!s.examples) {
                  s.examples = std::vector<sv>{};
               }
               for (auto& v : meta<V>::examples) {
                  s.examples->emplace_back(v);
               }
            }

            static constexpr auto N = reflection_count<T>;

            static constexpr auto schema_map = make_reflection_schema_map<T>();

            s.properties = std::map<sv, schema, std::less<>>();
            for_each<N>([&](auto I) {
               using Element = glaze_tuple_element<I, N, T>;
               static constexpr size_t member_index = Element::member_index;
               using val_t = std::decay_t<typename Element::type>;

               auto& def = defs[name_v<val_t>];
               if (!def.type) {
                  to_json_schema<val_t>::template op<Opts>(def, defs);
               }

               constexpr sv key = key_name<I, T, Element::use_reflection>;

               schema ref_val{};
               if constexpr (schema_map.size()) {
                  static constexpr auto it = schema_map.find(key);
                  if constexpr (it != schema_map.end()) {
                     ref_val = it->second;
                  }
               }
               if (!ref_val.ref) {
                  ref_val.ref = join_v<chars<"#/$defs/">, name_v<val_t>>;
               }

               static constexpr size_t comment_index = member_index + 1;
               static constexpr auto Size = glz::tuple_size_v<typename Element::Item>;

               if constexpr (Size > comment_index && glaze_object_t<T>) {
                  static constexpr auto item = glz::get<I>(meta_v<V>);
                  using additional_data_type = decltype(get<comment_index>(item));
                  if constexpr (std::is_convertible_v<additional_data_type, sv>) {
                     ref_val.description = get<comment_index>(item);
                  }
                  else if constexpr (std::is_convertible_v<additional_data_type, schema>) {
                     ref_val = get<comment_index>(item);
                     ref_val.ref = join_v<chars<"#/$defs/">, name_v<val_t>>;
                  }
               }

               (*s.properties)[key] = ref_val;
            });
            s.additionalProperties = false;
         }
      };

   }

   template <class T, opts Opts = opts{}, class Buffer>
   inline void write_json_schema(Buffer&& buffer) noexcept
   {
      detail::schematic s{};
      s.defs.emplace();
      detail::to_json_schema<std::decay_t<T>>::template op<Opts>(s, *s.defs);
      write<opt_false<Opts, &opts::write_type_info>>(std::move(s), std::forward<Buffer>(buffer));
   }

   template <class T, opts Opts = opts{}>
   [[nodiscard]] inline auto write_json_schema() noexcept
   {
      std::string buffer{};
      write_json_schema<T, Opts>(buffer);
      return buffer;
   }
}
