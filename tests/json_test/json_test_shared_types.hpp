
#pragma once

#include <string_view>

#include "glaze/json.hpp"

enum class Color { Red, Green, Blue };

template <>
struct glz::meta<Color>
{
   static constexpr std::string_view name = "Color";
   static constexpr auto value = enumerate("Red", Color::Red, //
                                           "Green", Color::Green, //
                                           "Blue", Color::Blue //
   );
};

static_assert(glz::enum_name_v<Color::Red> == "Red");

static_assert(glz::get_enum_name(Color::Green) == "Green");

struct xy_t
{
   int x{};
   int y{};
};

template <>
struct glz::meta<xy_t>
{
   using T = xy_t;
   static constexpr auto value = object("x", &T::x, "y", &T::y);
};
