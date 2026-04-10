// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.format.format_to;

import glaze.core.write;
import glaze.util.dtoa;
import glaze.util.itoa;
import glaze.concepts.container_concepts;

import std;

using std::size_t;

namespace glz
{
   export template <num_t T>
   void format_to(std::string& buffer, T&& value)
   {
      auto ix = buffer.size();
      buffer.resize((std::max)(buffer.size() * 2, ix + 64));

      const auto start = buffer.data() + ix;
      const auto end = glz::to_chars(start, std::forward<T>(value));
      ix += size_t(end - start);
      buffer.resize(ix);
   }
}
