// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/common.hpp"
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
      }
      &&!nano::ranges::range<T>;

      template <matrix_t Matrix>
      struct custom<Matrix>
      {
         template <class T>
         static void from_iter(T &value, auto&& it, auto&& end)
         {
            skip_ws(it, end);
            match<'['>(it, end);
            skip_ws(it, end);

            match<'['>(it, end);
            skip_ws(it, end);
            int rows, cols;
            glaze::detail::from_iter(rows, it, end);
            skip_ws(it, end);
            match<','>(it, end);
            skip_ws(it, end);
            glaze::detail::from_iter(cols, it, end);
            skip_ws(it, end);
            match<']'>(it, end);
            skip_ws(it, end);

            
            match<','>(it, end);
            skip_ws(it, end);

            value.resize(rows, cols);
            std::span view(value.data(), value.size());
            glaze::detail::from_iter(view, it, end);
            skip_ws(it, end);

            match<']'>(it, end);
         }

         template <bool C = false, class T, class B>
         static void to_buffer(T &&value, B &&b)
         {
            write<'['>(b);

            // Write shape
            write<'['>(b);
            glaze::detail::to_buffer(value.rows(), b);
            write<','>(b);
            glaze::detail::to_buffer(value.cols(), b);
            write<"],">(b);

            // Write data
            write<'['>(b);
            const auto size = value.size();
            const auto data = value.data();
            if (size >= 1) glaze::detail::to_buffer(*(data), b);
            for (decltype(value.size()) i = 1; i < size; ++i) {
               write<','>(b);
               glaze::detail::to_buffer(*(data + i), b);
            }
            write<']'>(b);

            write<']'>(b);
         }

         template <class F, class T>
         static bool seek_impl(F &&func, T &&value, std::string_view json_ptr)
         {
            return false;
         }
      };
   }  // namespace detail
}  // namespace vireo
