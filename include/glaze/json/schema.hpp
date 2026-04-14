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
#include "glaze/api/std/variant.hpp"
#include "glaze/api/std/vector.hpp"
#include "glaze/api/tuplet.hpp"
#include "glaze/api/type_support.hpp"
#include "glaze/core/custom_meta.hpp"
#include "glaze/json/wrappers.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   namespace detail
   {
      enum struct defined_formats : uint32_t;
      struct ExtUnits final
      {
         std::optional<std::string_view> unitAscii{}; // ascii representation of the unit, e.g. "m^2" for square meters
         std::optional<std::string_view>
            unitUnicode{}; // unicode representation of the unit, e.g. "m²" for square meters
         constexpr bool operator==(const ExtUnits&) const noexcept = default;
      };
   }
   struct schema final
   {
      bool reflection_helper{}; // needed to support automatic reflection, because ref is a std::optional
      std::optional<std::string_view> ref{};
      std::optional<std::variant<std::string_view, std::vector<std::string_view>>> type{};
      using schema_number = std::optional<std::variant<int64_t, uint64_t, double>>;
      using schema_any = std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string_view>;
      // meta data keywords, ref: https://www.learnjsonschema.com/2020-12/meta-data/
      std::optional<std::string_view> title{};
      std::optional<std::string_view> description{};
      std::optional<schema_any> defaultValue{};
      std::optional<bool> deprecated{};
      std::optional<std::vector<std::string_view>> examples{};
      std::optional<bool> readOnly{};
      std::optional<bool> writeOnly{};
      // hereafter validation keywords, ref: https://www.learnjsonschema.com/2020-12/validation/
      std::optional<schema_any> constant{};
      // string only keywords
      std::optional<uint64_t> minLength{};
      std::optional<uint64_t> maxLength{};
      std::optional<std::string_view> pattern{};
      // https://www.learnjsonschema.com/2020-12/format-annotation/format/
      std::optional<detail::defined_formats> format{};
      // number only keywords
      schema_number minimum{};
      schema_number maximum{};
      schema_number exclusiveMinimum{};
      schema_number exclusiveMaximum{};
      schema_number multipleOf{};
      // object only keywords
      std::optional<uint64_t> minProperties{};
      std::optional<uint64_t> maxProperties{};
      // std::optional<std::map<std::string_view, std::vector<std::string_view>>> dependent_required{};
      std::optional<std::vector<std::string_view>> required{};
      // array only keywords
      std::optional<uint64_t> minItems{};
      std::optional<uint64_t> maxItems{};
      std::optional<uint64_t> minContains{};
      std::optional<uint64_t> maxContains{};
      std::optional<bool> uniqueItems{};
      // properties
      std::optional<std::vector<std::string_view>> enumeration{}; // enum

      // out of json schema specification
      std::optional<detail::ExtUnits> ExtUnits{};
      std::optional<bool>
         ExtAdvanced{}; // flag to indicate that the parameter is advanced and can be hidden in default views

      static constexpr auto schema_attributes{true}; // allowance flag to indicate metadata within glz::object(...)

      // TODO switch to using variants when we have write support to get rid of nulls
      // TODO We should be able to generate the json schema compiletime
      struct glaze
      {
         using T = schema;
         static constexpr std::array keys{"type", //
                                          "$ref", //
                                          "title", //
                                          "description", //
                                          "default", //
                                          "deprecated", //
                                          "examples", //
                                          "readOnly", //
                                          "writeOnly", //
                                          "const", //
                                          "minLength", //
                                          "maxLength", //
                                          "pattern", //
                                          "format", //
                                          "minimum", //
                                          "maximum", //
                                          "exclusiveMinimum", //
                                          "exclusiveMaximum", //
                                          "multipleOf", //
                                          "minProperties", //
                                          "maxProperties", //
                                          //"dependentRequired", //
                                          "required", //
                                          "minItems", //
                                          "maxItems", //
                                          "minContains", //
                                          "maxContains", //
                                          "uniqueItems", //
                                          "enum", //
                                          "ExtUnits", //
                                          "ExtAdvanced"};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
         static constexpr glz::tuple value = {&T::type, //
                                              &T::ref, //
                                              &T::title, //
                                              &T::description, //
                                              &T::defaultValue, //
                                              &T::deprecated, //
                                              unquoted<&T::examples>, //
                                              &T::readOnly, //
                                              &T::writeOnly, //
                                              &T::constant, //
                                              &T::minLength, //
                                              &T::maxLength, //
                                              &T::pattern, //
                                              &T::format, //
                                              &T::minimum, //
                                              &T::maximum, //
                                              &T::exclusiveMinimum, //
                                              &T::exclusiveMaximum, //
                                              &T::multipleOf, //
                                              &T::minProperties, //
                                              &T::maxProperties, //
                                              // &T::dependent_required, //
                                              &T::required, //
                                              &T::minItems, //
                                              &T::maxItems, //
                                              &T::minContains, //
                                              &T::maxContains, //
                                              &T::uniqueItems, //
                                              &T::enumeration, //
                                              &T::ExtUnits, //
                                              &T::ExtAdvanced};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
      };
   };

   namespace detail
   {
      struct schematic final
      {
         std::optional<std::variant<std::string_view, std::vector<std::string_view>>> type{};
         std::optional<std::map<std::string_view, schema, std::less<>>> properties{}; // glaze_object
         std::optional<std::vector<schematic>> prefixItems{}; // tuple
         std::optional<std::variant<bool, schema>> items{}; // array or tuple (false for tuple)
         std::optional<std::variant<bool, schema>> additionalProperties{}; // map
         std::optional<std::map<std::string_view, schematic, std::less<>>> defs{};
         std::optional<std::vector<schematic>> oneOf{};
         std::optional<std::vector<std::string_view>> required{};
         std::optional<std::vector<std::string_view>> examples{};
         schema attributes{};
      };

      enum struct defined_formats : uint32_t {
         datetime, //
         date, //
         time, //
         duration, //
         email, //
         idn_email, //
         hostname, //
         idn_hostname, //
         ipv4, //
         ipv6, //
         uri, //
         uri_reference, //
         iri, //
         iri_reference, //
         uuid, //
         uri_template, //
         json_pointer, //
         relative_json_pointer, //
         regex
      };
   }
}

