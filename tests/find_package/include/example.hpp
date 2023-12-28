#pragma once

#include <glaze/glaze.hpp>

namespace example
{
   struct person
   {
      std::string first_name{};
      std::string last_name{};
      uint32_t age{};
   };
}
