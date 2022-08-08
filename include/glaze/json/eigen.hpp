// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

#include <span>

namespace glaze
{
   namespace detail
   {
      template <class T>
      concept matrix_t = requires(T matrix)
      {
         matrix.resize(2, 4);
         matrix.data();
         {
            matrix.rows()
            } -> std::convertible_to<size_t>;
         {
            matrix.cols()
            } -> std::convertible_to<size_t>;
         {
            matrix.size()
            } -> std::convertible_to<size_t>;
      } &&!nano::ranges::range<T>;
      
      template <matrix_t T>
      struct from_json<T>
      {
         static_assert(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0,
                       "Does not handle dynamic matrices");

         static void op(auto &value, auto&& it, auto&& end)
         {
            std::span view(value.data(), value.size());
            detail::read<json>::op(view, it, end);
         }
      };

      template <matrix_t T>
      struct to_json<T>
      {
         static_assert(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0,
                       "Does not handle dynamic matrices");

         template <bool C>
         static void op(auto &&value, auto &&b) noexcept
         {
            dump<'['>(b);
            if (value.size() != 0) {
               auto it = value.data();
               write<json>::op<C>(*it, b);
               ++it;
               const auto end = value.data() + value.size();
               for (; it != end; ++it) {
                  dump<','>(b);
                  write<json>::op<C>(*it, b);
               }   
            }
            dump<']'>(b);
         }
      };
   }  // namespace detail
}  // namespace glaze
