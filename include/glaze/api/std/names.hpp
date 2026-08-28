// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// Canonical aggregate of the naming metas for standard library types.
//
// Without a meta<T>::name, glz::name_v<T> falls back to glz::type_name<T>, which is derived from
// __PRETTY_FUNCTION__/__FUNCSIG__ and spells the same type differently on every compiler and
// standard library. The metas gathered here pin one spelling so the name is stable everywhere.
//
// Include this header rather than reaching for the individual glaze/api/std/*.hpp headers.
// glz::name_v<T> is an inline variable, so a translation unit that sees meta<T> and one that does
// not initialize the same inline variable to different values — an ODR violation no compiler
// diagnoses. The names are load-bearing, not cosmetic: they key "$defs" and "$ref" in JSON Schema
// output, and they feed glz::trait<T>::hash, which is the type check across a shared library
// boundary in glz::api. Selecting a subset per call site is how two translation units in one
// program come to disagree.
//
// The individual headers remain available for the rare translation unit that needs exactly one
// naming meta and nothing else.

#include "glaze/api/std/array.hpp"
#include "glaze/api/std/deque.hpp"
#include "glaze/api/std/functional.hpp"
#include "glaze/api/std/list.hpp"
#include "glaze/api/std/map.hpp"
#include "glaze/api/std/optional.hpp"
#include "glaze/api/std/set.hpp"
#include "glaze/api/std/shared_ptr.hpp"
#include "glaze/api/std/span.hpp"
#include "glaze/api/std/string.hpp"
#include "glaze/api/std/tuple.hpp"
#include "glaze/api/std/unique_ptr.hpp"
#include "glaze/api/std/unordered_map.hpp"
#include "glaze/api/std/unordered_set.hpp"
#include "glaze/api/std/variant.hpp"
#include "glaze/api/std/vector.hpp"
