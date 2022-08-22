// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/opts.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glaze
{
   template <class T, class B>
   bool write_from(T&& root_value, const std::string_view json_ptr, B&& buffer) {
      return detail::seek_impl(
        [&](auto&& val) {
          read_json(val, buffer);
        },
        std::forward<T>(root_value), json_ptr
    );
   }

   template <opts Opts, class T, class B>
   bool read_from(T&& root_value, const std::string_view json_ptr, B& buffer)
   {
      return detail::seek_impl([&](auto&& val) { write<Opts>(val, buffer); },
                       std::forward<T>(root_value), json_ptr);
   }
}
