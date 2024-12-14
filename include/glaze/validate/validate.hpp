// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json/schema.hpp"

// The purpose of glz::validate(value) is to use the structural memory for validation
// rather than validating against an input buffer like JSON
// Benefits of this approach:
// - Supports validation of multiple formats by first parsing into the object and then validating the input.
// - Supports both read/write validation.
// - Allows this validation to be developed separately from format code, which significantly reduces the amount of code required.
// Downsides:
// - For read validation we don't error until after the entire object has been parsed, which forces a copy of data if we're trying to prevent invalid inputs. But, the benefits still often outweigh the costs.

namespace glz
{
   template <json_schema_t T>
   [[nodiscard]] std::optional<std::string> validate() noexcept
   {
      // TODO: Implement
      return {};
   }
}