template <>
struct glz::meta<glz::detail::defined_formats>
{
   static constexpr std::string_view name = "defined_formats";
   using enum glz::detail::defined_formats;
   static constexpr std::array keys{
      "date-time", "date",          "time", "duration",     "email",        "idn-email",
      "hostname",  "idn-hostname",  "ipv4", "ipv6",         "uri",          "uri-reference",
      "iri",       "iri-reference", "uuid", "uri-template", "json-pointer", "relative-json-pointer",
      "regex"};
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
   static constexpr std::array value{datetime, //
                                     date, //
                                     time, //
                                     duration, //
                                     email, //
                                     idn_email, //
                                     hostname, //
                                     idn_hostname, //
                                     ipv4, //
                                     ipv6, //
                                     uri, //
                                     uri_reference, //
                                     iri, //
                                     iri_reference, //
                                     uuid, //
                                     uri_template, //
                                     json_pointer, //
                                     relative_json_pointer, //
                                     regex};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
};

template <>
struct glz::meta<glz::detail::schematic>
{
   static constexpr std::string_view name = "glz::detail::schema";
   using T = detail::schematic;
   static constexpr std::array keys{"type", //
                                    "$ref", //
                                    "properties", //
                                    "prefixItems", //
                                    "items", //
                                    "additionalProperties", //
                                    "$defs", //
                                    "oneOf", //
                                    "examples", //
                                    "required", //
                                    "title", //
                                    "description", //
                                    "default", //
                                    "deprecated", //
                                    "readOnly", //
                                    "writeOnly", //
                                    "const", //
                                    "minLength", //
                                    "maxLength", //
                                    "pattern", //
                                    "format", //
                                    "minimum", //
                                    "maximum", //
                                    "exclusiveMinimum", //
                                    "exclusiveMaximum", //
                                    "multipleOf", //
                                    "minProperties", //
                                    "maxProperties", //
                                    // "dependentRequired", //
                                    "minItems", //
                                    "maxItems", //
                                    "minContains", //
                                    "maxContains", //
                                    "uniqueItems", //
                                    "enum", //
                                    "ExtUnits", //
                                    "ExtAdvanced"};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
   [[maybe_unused]] static constexpr glz::tuple value{
      &T::type, //
      [](auto&& s) -> auto& { return s.attributes.ref; }, //
      &T::properties, //
      &T::prefixItems, //
      &T::items, //
      &T::additionalProperties, //
      &T::defs, //
      &T::oneOf, //
      unquoted<&T::examples>, //
      &T::required, //
      [](auto&& s) -> auto& { return s.attributes.title; }, //
      [](auto&& s) -> auto& { return s.attributes.description; }, //
      [](auto&& s) -> auto& { return s.attributes.defaultValue; }, //
      [](auto&& s) -> auto& { return s.attributes.deprecated; }, //
      [](auto&& s) -> auto& { return s.attributes.readOnly; }, //
      [](auto&& s) -> auto& { return s.attributes.writeOnly; }, //
      [](auto&& s) -> auto& { return s.attributes.constant; }, //
      [](auto&& s) -> auto& { return s.attributes.minLength; }, //
      [](auto&& s) -> auto& { return s.attributes.maxLength; }, //
      [](auto&& s) -> auto& { return s.attributes.pattern; }, //
      [](auto&& s) -> auto& { return s.attributes.format; }, //
      [](auto&& s) -> auto& { return s.attributes.minimum; }, //
      [](auto&& s) -> auto& { return s.attributes.maximum; }, //
      [](auto&& s) -> auto& { return s.attributes.exclusiveMinimum; }, //
      [](auto&& s) -> auto& { return s.attributes.exclusiveMaximum; }, //
      [](auto&& s) -> auto& { return s.attributes.multipleOf; }, //
      [](auto&& s) -> auto& { return s.attributes.minProperties; }, //
      [](auto&& s) -> auto& { return s.attributes.maxProperties; }, //
      // [](auto&& s) -> auto& { return s.attributes.dependent_required; }, //
      [](auto&& s) -> auto& { return s.attributes.minItems; }, //
      [](auto&& s) -> auto& { return s.attributes.maxItems; }, //
      [](auto&& s) -> auto& { return s.attributes.minContains; }, //
      [](auto&& s) -> auto& { return s.attributes.maxContains; }, //
      [](auto&& s) -> auto& { return s.attributes.uniqueItems; }, //
      [](auto&& s) -> auto& { return s.attributes.enumeration; }, //
      [](auto&& s) -> auto& { return s.attributes.ExtUnits; }, //
      [](auto&& s) -> auto& { return s.attributes.ExtAdvanced; }};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
};

