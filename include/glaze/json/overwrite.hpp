// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json/json_ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

#include <string>

namespace glaze
{
  template <class T, class B>
  bool overwrite(T&& root_value, const std::string_view json_ptr, B&& buffer) {
    return detail::seek_impl(
        [&](auto&& val) {
          read_json(val, buffer);
        },
        std::forward<T>(root_value), json_ptr
    );
  }

  template <class T, class B>
  bool read_out(T&& root_value, const std::string_view json_ptr, B& buffer)
  {
     return detail::seek_impl([&](auto&& val) { write_json(val, buffer); },
                       std::forward<T>(root_value), json_ptr);
  }
}
