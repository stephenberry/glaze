#pragma once

#include "glaze/glaze.hpp"

namespace example
{
   struct person
   {
      std::string first_name{};
      std::string last_name{};
      uint32_t age{};
   };
}

template <>
struct glz::meta<example::person>
{
   using T = example::person;
   static constexpr auto value = object("first_name", &T::first_name, "last_name", &T::last_name, "age", &T::age);
};
