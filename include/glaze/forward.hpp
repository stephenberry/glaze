// Glaze Library
// For the license information refer to glaze.hpp

// Lightweight forward declarations of Glaze's customization points.
//
// Include this header alone when you need to specialize glz::meta,
// glz::to<Format, T>, glz::from<Format, T>, or construct glz::custom_t in
// widely-included user headers. It pulls in no Glaze internals and only
// <cstdint> from the standard library, so it's cheap to include broadly and
// lets user code defer inclusion of the full Glaze headers (glaze/glaze.hpp,
// glaze/json.hpp, glaze/core/common.hpp) to the translation units that
// actually call read/write.
//
// Full definitions of these types — and builders like glz::object(...),
// glz::array(...), glz::enumerate(...) — still live in glaze/core/meta.hpp
// and glaze/core/common.hpp.

#pragma once

#include <cstdint>

namespace glz
{
   using uint32_t = std::uint32_t;

   // Format identifiers.
   // Built-in formats are < 65536; user-defined formats may use 65536..2^32-1.
   inline constexpr uint32_t INVALID = 0;
   inline constexpr uint32_t BEVE = 1;
   inline constexpr uint32_t CBOR = 2; // RFC 8949 - Concise Binary Object Representation
   inline constexpr uint32_t JSONB = 3; // SQLite JSONB binary JSON format - https://sqlite.org/jsonb.html
   inline constexpr uint32_t BSON = 4; // MongoDB BSON 1.1 - https://bsonspec.org/spec.html
   inline constexpr uint32_t JSON = 10;
   inline constexpr uint32_t JSON_PTR = 20;
   inline constexpr uint32_t MSGPACK = 30;
   inline constexpr uint32_t NDJSON = 100; // new line delimited JSON
   inline constexpr uint32_t TOML = 400;
   inline constexpr uint32_t YAML = 450;
   inline constexpr uint32_t STENCIL = 500;
   inline constexpr uint32_t MUSTACHE = 501;
   inline constexpr uint32_t CSV = 10000;
   inline constexpr uint32_t EETF = 20000;

   // Protocol formats
   inline constexpr uint32_t REPE = 30000;
   inline constexpr uint32_t REST = 30100;
   inline constexpr uint32_t JSONRPC = 30200;

   // Reflection metadata customization point.
   template <class T>
   struct meta;

   // Per-(format, type) serializer/deserializer specializations.
   // Primary templates are left incomplete so that the write_supported /
   // read_supported concepts can detect whether a specialization exists.
   template <uint32_t Format = INVALID, class T = void>
   struct to;

   template <uint32_t Format = INVALID, class T = void>
   struct from;

   template <uint32_t Format = INVALID, class T = void>
   struct to_partial;

   template <uint32_t Format = INVALID>
   struct skip_value;

   // Per-format dispatch layer. Specialized in json/write.hpp, cbor/read.hpp, etc.
   template <uint32_t Format>
   struct parse
   {};

   template <uint32_t Format>
   struct serialize
   {};

   template <uint32_t Format>
   struct serialize_partial
   {};

   // Wrapper that registers custom read/write handlers against an existing type.
   // Fully defined here because it's a trivial aggregate with no Glaze dependencies,
   // and user headers constructing it via glz::custom_t{v, From, To} benefit from
   // not pulling in glaze/core/wrappers.hpp.
   template <class T, class From, class To>
   struct custom_t final
   {
      static constexpr auto glaze_reflect = false;
      using from_t = From;
      using to_t = To;
      T& val;
      From from;
      To to;
   };

   template <class T, class From, class To>
   custom_t(T&, From, To) -> custom_t<T, From, To>;
}
