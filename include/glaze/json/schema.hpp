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
   // Heap-allocated nullable wrapper. Same API as std::optional<T> but stores the value
   // behind a unique_ptr (8 bytes inline) instead of inline (sizeof(T) + padding).
   // Used by schema to reduce its per-instance size for sparsely populated fields.
   template <class T>
   struct boxed final
   {
      using value_type = T;

      std::unique_ptr<T> ptr{};

      boxed() noexcept = default;

      boxed(const boxed& other) : ptr(other.ptr ? std::make_unique<T>(*other.ptr) : nullptr) {}
      boxed& operator=(const boxed& other)
      {
         if (this != &other) {
            ptr = other.ptr ? std::make_unique<T>(*other.ptr) : nullptr;
         }
         return *this;
      }

      boxed(boxed&&) noexcept = default;
      boxed& operator=(boxed&&) noexcept = default;

      // Converting constructor: accepts any type constructible to T
      template <class U>
         requires(!std::same_as<std::decay_t<U>, boxed> && std::is_constructible_v<T, U&&>)
      boxed(U&& val) : ptr(std::make_unique<T>(std::forward<U>(val)))
      {}

      // Converting assignment
      template <class U>
         requires(!std::same_as<std::decay_t<U>, boxed> && std::is_constructible_v<T, U&&>)
      boxed& operator=(U&& val)
      {
         ptr = std::make_unique<T>(std::forward<U>(val));
         return *this;
      }

      explicit operator bool() const noexcept { return ptr != nullptr; }
      bool has_value() const noexcept { return ptr != nullptr; }

      T& operator*() noexcept { return *ptr; }
      const T& operator*() const noexcept { return *ptr; }
      T* operator->() noexcept { return ptr.get(); }
      const T* operator->() const noexcept { return ptr.get(); }

      T& value() { return *ptr; }
      const T& value() const { return *ptr; }

      void reset() noexcept { ptr.reset(); }

      template <class... Args>
      T& emplace(Args&&... args)
      {
         ptr = std::make_unique<T>(std::forward<Args>(args)...);
         return *ptr;
      }

      T value_or(T default_val) const { return ptr ? *ptr : std::move(default_val); }
   };

   // Nullable string view: 16 bytes instead of optional<string_view>'s 24 bytes.
   // Null when data == nullptr. Satisfies nullable_t so Glaze skips it with skip_null_members.
   struct sv_opt final
   {
      const char* data{};
      size_t size{};

      constexpr sv_opt() noexcept = default;
      constexpr sv_opt(const char* str) noexcept : data(str), size(str ? std::char_traits<char>::length(str) : 0) {}
      constexpr sv_opt(std::string_view sv) noexcept : data(sv.data()), size(sv.size()) {}
      constexpr sv_opt(const char* str, size_t len) noexcept : data(str), size(len) {}
      constexpr sv_opt& operator=(std::string_view sv) noexcept
      {
         data = sv.data();
         size = sv.size();
         return *this;
      }

      explicit constexpr operator bool() const noexcept { return data != nullptr; }
      constexpr bool has_value() const noexcept { return data != nullptr; }
      constexpr operator std::string_view() const noexcept { return {data, size}; }
      constexpr std::string_view operator*() const noexcept { return {data, size}; }
      constexpr std::string_view value() const noexcept { return {data, size}; }

      constexpr void reset() noexcept
      {
         data = {};
         size = {};
      }
   };

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
      bool reflection_helper{}; // needed to support automatic reflection

      using schema_number = boxed<std::variant<int64_t, uint64_t, double>>;
      using schema_any = std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string_view>;

      // meta data keywords, ref: https://www.learnjsonschema.com/2020-12/meta-data/
      sv_opt title{};
      sv_opt description{};
      boxed<schema_any> defaultValue{};
      std::optional<bool> deprecated{};
      boxed<std::vector<std::string_view>> examples{};
      std::optional<bool> readOnly{};
      std::optional<bool> writeOnly{};
      // validation keywords, ref: https://www.learnjsonschema.com/2020-12/validation/
      boxed<schema_any> constant{};
      // string only keywords
      boxed<uint64_t> minLength{};
      boxed<uint64_t> maxLength{};
      sv_opt pattern{};
      // https://www.learnjsonschema.com/2020-12/format-annotation/format/
      std::optional<detail::defined_formats> format{};
      // number only keywords
      schema_number minimum{};
      schema_number maximum{};
      schema_number exclusiveMinimum{};
      schema_number exclusiveMaximum{};
      schema_number multipleOf{};
      // object only keywords
      boxed<uint64_t> minProperties{};
      boxed<uint64_t> maxProperties{};
      // std::optional<std::map<std::string_view, std::vector<std::string_view>>> dependent_required{};
      // array only keywords
      boxed<uint64_t> minItems{};
      boxed<uint64_t> maxItems{};
      boxed<uint64_t> minContains{};
      boxed<uint64_t> maxContains{};
      std::optional<bool> uniqueItems{};
      boxed<std::vector<std::string_view>> enumeration{}; // enum

      // out of json schema specification
      boxed<detail::ExtUnits> ExtUnits{};
      std::optional<bool>
         ExtAdvanced{}; // flag to indicate that the parameter is advanced and can be hidden in default views

      // structural keywords (used internally by schema generation, not typically set by users)
      boxed<std::variant<std::string_view, std::vector<std::string_view>>> type{};
      sv_opt ref{};
      boxed<std::map<std::string_view, schema, std::less<>>> properties{};
      boxed<std::vector<schema>> prefixItems{};
      boxed<std::variant<bool, std::shared_ptr<schema>>> items{};
      boxed<std::variant<bool, std::shared_ptr<schema>>> additionalProperties{};
      boxed<std::map<std::string_view, schema, std::less<>>> defs{};
      boxed<std::vector<schema>> oneOf{};
      boxed<std::vector<std::string_view>> required{};

      static constexpr auto schema_attributes{true}; // allowance flag to indicate metadata within glz::object(...)
   };

   namespace detail
   {

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
struct glz::meta<glz::schema>
{
   static constexpr std::string_view name = "glz::schema";
   using T = glz::schema;
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
   [[maybe_unused]] static constexpr glz::tuple value{&T::type, //
                                                      &T::ref, //
                                                      &T::properties, //
                                                      &T::prefixItems, //
                                                      &T::items, //
                                                      &T::additionalProperties, //
                                                      &T::defs, //
                                                      &T::oneOf, //
                                                      glz::unquoted<&T::examples>, //
                                                      &T::required, //
                                                      &T::title, //
                                                      &T::description, //
                                                      &T::defaultValue, //
                                                      &T::deprecated, //
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

namespace glz
{
   // Note: has_custom_meta_v and detail::custom_read_input_type are defined in common.hpp

   namespace detail
   {
      // Merge annotation/validation metadata from src into dst (non-null src fields overwrite dst).
      // Does NOT merge structural keywords (type, ref, properties, items, etc.).
      // Used for applying user json_schema metadata and for preserving sibling keywords during $ref inlining.
      inline void merge_schema_attrs(schema& dst, schema& src)
      {
         auto merge = [](auto& d, auto& s) {
            if (s) d = std::move(s);
         };
         merge(dst.title, src.title);
         merge(dst.description, src.description);
         merge(dst.defaultValue, src.defaultValue);
         merge(dst.deprecated, src.deprecated);
         merge(dst.readOnly, src.readOnly);
         merge(dst.writeOnly, src.writeOnly);
         merge(dst.constant, src.constant);
         merge(dst.minLength, src.minLength);
         merge(dst.maxLength, src.maxLength);
         merge(dst.pattern, src.pattern);
         merge(dst.format, src.format);
         merge(dst.minimum, src.minimum);
         merge(dst.maximum, src.maximum);
         merge(dst.exclusiveMinimum, src.exclusiveMinimum);
         merge(dst.exclusiveMaximum, src.exclusiveMaximum);
         merge(dst.multipleOf, src.multipleOf);
         merge(dst.minProperties, src.minProperties);
         merge(dst.maxProperties, src.maxProperties);
         merge(dst.minItems, src.minItems);
         merge(dst.maxItems, src.maxItems);
         merge(dst.minContains, src.minContains);
         merge(dst.maxContains, src.maxContains);
         merge(dst.uniqueItems, src.uniqueItems);
         merge(dst.enumeration, src.enumeration);
         merge(dst.ExtUnits, src.ExtUnits);
         merge(dst.ExtAdvanced, src.ExtAdvanced);
      }

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
                        auto user_metadata = get<schema_index>(to_tie(schema_v));
                        merge_schema_attrs(s, user_metadata);
                     }
                  }
               }
            }
            else if constexpr (glaze_const_value_t<T>) { // &T::constexpr_member
               using constexpr_val_t = std::remove_cvref_t<member_t<T, meta_wrapper_t<T>>>;
               static constexpr auto val_v{*glz::meta_wrapper_v<T>};
               if constexpr (glz::glaze_enum_t<constexpr_val_t>) {
                  s.constant = glz::enum_name_v<val_v>;
               }
               else {
                  // General case, needs to be convertible to schema_any
                  s.constant = val_v;
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
               s.minimum = static_cast<std::int64_t>(std::numeric_limits<V>::lowest());
               s.maximum = static_cast<std::uint64_t>((std::numeric_limits<V>::max)());
            }
            else {
               s.type = sv{"number"};
               s.minimum = std::numeric_limits<V>::lowest();
               s.maximum = (std::numeric_limits<V>::max)();
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
            s.constant = std::monostate{};
         }
      };

      template <glaze_enum_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto&)
         {
            s.type = sv{"string"};

            static constexpr auto N = reflect<T>::size;
            s.oneOf = std::vector<schema>(N);

            // Apply json_schema attributes (e.g. description) to each enum value
            if constexpr (json_schema_t<T>) {
               static constexpr auto schema_size = reflect<json_schema_type<T>>::size;
               if constexpr (schema_size > 0) {
                  for_each<N>([&]<auto I>() {
                     auto& enumeration = (*s.oneOf)[I];
                     static constexpr sv enum_key = reflect<T>::keys[I];
                     constexpr auto schema_index = [] {
                        const auto& schema_keys = reflect<json_schema_type<T>>::keys;
                        for (size_t i = 0; i < schema_size; ++i) {
                           if (schema_keys[i] == enum_key) {
                              return i;
                           }
                        }
                        return schema_size;
                     }();
                     if constexpr (schema_index < schema_size) {
                        static const auto schema_v = json_schema_type<T>{};
                        auto user_metadata = get<schema_index>(to_tie(schema_v));
                        merge_schema_attrs(enumeration, user_metadata);
                     }
                  });
               }
            }

            for_each<N>([&]<auto I>() {
               auto& enumeration = (*s.oneOf)[I];
               // Do not override if already set
               if (!enumeration.constant.has_value()) {
                  enumeration.constant = reflect<T>::keys[I];
               }
               if (!enumeration.title.has_value()) {
                  enumeration.title = reflect<T>::keys[I];
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

      // Build an inline schema for a primitive type (for items/additionalProperties)
      template <class V>
      std::shared_ptr<schema> make_primitive_schema()
      {
         auto s = std::make_shared<schema>();
         if constexpr (std::same_as<V, bool>) {
            s->type = sv{"boolean"};
         }
         else if constexpr (str_t<V> || char_t<V>) {
            s->type = sv{"string"};
         }
         else if constexpr (std::integral<V>) {
            s->type = sv{"integer"};
            s->minimum = static_cast<std::int64_t>(std::numeric_limits<V>::lowest());
            s->maximum = static_cast<std::uint64_t>((std::numeric_limits<V>::max)());
         }
         else if constexpr (std::floating_point<V>) {
            s->type = sv{"number"};
            s->minimum = std::numeric_limits<V>::lowest();
            s->maximum = (std::numeric_limits<V>::max)();
         }
         return s;
      }

      // Build a $ref schema (for items/additionalProperties)
      inline std::shared_ptr<schema> make_ref_schema(std::string_view ref)
      {
         auto s = std::make_shared<schema>();
         s->ref = ref;
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
                  s.additionalProperties = make_ref_schema(join_v<chars<"#/$defs/">, name_v<ValueType>>);
               }
            }
            else {
               s.type = sv{"array"};
               if constexpr (has_fixed_size_container<std::decay_t<T>>) {
                  s.maxItems = get_size<std::decay_t<T>>();
               }
               if constexpr (schema_primitive<V>) {
                  s.items = make_primitive_schema<V>();
               }
               else {
                  auto& def = defs[name_v<V>];
                  if (!def.type) {
                     to_json_schema<V>::template op<Opts>(def, defs);
                  }
                  s.items = make_ref_schema(join_v<chars<"#/$defs/">, name_v<V>>);
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
               s.additionalProperties = make_ref_schema(join_v<chars<"#/$defs/">, name_v<V>>);
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
            s.oneOf = std::vector<schema>(N);

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
                  schema_val.ref = join_v<chars<"#/$defs/">, name_v<V>>;
               }
               else {
                  to_json_schema<V>::template op<Opts>(schema_val, defs);
               }

               if (not schema_val.title) {
                  schema_val.title = ids[I];
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
            s.prefixItems = std::vector<schema>(N);
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
            s.maxItems = N;
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

               schema prop{};
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
                     auto user_metadata = get<schema_index>(to_tie(schema_v));
                     merge_schema_attrs(prop, user_metadata);
                  }
               }

               // Determine if this type can be inlined (bool, string, or nullable versions)
               using inner_val_t = unwrap_nullable_t<val_t>;
               constexpr bool can_inline = std::same_as<inner_val_t, bool> || str_t<inner_val_t> || char_t<inner_val_t>;

               if constexpr (can_inline) {
                  if (!prop.ref) {
                     // Inline the type directly instead of using $defs/$ref
                     if constexpr (std::same_as<inner_val_t, bool>) {
                        if constexpr (nullable_t<val_t>) {
                           prop.type = std::vector<sv>{sv{"boolean"}, sv{"null"}};
                        }
                        else {
                           prop.type = sv{"boolean"};
                        }
                     }
                     else {
                        if constexpr (nullable_t<val_t>) {
                           prop.type = std::vector<sv>{sv{"string"}, sv{"null"}};
                        }
                        else {
                           prop.type = sv{"string"};
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
                  if (!prop.ref) {
                     validate_ref<name_v<val_t>>();
                     prop.ref = join_v<chars<"#/$defs/">, name_v<val_t>>;
                  }

                  auto& def = defs[name_v<val_t>];
                  if (!def.type) {
                     to_json_schema<val_t>::template op<Opts>(def, defs);
                  }
               }

               (*s.properties)[key] = std::move(prop);
            });
            if (!req.empty()) {
               s.required = std::move(req);
            }
            s.additionalProperties = false;
         }
      };

      // Count $ref occurrences in a schema tree.
      // Note: map keys are string_views pointing to ref values, which are compile-time static storage
      // (from join_v<...>). If runtime-constructed ref strings are ever introduced, these keys could
      // dangle after try_inline_ref moves the source schema.
      inline void count_schema_refs(const schema& s, std::map<std::string_view, size_t>& counts)
      {
         if (s.ref) {
            ++counts[*s.ref];
         }
         if (s.properties) {
            for (const auto& [_, prop] : *s.properties) {
               count_schema_refs(prop, counts);
            }
         }
         if (s.items) {
            if (auto* ptr = std::get_if<std::shared_ptr<schema>>(&*s.items)) {
               if (*ptr) count_schema_refs(**ptr, counts);
            }
         }
         if (s.additionalProperties) {
            if (auto* ptr = std::get_if<std::shared_ptr<schema>>(&*s.additionalProperties)) {
               if (*ptr) count_schema_refs(**ptr, counts);
            }
         }
         if (s.oneOf) {
            for (const auto& entry : *s.oneOf) {
               count_schema_refs(entry, counts);
            }
         }
         if (s.prefixItems) {
            for (const auto& entry : *s.prefixItems) {
               count_schema_refs(entry, counts);
            }
         }
         if (s.defs) {
            for (const auto& [_, def] : *s.defs) {
               count_schema_refs(def, counts);
            }
         }
      }

      // Try to inline a single-use $ref node by moving the definition from defs.
      // Returns true if the node was inlined.
      inline bool try_inline_ref(schema& node, std::map<std::string_view, schema, std::less<>>& defs,
                                 const std::map<std::string_view, size_t>& counts)
      {
         static constexpr std::string_view prefix = "#/$defs/";
         if (!node.ref) return false;
         auto ref = *node.ref;
         if (!ref.starts_with(prefix)) return false;
         auto it = counts.find(ref);
         if (it == counts.end() || it->second != 1) return false;
         auto def_name = ref.substr(prefix.size());
         auto def_it = defs.find(def_name);
         if (def_it == defs.end()) return false;

         // Save sibling metadata before overwriting
         schema saved_metadata = std::move(node);
         saved_metadata.ref.reset();

         // Move the definition into this node (safe: single-use means exactly one consumer)
         node = std::move(def_it->second);

         // Merge saved sibling metadata into the inlined definition
         merge_schema_attrs(node, saved_metadata);
         return true;
      }

      // Inline single-use $ref entries by moving the definition from $defs into the reference site.
      inline void inline_single_use_refs(schema& s, std::map<std::string_view, schema, std::less<>>& defs,
                                         const std::map<std::string_view, size_t>& counts)
      {
         if (s.properties) {
            for (auto& [_, prop] : *s.properties) {
               inline_single_use_refs(prop, defs, counts);
               if (try_inline_ref(prop, defs, counts)) {
                  // Re-process the newly inlined content for chained single-use refs
                  inline_single_use_refs(prop, defs, counts);
               }
            }
         }
         if (s.items) {
            if (auto* ptr = std::get_if<std::shared_ptr<schema>>(&*s.items)) {
               if (*ptr) {
                  inline_single_use_refs(**ptr, defs, counts);
                  if (try_inline_ref(**ptr, defs, counts)) {
                     inline_single_use_refs(**ptr, defs, counts);
                  }
               }
            }
         }
         if (s.additionalProperties) {
            if (auto* ptr = std::get_if<std::shared_ptr<schema>>(&*s.additionalProperties)) {
               if (*ptr) {
                  inline_single_use_refs(**ptr, defs, counts);
                  if (try_inline_ref(**ptr, defs, counts)) {
                     inline_single_use_refs(**ptr, defs, counts);
                  }
               }
            }
         }
         if (s.oneOf) {
            for (auto& entry : *s.oneOf) {
               inline_single_use_refs(entry, defs, counts);
               if (try_inline_ref(entry, defs, counts)) {
                  inline_single_use_refs(entry, defs, counts);
               }
            }
         }
         if (s.prefixItems) {
            for (auto& entry : *s.prefixItems) {
               inline_single_use_refs(entry, defs, counts);
               if (try_inline_ref(entry, defs, counts)) {
                  inline_single_use_refs(entry, defs, counts);
               }
            }
         }
      }

      // Remove inlined (single-use) entries from $defs
      inline void prune_inlined_defs(schema& s, const std::map<std::string_view, size_t>& counts)
      {
         if (!s.defs) return;
         static constexpr std::string_view prefix = "#/$defs/";
         for (auto it = s.defs->begin(); it != s.defs->end();) {
            // Build the full $ref path for this def entry
            // Use a local string since string_view concatenation isn't possible
            std::string full_ref{prefix};
            full_ref += it->first;
            auto count_it = counts.find(std::string_view{full_ref});
            if (count_it != counts.end() && count_it->second == 1) {
               it = s.defs->erase(it);
            }
            else {
               ++it;
            }
         }
         if (s.defs->empty()) {
            s.defs.reset();
         }
      }

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
      schema s{};
      s.defs.emplace();
      detail::to_json_schema<std::decay_t<T>>::template op<Opts>(s, *s.defs);
      s.title = name_v<T>;

      // Inline single-use $defs entries at their reference sites
      std::map<std::string_view, size_t> ref_counts;
      detail::count_schema_refs(s, ref_counts);
      // First inline refs within defs entries (for chained single-use types)
      for (auto& [_, def] : *s.defs) {
         detail::inline_single_use_refs(def, *s.defs, ref_counts);
      }
      // Then inline refs in the main schema tree
      detail::inline_single_use_refs(s, *s.defs, ref_counts);
      detail::prune_inlined_defs(s, ref_counts);

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

   // Custom reader for sv_opt: delegates to string_view reader, then assigns to sv_opt.
   template <>
   struct from<JSON, sv_opt>
   {
      template <auto Opts>
      static void op(sv_opt& value, is_context auto&& ctx, auto&& it, auto end)
      {
         std::string_view sv;
         from<JSON, std::string_view>::template op<Opts>(sv, ctx, std::forward<decltype(it)>(it), end);
         if (!bool(ctx.error)) {
            value = sv;
         }
      }
   };
}
