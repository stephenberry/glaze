// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/rpc/registry_fwd.hpp"
// glz:header std=<cstddef>
// glz:header std=<cstdint>
// glz:header std=<functional>
// glz:header std=<string>
// glz:header std=<string_view>
export module glaze.rpc.registry_fwd;

import std;

namespace glz
{
   export template <std::uint32_t Protocol>
   struct protocol_storage
   {};

   export template <auto Opts, std::uint32_t Protocol>
   struct registry_impl;
}

namespace glz::detail
{
   export struct string_hash
   {
      using is_transparent = void;
      [[nodiscard]] std::size_t operator()(const char* value) const { return std::hash<std::string_view>{}(value); }
      [[nodiscard]] std::size_t operator()(std::string_view value) const
      {
         return std::hash<std::string_view>{}(value);
      }
      [[nodiscard]] std::size_t operator()(const std::string& value) const { return std::hash<std::string>{}(value); }
   };
}
