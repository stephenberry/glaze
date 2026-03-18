
export module glaze.tests.json.json_test_shared_types;

import std;

import glaze.core.common;
import glaze.core.meta;
import glaze.core.reflect;

export enum class Color { Red, Green, Blue };

export template <>
struct glz::meta<Color>
{
   static constexpr std::string_view name = "Color";
   static constexpr auto value = glz::enumerate("Red", Color::Red, //
                                                "Green", Color::Green, //
                                                "Blue", Color::Blue //
   );
};

static_assert(glz::enum_name_v<Color::Red> == "Red");

static_assert(glz::get_enum_name(Color::Green) == "Green");

export struct xy_t
{
   int x{};
   int y{};
};

export template <>
struct glz::meta<xy_t>
{
   using T = xy_t;
   static constexpr auto value = glz::object("x", &T::x, "y", &T::y);
};
