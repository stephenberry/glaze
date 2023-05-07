// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>

namespace glz
{
   // format
   constexpr uint32_t binary = 0;
   constexpr uint32_t json = 10;
   constexpr uint32_t ndjson = 100; // new line delimited JSON
   constexpr uint32_t json_schema = 1000;
   constexpr uint32_t csv = 10000;

   // layout
   constexpr uint32_t rowwise = 0;
   constexpr uint32_t colwise = 1;
}
