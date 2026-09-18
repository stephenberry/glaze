#pragma once

#include <string>
#include <vector>

#include <glaze/core/reflect.hpp>

// Private data, no glz::meta: no format has a writer or a reader for it, in either reflection mode.
// Traditional reflection already excludes it (a class with private data is not an aggregate), but
// C++26 reflection can reflect any class, so the opt-out has to be explicit for the case to mean the
// same thing there.
class Opaque
{
   int value{};

public:
   static constexpr bool glaze_reflect = false;
   Opaque() = default;
};

// The member at index 1 is the one no format can handle.
struct Outer
{
   int a{};
   Opaque o{};
   int b{};
};

// The same, one level down: every member of Nested itself has a writer, so the report has to come
// from Inner rather than from the object that contains it.
struct Inner
{
   int a{};
   Opaque o{};
};

struct Nested
{
   int head{};
   Inner inner{};
};

// Two members no format can handle, with a good one between them. Which of the two the report names
// is only visible in the instantiation trace, and a type with a single unsupported member cannot
// tell "first" from "last" -- without this shape a fold that keeps the last index instead of the
// first passes the whole suite.
struct TwoBad
{
   Opaque first_bad{};
   int ok{};
   Opaque second_bad{};
};

// Renamed keys: what the report names has to be the key the output uses, so that the member can be
// found in the document.
struct Renamed
{
   int a{};
   Opaque o{};
};

template <>
struct glz::meta<Renamed>
{
   using T = Renamed;
   static constexpr auto value = glz::object("alpha", &T::a, "omicron", &T::o);
};