namespace glz
{
   // Note: has_custom_meta_v and detail::custom_read_input_type are defined in common.hpp

   namespace detail
   {
      template <class T = void>
      struct to_json_schema
      {
         template <auto Opts>
         static void op(auto& s, auto& defs)
         {
            // &T::member
            if constexpr (glaze_t<T> && std::is_member_object_pointer_v<meta_wrapper_t<T>>) {
               using val_t = std::remove_cvref_t<member_t<T, meta_wrapper_t<T>>>;
               to_json_schema<val_t>::template op<Opts>(s, defs);
               if constexpr (json_schema_t<T>) {
                  static constexpr auto schema_size = reflect<json_schema_type<T>>::size;
                  if constexpr (schema_size > 0) {
                     static constexpr sv member_name = get_name<meta_wrapper_v<T>>();
                     constexpr auto schema_index = [] {
                        const auto& schema_keys = reflect<json_schema_type<T>>::keys;
                        for (size_t i = 0; i < schema_size; ++i) {
                           if (schema_keys[i] == member_name) {
                              return i;
                           }
                        }
                        return schema_size;
                     }();
                     if constexpr (schema_index < schema_size) {
                        static const auto schema_v = json_schema_type<T>{};
                        s.attributes = get<schema_index>(to_tie(schema_v));
                     }
                  }
               }
            }
            else if constexpr (glaze_const_value_t<T>) { // &T::constexpr_member
               using constexpr_val_t = std::remove_cvref_t<member_t<T, meta_wrapper_t<T>>>;
               static constexpr auto val_v{*glz::meta_wrapper_v<T>};
               if constexpr (glz::glaze_enum_t<constexpr_val_t>) {
                  s.attributes.constant = glz::enum_name_v<val_v>;
               }
               else {
                  // General case, needs to be convertible to schema_any
                  s.attributes.constant = val_v;
               }
               to_json_schema<constexpr_val_t>::template op<Opts>(s, defs);
            }
            else if constexpr (has_mimic<T>) {
               // Type with mimic declaration: use the mimic type's schema
               to_json_schema<mimic_type<T>>::template op<Opts>(s, defs);
            }
            else if constexpr (detail::custom_read_input_type<T>::has_custom) {
               // Type with custom read/write: infer schema from input type
               using InputType = typename detail::custom_read_input_type<T>::type;
               to_json_schema<InputType>::template op<Opts>(s, defs);
            }
            else {
               s.type = std::vector<sv>{"number", "string", "boolean", "object", "array", "null"};
            }
         }
      };

