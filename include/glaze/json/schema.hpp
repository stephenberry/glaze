// Glaze Library
// For the license information refer to glaze.hpp
#pragma once

#include "glaze/json/write.hpp"
#include "glaze/api/impl.hpp"

namespace glz
{
   namespace detail
   {
      // TODO switch to using variants when we have write support to get rid of nulls
      // TODO We should be able to generate the json schema compiletime
      struct schema_ref
      {
         std::string_view ref{};
         std::optional<std::string_view> description{};

         struct glaze
         {
            using T = schema_ref;
            static constexpr auto value = glz::object("$ref", &T::ref,                //
                                                      "description", &T::description  //
            );
         };
      };

      struct schema
      {
         std::optional<std::vector<std::string_view>> type{};
         std::optional<std::string_view> _const{};
         std::optional<std::string_view> description{};
         std::optional<std::map<std::string_view, schema_ref, std::less<>>> properties{};  // glaze_object
         std::optional<schema_ref> items{};                                        // array
         std::optional<std::variant<bool, schema_ref>> additionalProperties{};               // map
         std::optional<std::map<std::string_view, schema, std::less<>>> defs{};
         std::optional<std::vector<std::string_view>> _enum{};  // enum
         std::optional<std::vector<schema>> oneOf{};
      };

   }
}

template <>
struct glz::meta<glz::detail::schema>
{
   static constexpr std::string_view name = "glz::detail::schema";
   using T = glz::detail::schema;
   static constexpr auto value = glz::object("type", &T::type,                                  //
                                             "description", &T::description,                    //
                                             "properties", &T::properties,                      //
                                             "items", &T::items,                                //
                                             "additionalProperties", &T::additionalProperties,  //
                                             "$defs", &T::defs,                                 //
                                             "enum", &T::_enum,                                 //
                                             "oneOf", &T::oneOf,                                //
                                             "const", &T::_const);
};

namespace glz
{
   namespace detail
   {
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

      template <glaze_enum_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto&) noexcept
         {
            s.type = {"string"};

            // TODO use oneOf instead of enum to handle doc comments
            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;
            //s._enum = std::vector<std::string_view>(N);
            //for_each<N>([&](auto I) {
            //   static constexpr auto item = std::get<I>(meta_v<V>);
            //   (*s._enum)[I.value] = std::get<0>(item);
            //});
            s.oneOf = std::vector<schema>(N);
            for_each<N>([&](auto I) {
               static constexpr auto item = glz::tuplet::get<I>(meta_v<V>);
               auto& _enum = (*s.oneOf)[I.value];
               _enum._const = glz::tuplet::get<0>(item);
               if constexpr (std::tuple_size_v < decltype(item) >> 2) {
                  _enum.description = std::get<2>(item);
               }
            });
         }
      };

      template <class T>
      requires std::same_as<std::decay_t<T>, raw_json>
      struct to_json_schema<T>
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
            s.items = schema_ref{join_v<chars<"#/$defs/">, name_v<V>>};
         }
      };

      template <map_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto& defs) noexcept
         {
            using V = std::decay_t<std::tuple_element_t<1,range_value_t<std::decay_t<T>>>>;
            s.type = {"object"};
            auto& def = defs[name_v<V>];
            if (!def.type) {
               to_json_schema<V>::template op<Opts>(def, defs);
            }
            s.additionalProperties = schema_ref{join_v<chars<"#/$defs/">, name_v<V>>};
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
            (*s.type).emplace_back("null");
         }
      };

      template <class... Ts>
      struct to_json_schema<std::variant<Ts...>>
      {
         template <auto Opts>
         static void op(auto& s, auto& defs) noexcept
         {
            using tuple_t = glz::tuplet::tuple<Ts...>;
            static constexpr auto N = std::tuple_size_v<tuple_t>;
            s.type = {"number", "string", "boolean", "object", "array", "null"};
            s.oneOf = std::vector<schema>(N);
            for_each<N>([&](auto I) {
               using V = std::decay_t<std::tuple_element_t<I, tuple_t>>;
               auto& schema_val = (*s.oneOf)[I.value];
               //TODO use ref to avoid duplication in schema
               to_json_schema<V>::template op<Opts>(schema_val, defs);
               if constexpr (glaze_object_t<V>) {
                  //TODO validate type name
                  auto& def = defs[name_v<std::string>];
                  if (!def.type) {
                     to_json_schema<std::string>::template op<Opts>(def, defs);
                  }
                  (*schema_val.properties)["type"] = schema_ref{join_v<chars<"#/$defs/">, name_v<std::string>>};
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

      template <glaze_object_t T>
      struct to_json_schema<T>
      {
         template <auto Opts>
         static void op(auto& s, auto& defs) noexcept
         {
            s.type = {"object"};

            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;
            s.properties = std::map<std::string_view, schema_ref, std::less<>>();
            for_each<N>([&](auto I) {
               static constexpr auto item = glz::tuplet::get<I>(meta_v<V>);
               using mptr_t = decltype(glz::tuplet::get<1>(item));
               using val_t = std::decay_t<member_t<V, mptr_t>>;
               auto& def = defs[name_v<val_t>];
               if (!def.type) {
                  to_json_schema<val_t>::template op<Opts>(def, defs);
               }
               auto ref_val = schema_ref{join_v<chars<"#/$defs/">, name_v<val_t>>};
               if constexpr (std::tuple_size_v<decltype(item)> > 2) {
                  ref_val.description = glz::tuplet::get<2>(item);
               }
               (*s.properties)[glz::tuplet::get<0>(item)] = ref_val;
            });
            s.additionalProperties = false;
         }
      };

   }

   template <class T, class Buffer>
   inline void write_json_schema(Buffer&& buffer)
   {
      detail::schema s{};
      s.defs = std::map<std::string_view, detail::schema, std::less<>>{};
      detail::to_json_schema<std::decay_t<T>>::template op<opts{}>(s, *s.defs);
      write<opts{}>(std::move(s), std::forward<Buffer>(buffer));
   }

   template <class T>
   inline auto write_json_schema()
   {
      std::string buffer{};
      detail::schema s{};
      s.defs = std::map<std::string_view, detail::schema, std::less<>>{};
      detail::to_json_schema<std::decay_t<T>>::template op<opts{}>(s, *s.defs);
      write<opts{.write_type_info = false}>(std::move(s), buffer);
      return buffer;
   }
}
