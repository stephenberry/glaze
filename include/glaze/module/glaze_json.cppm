// Glaze Library
// For the license information refer to glaze.hpp

module;

#include <glaze/json.hpp>

export module glaze.json;

export namespace glz {

   // Core Types
   using glz::context;
   using glz::error_code;
   using glz::error_ctx;
   using glz::expected;
   using glz::JSON;
   using glz::opts;
   using glz::raw_json;

   // Reflection and Metadata
   using glz::enumerate;
   using glz::flags;
   using glz::for_each_field;
   using glz::meta;
   using glz::object;
   using glz::reflect;

   // JSON Reading
   using glz::read_file_json;
   using glz::read_json;

   // JSON Writing
   using glz::write_file_json;
   using glz::write_json;

   // Generic Read/Write
   using glz::read;
   using glz::write;

   // Error Handling
   using glz::format_error;

   // JSON Utilities
   using glz::minify_json;
   using glz::prettify_json;
   using glz::validate_json;

   // JSON Pointer
   using glz::get;
   using glz::set;

   // Wrappers
   using glz::custom;
   using glz::hide;
   using glz::quoted_num;
   using glz::read_constraint;
   using glz::skip;

   // Container Helpers
   using glz::arr;
   using glz::merge;
   using glz::obj;

   // NDJSON
   using glz::read_ndjson;
   using glz::write_ndjson;

   // JSONC
   using glz::read_jsonc;

   // JSON Schema
   using glz::json_schema;
   using glz::write_json_schema;

   // Generic JSON
   using glz::generic;

}