      template <>
      struct to_json_schema<void>
      {
         template <auto Opts>
         static void op(auto& s, auto&)
         {
            s.type = sv{"null"};
         }
      };

      template <class T>
         requires(std::same_as<T, bool> || std::same_as<T, std::vector<bool>::reference> ||
                  std::same_as<T, std::vector<bool>::const_reference>)
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto&)
         {
            s.type = sv{"boolean"};
         }
      };

      template <num_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto&)
         {
            using V = std::decay_t<T>;
            if constexpr (std::integral<V>) {
               s.type = sv{"integer"};
               s.attributes.minimum = static_cast<std::int64_t>(std::numeric_limits<V>::lowest());
               s.attributes.maximum = static_cast<std::uint64_t>((std::numeric_limits<V>::max)());
            }
            else {
               s.type = sv{"number"};
               s.attributes.minimum = std::numeric_limits<V>::lowest();
               s.attributes.maximum = (std::numeric_limits<V>::max)();
            }
         }
      };

      template <class T>
         requires str_t<T> || char_t<T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto&)
         {
            s.type = sv{"string"};
         }
      };

      template <always_null_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto&)
         {
            s.type = sv{"null"};
            s.attributes.constant = std::monostate{};
         }
      };

      template <glaze_enum_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto&)
         {
            s.type = sv{"string"};

            // TODO use oneOf instead of enum to handle doc comments
            static constexpr auto N = reflect<T>::size;
            // s.enumeration = std::vector<std::string_view>(N);
            // for_each<N>([&]<auto I>() {
            //    static constexpr auto item = std::get<I>(meta_v<V>);
            //    (*s.enumeration)[I] = std::get<0>(item);
            // });
            s.oneOf = std::vector<schematic>(N);
            for_each<N>([&]<auto I>() {
               auto& enumeration = (*s.oneOf)[I];
               // Do not override if already set
               if (!enumeration.attributes.constant.has_value()) {
                  enumeration.attributes.constant = reflect<T>::keys[I];
               }
               if (!enumeration.attributes.title.has_value()) {
                  enumeration.attributes.title = reflect<T>::keys[I];
               }
            });
         }
      };

      template <class T>
      struct to_json_schema<basic_raw_json<T>>
      {
         template <auto Opts>
         static void op(auto& s, auto&)
         {
            s.type = std::vector<sv>{"number", "string", "boolean", "object", "array", "null"};
         }
      };

      // Whether a type can be represented as an inline JSON Schema primitive
      template <class V>
      constexpr bool schema_primitive = std::same_as<V, bool> || num_t<V> || str_t<V> || char_t<V>;

      // Unwrap nested nullable types (e.g. std::optional<std::optional<std::string>> → std::string)
      template <class T>
      struct unwrap_nullable
      {
         using type = T;
      };

      template <nullable_t T>
      struct unwrap_nullable<T>
      {
         using type = typename unwrap_nullable<std::decay_t<decltype(*std::declval<T>())>>::type;
      };

      template <class T>
      using unwrap_nullable_t = typename unwrap_nullable<T>::type;

      // Build an inline schema for a primitive type
      template <class V>
      schema make_primitive_schema()
      {
         schema s{};
         if constexpr (std::same_as<V, bool>) {
            s.type = sv{"boolean"};
         }
         else if constexpr (str_t<V> || char_t<V>) {
            s.type = sv{"string"};
         }
         else if constexpr (std::integral<V>) {
            s.type = sv{"integer"};
            s.minimum = static_cast<std::int64_t>(std::numeric_limits<V>::lowest());
            s.maximum = static_cast<std::uint64_t>((std::numeric_limits<V>::max)());
         }
         else if constexpr (std::floating_point<V>) {
            s.type = sv{"number"};
            s.minimum = std::numeric_limits<V>::lowest();
            s.maximum = (std::numeric_limits<V>::max)();
         }
         return s;
      }

      template <array_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto& defs)
         {
            using V = std::decay_t<range_value_t<std::decay_t<T>>>;
            // When concatenate is true (default), arrays of pairs serialize as objects
            if constexpr (pair_t<V> && check_concatenate(Opts)) {
               using ValueType = std::decay_t<glz::tuple_element_t<1, V>>;
               s.type = sv{"object"};
               if constexpr (schema_primitive<ValueType>) {
                  s.additionalProperties = make_primitive_schema<ValueType>();
               }
               else {
                  auto& def = defs[name_v<ValueType>];
                  if (!def.type) {
                     to_json_schema<ValueType>::template op<Opts>(def, defs);
                  }
                  s.additionalProperties = schema{true, join_v<chars<"#/$defs/">, name_v<ValueType>>};
               }
            }
            else {
               s.type = sv{"array"};
               if constexpr (has_fixed_size_container<std::decay_t<T>>) {
                  s.attributes.maxItems = get_size<std::decay_t<T>>();
               }
               if constexpr (schema_primitive<V>) {
                  s.items = make_primitive_schema<V>();
               }
               else {
                  auto& def = defs[name_v<V>];
                  if (!def.type) {
                     to_json_schema<V>::template op<Opts>(def, defs);
                  }
                  s.items = schema{true, join_v<chars<"#/$defs/">, name_v<V>>};
               }
            }
         }
      };

      template <writable_map_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto& defs)
         {
            using V = std::decay_t<glz::tuple_element_t<1, range_value_t<std::decay_t<T>>>>;
            s.type = sv{"object"};
            if constexpr (schema_primitive<V>) {
               s.additionalProperties = make_primitive_schema<V>();
            }
            else {
               auto& def = defs[name_v<V>];
               if (!def.type) {
                  to_json_schema<V>::template op<Opts>(def, defs);
               }
               s.additionalProperties = schema{true, join_v<chars<"#/$defs/">, name_v<V>>};
            }
         }
      };

      template <nullable_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto& defs)
         {
            using V = std::decay_t<decltype(*std::declval<std::decay_t<T>>())>;
            to_json_schema<V>::template op<Opts>(s, defs);
            // to_json_schema above should populate the correct type, let's throw if it wasn't set
            auto& type_val = *s.type;
            if (auto* str = std::get_if<sv>(&type_val)) {
               if (*str != "null") {
                  type_val = std::vector<sv>{*str, "null"};
               }
            }
            else {
               auto& vec = std::get<std::vector<sv>>(type_val);
               if (std::find(vec.begin(), vec.end(), "null") == vec.end()) {
                  vec.emplace_back("null");
               }
            }
         }
      };

      template <is_variant T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto& defs)
         {
            static constexpr auto N = std::variant_size_v<T>;
            using type_counts = variant_type_count<T>;
            std::vector<sv> type_vec{};
            if constexpr (type_counts::n_number) {
               type_vec.emplace_back("number");
            }
            if constexpr (type_counts::n_string) {
               type_vec.emplace_back("string");
            }
            if constexpr (type_counts::n_bool) {
               type_vec.emplace_back("boolean");
            }
            if constexpr (type_counts::n_object) {
               type_vec.emplace_back("object");
            }
            if constexpr (type_counts::n_array) {
               type_vec.emplace_back("array");
            }
            if constexpr (type_counts::n_null) {
               type_vec.emplace_back("null");
            }
            if (type_vec.size() == 1) {
               s.type = type_vec[0];
            }
            else {
               s.type = std::move(type_vec);
            }
            s.oneOf = std::vector<schematic>(N);

            const auto& ids = ids_v<T>;

            for_each<N>([&]<auto I>() {
               using V = std::decay_t<std::variant_alternative_t<I, T>>;
               auto& schema_val = (*s.oneOf)[I];

               // Use $ref for complex types to avoid duplicating large definitions in oneOf.
               // Exception: tagged variant object alternatives must remain inline because the tag
               // discriminator property is added to the oneOf entry alongside the object's own
               // properties, and additionalProperties:false in a $ref'd schema would reject the tag.
               constexpr bool is_complex = glaze_object_t<V> || reflectable<V> || array_t<V> || writable_map_t<V>;
               constexpr bool is_tagged_object = (glaze_object_t<V> || reflectable<V>) && !tag_v<T>.empty();

               if constexpr (is_complex && !is_tagged_object) {
                  auto& def = defs[name_v<V>];
                  if (!def.type) {
                     to_json_schema<V>::template op<Opts>(def, defs);
                  }
                  schema_val.attributes.ref = join_v<chars<"#/$defs/">, name_v<V>>;
               }
               else {
                  to_json_schema<V>::template op<Opts>(schema_val, defs);
               }

               if (not schema_val.attributes.title) {
                  schema_val.attributes.title = ids[I];
               }

               if constexpr ((glaze_object_t<V> || reflectable<V>) && not tag_v<T>.empty()) {
                  if (not schema_val.required) {
                     schema_val.required = std::vector<sv>{}; // allocate
                  }
                  schema_val.required->emplace_back(tag_v<T>);
                  auto& tag = (*schema_val.properties)[tag_v<T>];
                  tag.constant = ids[I];
               }
            });
         }
      };

      template <class T>
         requires glaze_array_t<std::decay_t<T>> || tuple_t<std::decay_t<T>> || is_std_tuple<std::decay_t<T>>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto& defs)
         {
            using V = std::decay_t<T>;
            s.type = sv{"array"};
            static constexpr auto N = []() constexpr {
               if constexpr (glaze_array_t<V>) {
                  return glz::tuple_size_v<meta_t<V>>;
               }
               else {
                  return glz::tuple_size_v<V>;
               }
            }();
            s.prefixItems = std::vector<schematic>(N);
            for_each<N>([&]<auto I>() {
               if constexpr (glaze_array_t<V>) {
                  using element_t = std::decay_t<member_t<V, decltype(glz::get<I>(meta_v<V>))>>;
                  auto& item_schema = (*s.prefixItems)[I];
                  to_json_schema<element_t>::template op<Opts>(item_schema, defs);
               }
               else {
                  using element_t = std::decay_t<glz::tuple_element_t<I, V>>;
                  auto& item_schema = (*s.prefixItems)[I];
                  to_json_schema<element_t>::template op<Opts>(item_schema, defs);
               }
            });
            s.items = false;
            s.attributes.maxItems = N;
         }
      };

      template <class T>
      consteval bool json_schema_matches_object_keys()
      {
         if constexpr (json_schema_t<T> && (count_members<json_schema_type<T>> > 0)) {
            constexpr auto& json_schema_names = member_names<json_schema_type<T>>;
            auto fields = reflect<T>::keys;
            std::sort(fields.begin(), fields.end());

            for (const auto& key : json_schema_names) {
               if (std::binary_search(fields.begin(), fields.end(), key)) {
                  continue;
               }
               // For types with modify, json_schema members may use original C++ member names
               if constexpr (modify_t<std::decay_t<T>>) {
                  constexpr auto& original_fields = member_names<std::decay_t<T>>;
                  bool found = false;
                  for (const auto& field : original_fields) {
                     if (field == key) {
                        found = true;
                        break;
                     }
                  }
                  if (found) {
                     continue;
                  }
               }
               return false;
            }
            return true;
         }
         else {
            return true;
         }
      }

      auto consteval has_slash(std::string_view str) noexcept -> bool
      {
         return std::ranges::any_of(str, [](const auto character) { return character == '/'; });
      }
      template <const std::string_view& ref>
      auto consteval validate_ref() noexcept -> void
      {
#if (__cpp_static_assert >= 202306L) && (__cplusplus > 202302L)
         static_assert(!has_slash(ref),
                       join_v<chars<"Slash in name: \"">, ref, chars<"\" in json schema references is not allowed">>);
#else
         static_assert(!has_slash(ref), "Slashes in json schema references are not allowed");
#endif
      }

      template <class T>
         requires((glaze_object_t<T> || reflectable<T>))
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto& defs)
         {
            static_assert(json_schema_matches_object_keys<T>());

            s.type = sv{"object"};

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

            static constexpr auto N = reflect<T>::size;
            static constexpr auto json_schema_size = reflect<json_schema_type<T>>::size;
            auto req = s.required.value_or(std::vector<std::string_view>{});

            s.properties = std::map<sv, schema, std::less<>>();
            for_each<N>([&]<auto I>() {
               using val_t = std::decay_t<refl_t<T, I>>;

               static constexpr sv key = reflect<T>::keys[I];
               if constexpr (requires_key<T, val_t, Opts>(key)) {
                  req.emplace_back(key);
               }

               schema ref_val{};
               if constexpr (N > 0 && json_schema_size > 0) {
                  // We need schema_index to be the index within json_schema_type<T>,
                  // but this struct may have fewer keys that don't match the full set of keys in the struct T
                  // we therefore can't use `decode_hash_with_size` for either structure.
                  // Instead we just loop over the keys, looking for a match:

                  constexpr auto schema_index = [] {
                     const auto& schema_keys = reflect<json_schema_type<T>>::keys;
                     for (size_t i = 0; i < json_schema_size; ++i) {
                        if (schema_keys[i] == key) {
                           return i;
                        }
                     }
                     // For modify types, try matching by original C++ member name
                     if constexpr (modify_t<std::decay_t<T>> && I < count_members<std::decay_t<T>>) {
                        constexpr sv original_name = member_names<std::decay_t<T>>[I];
                        if constexpr (original_name != key) {
                           for (size_t i = 0; i < json_schema_size; ++i) {
                              if (schema_keys[i] == original_name) {
                                 return i;
                              }
                           }
                        }
                     }
                     return json_schema_size;
                  }();

                  if constexpr (schema_index < json_schema_size) {
                     // Experimented with a to_array approach, but the compilation times were significantly higher
                     // even when converting this access to a run-time access
                     // Tested with both creating a std::array and a heap allocated C-style array and storing in a
                     // unique_ptr
                     static const auto schema_v = json_schema_type<T>{};
                     ref_val = get<schema_index>(to_tie(schema_v));
                  }
               }

               // Determine if this type can be inlined (bool, string, or nullable versions)
               using inner_val_t = unwrap_nullable_t<val_t>;
               constexpr bool can_inline =
                  std::same_as<inner_val_t, bool> || str_t<inner_val_t> || char_t<inner_val_t>;

               if constexpr (can_inline) {
                  if (!ref_val.ref) {
                     // Inline the type directly instead of using $defs/$ref
                     if constexpr (std::same_as<inner_val_t, bool>) {
                        if constexpr (nullable_t<val_t>) {
                           ref_val.type = std::vector<sv>{sv{"boolean"}, sv{"null"}};
                        }
                        else {
                           ref_val.type = sv{"boolean"};
                        }
                     }
                     else {
                        if constexpr (nullable_t<val_t>) {
                           ref_val.type = std::vector<sv>{sv{"string"}, sv{"null"}};
                        }
                        else {
                           ref_val.type = sv{"string"};
                        }
                     }
                  }
                  else {
                     // User explicitly set $ref via json_schema metadata, honor it
                     auto& def = defs[name_v<val_t>];
                     if (!def.type) {
                        to_json_schema<val_t>::template op<Opts>(def, defs);
                     }
                  }
               }
               else {
                  if (!ref_val.ref) {
                     validate_ref<name_v<val_t>>();
                     ref_val.ref = join_v<chars<"#/$defs/">, name_v<val_t>>;
                  }

                  auto& def = defs[name_v<val_t>];
                  if (!def.type) {
                     to_json_schema<val_t>::template op<Opts>(def, defs);
                  }
               }

               (*s.properties)[key] = ref_val;
            });
            if (!req.empty()) {
               s.required = std::move(req);
            }
            s.additionalProperties = false;
         }
      };
   }

   // Moved definition outside of write_json_schema to fix MSVC bug
   template <class Opts>
   struct opts_write_type_info_off : std::decay_t<Opts>
   {
      bool write_type_info = false;
   };

   template <class T, auto Opts = opts{}, class Buffer>
   [[nodiscard]] error_ctx write_json_schema(Buffer&& buffer)
   {
      detail::schematic s{};
      s.defs.emplace();
      detail::to_json_schema<std::decay_t<T>>::template op<Opts>(s, *s.defs);
      s.attributes.title = name_v<T>;
      // Making this static constexpr options to fix MSVC bug
      static constexpr opts options = opts_write_type_info_off<decltype(Opts)>{{Opts}};
      return write<options>(std::move(s), std::forward<Buffer>(buffer));
   }

   template <class T, auto Opts = opts{}>
   [[nodiscard]] glz::expected<std::string, error_ctx> write_json_schema()
   {
      std::string buffer{};
      const error_ctx ec = write_json_schema<T, Opts>(buffer);
      if (bool(ec)) [[unlikely]] {
         return glz::unexpected(ec);
      }
      return {buffer};
   }
}
