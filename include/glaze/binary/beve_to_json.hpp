// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/binary/header.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   template <glz::opts Opts = glz::opts{}, class Buffer>
   inline write_error beve_to_json(const std::string& beve, Buffer& out) noexcept {
      size_t ix{}; // write index
      
      auto* it = beve.data();
      auto* end = it + beve.size();
      
      context ctx{};
      
      while (it < end)
      {
         const auto tag = uint8_t(*it);
         const auto type = tag & 0b00000'111;
         switch (type)
         {
            case tag::null: {
               if (tag & tag::boolean) {
                  if (tag >> 4) {
                     detail::dump<"true">(out, ix);
                  }
                  else {
                     detail::dump<"false">(out, ix);
                  }
               }
               else {
                  detail::dump<"null">(out, ix);
               }
               ++it;
               break;
            }
            case tag::number: {
               ++it;
               const auto number_type = (tag & 0b000'11'000) >> 3;
               const uint8_t byte_count = detail::byte_count_lookup[tag >> 5];
               
               auto write_number = [&]<class T>(T&& value){
                  std::memcpy(&value, &(*it), sizeof(T));
                  detail::to_json<T>::template op<Opts>(value, ctx, out, ix);
                  std::advance(it, sizeof(T));
               };
               
               switch (number_type)
               {
                  case 0: {
                     // floating point
                     switch (byte_count) {
                        case 4: {
                           write_number(float{});
                           break;
                        }
                        case 8: {
                           write_number(double{});
                           break;
                        }
                        default: {
                           return {error_code::syntax_error};
                        }
                     }
                     break;
                  }
                  case 1: {
                     // signed integer
                     switch (byte_count) {
                        case 1: {
                           write_number(int8_t{});
                           break;
                        }
                        case 2: {
                           write_number(int16_t{});
                           break;
                        }
                        case 4: {
                           write_number(int32_t{});
                           break;
                        }
                        case 8: {
                           write_number(int64_t{});
                           break;
                        }
                        default: {
                           return {error_code::syntax_error};
                        }
                     }
                     break;
                  }
                  case 2: {
                     // unsigned integer
                     switch (byte_count) {
                        case 1: {
                           write_number(uint8_t{});
                           break;
                        }
                        case 2: {
                           write_number(uint16_t{});
                           break;
                        }
                        case 4: {
                           write_number(uint32_t{});
                           break;
                        }
                        case 8: {
                           write_number(uint64_t{});
                           break;
                        }
                        default: {
                           return {error_code::syntax_error};
                        }
                     }
                     break;
                  }
                  default: {
                     return {error_code::syntax_error};
                  }
               }
               break;
            }
            case tag::string: {
               ++it;
               const auto n = detail::int_from_compressed(it, end);
               const std::string_view value{ &(*it), n };
               detail::to_json<std::string_view>::template op<Opts>(value, ctx, out, ix);
               std::advance(it, n);
               break;
            }
            default: {
               return {error_code::syntax_error};
            }
         }
      }
      
      if constexpr (detail::resizeable<Buffer>) {
         out.resize(ix);
      }
      
      return {};
   }
}
