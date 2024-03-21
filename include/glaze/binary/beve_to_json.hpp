// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/binary/header.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   namespace detail
   {
      template <glz::opts Opts = glz::opts{}, class Buffer>
      inline void beve_to_json_value(auto&& ctx, auto&& it, auto&& end, Buffer& out, auto&& ix) noexcept {
         const auto tag = uint8_t(*it);
         const auto type = tag & 0b00000'111;
         switch (type)
         {
            case tag::null: {
               if (tag & tag::boolean) {
                  if (tag >> 4) {
                     dump<"true">(out, ix);
                  }
                  else {
                     dump<"false">(out, ix);
                  }
               }
               else {
                  dump<"null">(out, ix);
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
                  to_json<T>::template op<Opts>(value, ctx, out, ix);
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
                           ctx.error = error_code::syntax_error;
                           return;
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
                           ctx.error = error_code::syntax_error;
                           return;
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
                           ctx.error = error_code::syntax_error;
                           return;
                        }
                     }
                     break;
                  }
                  default: {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }
               break;
            }
            case tag::string: {
               ++it;
               const auto n = detail::int_from_compressed(it, end);
               const sv value{ &(*it), n };
               to_json<sv>::template op<Opts>(value, ctx, out, ix);
               std::advance(it, n);
               break;
            }
            case tag::object: {
               ++it;
               
               dump<'{'>(out, ix);
               
               const auto key_type = (tag & 0b000'11'000) >> 3;
               switch (key_type)
               {
                  case 0: {
                     // string key
                     const auto n_fields = detail::int_from_compressed(it, end);
                     for (size_t i = 0; i < n_fields; ++i) {
                        // convert the key
                        const auto n = detail::int_from_compressed(it, end);
                        const sv key{ &(*it), n };
                        to_json<sv>::template op<Opts>(key, ctx, out, ix);
                        dump<':'>(out, ix);
                        std::advance(it, n);
                        // convert the value
                        beve_to_json_value(ctx, it, end, out, ix);
                        if (i != n_fields - 1) {
                           dump<','>(out, ix);
                        }
                     }
                     break;
                  }
                  default: {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }
               
               dump<'}'>(out, ix);
               break;
            }
            case tag::typed_array: {
               ++it;
               const auto number_type = (tag & 0b000'11'000) >> 3;
               const uint8_t byte_count = detail::byte_count_lookup[tag >> 5];
               
               auto write_array = [&]<class T>(T&& value){
                  const auto n = int_from_compressed(it, end);
                  for (size_t i = 0; i < n; ++i) {
                     std::memcpy(&value, &(*it), sizeof(T));
                     to_json<T>::template op<Opts>(value, ctx, out, ix);
                     std::advance(it, sizeof(T));
                     if (i != n - 1) {
                        dump<','>(out, ix);
                     }
                  }
               };
               
               dump<'['>(out, ix);
               
               switch (number_type)
               {
                  case 0: {
                     // floating point
                     switch (byte_count) {
                        case 4: {
                           write_array(float{});
                           break;
                        }
                        case 8: {
                           write_array(double{});
                           break;
                        }
                        default: {
                           ctx.error = error_code::syntax_error;
                           return;
                        }
                     }
                     break;
                  }
                  case 1: {
                     // signed integer
                     switch (byte_count) {
                        case 1: {
                           write_array(int8_t{});
                           break;
                        }
                        case 2: {
                           write_array(int16_t{});
                           break;
                        }
                        case 4: {
                           write_array(int32_t{});
                           break;
                        }
                        case 8: {
                           write_array(int64_t{});
                           break;
                        }
                        default: {
                           ctx.error = error_code::syntax_error;
                           return;
                        }
                     }
                     break;
                  }
                  case 2: {
                     // unsigned integer
                     switch (byte_count) {
                        case 1: {
                           write_array(uint8_t{});
                           break;
                        }
                        case 2: {
                           write_array(uint16_t{});
                           break;
                        }
                        case 4: {
                           write_array(uint32_t{});
                           break;
                        }
                        case 8: {
                           write_array(uint64_t{});
                           break;
                        }
                        default: {
                           ctx.error = error_code::syntax_error;
                           return;
                        }
                     }
                     break;
                  }
                  default: {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }
               
               dump<']'>(out, ix);
               
               break;
            }
            default: {
               ctx.error = error_code::syntax_error;
               return;
            }
         }
      }
   }
   
   template <glz::opts Opts = glz::opts{}, class Buffer>
   inline write_error beve_to_json(const std::string& beve, Buffer& out) noexcept {
      size_t ix{}; // write index
      
      auto* it = beve.data();
      auto* end = it + beve.size();
      
      context ctx{};
      
      detail::beve_to_json_value(ctx, it, end, out, ix);
      if (bool(ctx.error)) {
         return {ctx.error};
      }
      
      if constexpr (detail::resizeable<Buffer>) {
         out.resize(ix);
      }
      
      return {};
   }
}
