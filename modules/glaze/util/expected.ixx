// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.util.expected;

import std;

namespace glz
{
   export template <class Expected, class Unexpected>
   using expected = std::expected<Expected, Unexpected>;

   export template <class T>
   concept is_expected =
      std::same_as<std::remove_cvref_t<T>, expected<typename T::value_type, typename T::error_type> >;

#ifdef __clang__
   export template <class Unexpected>
   struct unexpected : public std::unexpected<Unexpected>
   {
      using std::unexpected<Unexpected>::unexpected;
   };
   export template <class Unexpected>
   unexpected(Unexpected) -> unexpected<Unexpected>;
#else
   export template <class Unexpected>
   using unexpected = std::unexpected<Unexpected>;
#endif
}
