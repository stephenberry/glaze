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
      
      template <matrix_t T>
      struct from_json<T>
      {
         static void op(T &value, auto&& it, auto&& end)
         {
            skip_ws(it, end);
            match<'['>(it, end);
            skip_ws(it, end);

            match<'['>(it, end);
            skip_ws(it, end);
            int rows, cols;
            detail::read<json>::op(rows, it, end);
            skip_ws(it, end);
            match<','>(it, end);
            skip_ws(it, end);
            detail::read<json>::op(cols, it, end);
            skip_ws(it, end);
            match<']'>(it, end);
            skip_ws(it, end);
            
            match<','>(it, end);
            skip_ws(it, end);

            value.resize(rows, cols);
            std::span view(value.data(), value.size());
            detail::read<json>::op(view, it, end);
            skip_ws(it, end);

            match<']'>(it, end);
         }
      };

      /*template <matrix_t Matrix>
      struct custom<Matrix>
      {
         template <class T>
         static void from_json(T &value, auto&& it, auto&& end)
         {
            skip_ws(it, end);
            match<'['>(it, end);
            skip_ws(it, end);

            match<'['>(it, end);
            skip_ws(it, end);
            int rows, cols;
            detail::read<json>::op(rows, it, end);
            skip_ws(it, end);
            match<','>(it, end);
            skip_ws(it, end);
            detail::read<json>::op(cols, it, end);
            skip_ws(it, end);
            match<']'>(it, end);
            skip_ws(it, end);
            
            match<','>(it, end);
            skip_ws(it, end);

            value.resize(rows, cols);
            std::span view(value.data(), value.size());
            detail::read<json>::op(view, it, end);
            skip_ws(it, end);

            match<']'>(it, end);
         }

         template <bool C = false, class T, class B>
         static void to_buffer(T &&value, B &&b)
         {
            write<'['>(b);

            // Write shape
            write<'['>(b);
            detail::to_buffer(value.rows(), b);
            write<','>(b);
            detail::to_buffer(value.cols(), b);
            write<"],">(b);

            // Write data
            write<'['>(b);
            const auto size = value.size();
            const auto data = value.data();
            if (size >= 1) glaze::detail::to_buffer(*(data), b);
            for (decltype(value.size()) i = 1; i < size; ++i) {
               write<','>(b);
               detail::to_buffer(*(data + i), b);
            }
            write<']'>(b);

            write<']'>(b);
         }

         template <class F, class T>
         static bool seek_impl(F &&func, T &&value, std::string_view json_ptr)
         {
            if (json_ptr.empty()) {
               func(value);
               return true;
            }
            return false;
         }
      };*/
   }  // namespace detail
}  // namespace glaze
