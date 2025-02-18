// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../Export.hpp"
#include <tuple>
#ifdef CPP_MODULES
export module glaze.util.tuple;
import glaze.reflection.get_name;
import glaze.tuplet.tuple;
import glaze.util.for_each;
import glaze.util.string_literal;
#else
#include "glaze/reflection/get_name.cppm"
#include "glaze/tuplet/tuple.cppm"
#include "glaze/util/for_each.cppm"
#include "glaze/util/string_literal.cppm"
#endif





namespace glz
{
   template <class T>
   concept is_std_tuple = is_specialization_v<T, std::tuple>;

   // TODO: This doesn't appear to be used. Should it be removed?
   template <class Type>
   concept is_schema_class = requires {
      requires std::is_class_v<Type>;
      requires Type::schema_attributes;
   };
}
