// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>

namespace glz
{
   template <typename... types>
   struct type_list;

   template <typename value_type, typename... rest>
   struct type_list<value_type, rest...>
   {
      using current_type = value_type;
      using remaining_types = type_list<rest...>;
      static constexpr uint64_t size = 1 + sizeof...(rest);
   };

   template <typename value_type>
   struct type_list<value_type>
   {
      using current_type = value_type;
      static constexpr uint64_t size = 1;
   };

   template <typename type_list, uint64_t Index>
   struct get_type_at_index;

   template <typename value_type, typename... rest>
   struct get_type_at_index<type_list<value_type, rest...>, 0>
   {
      using type = value_type;
   };

   template <typename value_type, typename... rest, uint64_t Index>
   struct get_type_at_index<type_list<value_type, rest...>, Index>
   {
      using type = typename get_type_at_index<type_list<rest...>, Index - 1>::type;
   };

   using integer_list = type_list<uint64_t, uint32_t, uint16_t, uint8_t>;

   template <uint64_t index = 0, typename char_type01, typename char_type02>
   GLZ_ALWAYS_INLINE bool compare(char_type01* string1, char_type02* string2, uint64_t lengthNew)
   {
      using integer_type = typename get_type_at_index<integer_list, index>::type;
      static constexpr uint64_t size{sizeof(integer_type)};
      integer_type value01[2]{};
      while (lengthNew >= size) {
         std::memcpy(value01, string1, sizeof(integer_type));
         std::memcpy(value01 + 1, string2, sizeof(integer_type));
         lengthNew -= size;
         string1 += size;
         string2 += size;
         if (value01[0] != value01[1]) {
            return false;
         }
      }
      if constexpr (index < integer_list::size - 1) {
         return compare<index + 1>(string1, string2, lengthNew);
      }
      else {
         return true;
      }
   }
}
