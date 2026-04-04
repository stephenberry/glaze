#pragma once

#include <glaze/glaze.hpp>

using std::uint32_t;

namespace example
{
   struct person
   {
      std::string first_name{};
      std::string last_name{};
      uint32_t age{};
   };
}
