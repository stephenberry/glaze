// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../Export.hpp"
#ifdef CPP_MODULES
export module glaze.format.format_to;
import glaze.core.write;
import glaze.util.dtoa;
import glaze.util.itoa;
#else
#include "glaze/core/write.cppm"
#include "glaze/util/dtoa.cppm"
#include "glaze/util/itoa.cppm"
#endif




namespace glz
{
   template <detail::num_t T>
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
