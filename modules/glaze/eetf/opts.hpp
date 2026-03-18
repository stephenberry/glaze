#pragma once

#include <glaze/core/opts.hpp>

namespace glz::eetf
{

   // layout erlang term
   inline constexpr std::uint8_t map_layout = 0;
   inline constexpr std::uint8_t proplist_layout = 1;

   struct eetf_opts
   {
      std::uint32_t format;
      std::uint8_t layout{map_layout};
      bool error_on_unknown_keys{true};
      bool shrink_to_fit{false};
      std::uint32_t internal{};
   };

} // namespace glz::eetf

