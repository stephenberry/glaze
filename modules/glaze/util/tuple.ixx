// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.util.tuple;

import std;

import glaze.util.for_each;
import glaze.util.string_literal;
import glaze.util.type_traits;

import glaze.tuplet;

export namespace glz
{
   template <class T>
   concept is_std_tuple = is_specialization_v<T, std::tuple>;

   // TODO: This doesn't appear to be used. Should it be removed?
   // todo: Let know in the pr release notes that this wasn't exported
   template <class Type>
   concept is_schema_class = requires {
      requires std::is_class_v<Type>;
      requires Type::schema_attributes;
   };
}
