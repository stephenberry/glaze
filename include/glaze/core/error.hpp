// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/context.hpp"
#include "glaze/core/common.hpp"

#include <system_error>

namespace glz
{
   struct error_category : public std::error_category
   {
      static const error_category& instance() {
         static error_category instance{};
         return instance;
      }

      const char* name() const noexcept override { return "glz::error_category"; }

      std::string message(int ec) const override
      {
         static constexpr auto arr = detail::make_enum_to_string_array<error_code>();
         return std::string{arr[ec]};
      }
   };
}
