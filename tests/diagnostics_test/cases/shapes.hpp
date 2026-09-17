#pragma once

#include <string>
#include <vector>

#include <glaze/core/reflect.hpp>

// Private data, no glz::meta: neither an aggregate nor reflectable, so no format has a writer or a
// reader for it.
class Opaque
{
   int value{};

public:
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
