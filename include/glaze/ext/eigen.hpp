// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <Eigen/Core> //Note: You are expected to provide eigen if including this header
#include <span>

#include "glaze/api/name.hpp"
#include "glaze/api/std/array.hpp"
#include "glaze/binary/read.hpp"
#include "glaze/binary/write.hpp"
#include "glaze/core/common.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   namespace detail
   {
      template <class T>
      concept matrix_t = requires(T matrix) {
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
      } && !range<T>;

      template <matrix_t T>
         requires(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0)
      struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            ++it;
            constexpr uint8_t layout = uint8_t(!T::IsRowMajor);
            if (uint8_t(*it) != layout) {
               ctx.error = error_code::syntax_error;
            }
            ++it;
            std::array<Eigen::Index, 2> extents;
            detail::read<binary>::op<Opts>(extents, ctx, it, end);

            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::read<binary>::op<Opts>(view, ctx, it, end);
         }
      };

      template <matrix_t T>
         requires(T::RowsAtCompileTime < 0 || T::ColsAtCompileTime < 0)
      struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            ++it;
            constexpr uint8_t layout = uint8_t(!T::IsRowMajor);
            if (uint8_t(*it) != layout) {
               ctx.error = error_code::syntax_error;
            }
            ++it;
            std::array<Eigen::Index, 2> extents;
            detail::read<binary>::op<Opts>(extents, ctx, it, end);

            std::span<typename T::Scalar> view(value.data(), extents[0] * extents[1]);
            detail::read<binary>::op<Opts>(view, ctx, it, end);
         }
      };

      template <matrix_t T>
         requires(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0)
      struct to_binary<T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            constexpr uint8_t matrix = 0b00010'000;
            constexpr uint8_t tag = tag::extensions | matrix;
            dump_type(tag, args...);

            constexpr uint8_t layout = uint8_t(!T::IsRowMajor);
            dump_type(layout, args...);

            std::array<Eigen::Index, 2> extents{T::RowsAtCompileTime, T::ColsAtCompileTime};
            detail::write<binary>::op<Opts>(extents, ctx, args...);

            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::write<binary>::op<Opts>(view, ctx, args...);
         }
      };

      template <matrix_t T>
         requires(T::RowsAtCompileTime < 0 || T::ColsAtCompileTime < 0)
      struct to_binary<T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            constexpr uint8_t matrix = 0b00010'000;
            constexpr uint8_t tag = tag::extensions | matrix;
            dump_type(tag, args...);

            constexpr uint8_t layout = uint8_t(!T::IsRowMajor);
            dump_type(layout, args...);

            std::array<Eigen::Index, 2> extents{value.rows(), value.cols()};
            detail::write<binary>::op<Opts>(extents, ctx, args...);

            std::span<typename T::Scalar> view(value.data(), extents[0] * extents[1]);
            detail::write<binary>::op<Opts>(view, ctx, args...);
         }
      };

      template <matrix_t T>
         requires(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0)
      struct from_json<T>
      {
         template <auto Opts>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::read<json>::op<Opts>(view, ctx, it, end);
         }
      };

      template <matrix_t T>
         requires(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0)
      struct to_json<T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
         {
            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::write<json>::op<Opts>(view, ctx, b, ix);
         }
      };
   } // namespace detail
} // namespace glaze

template <class Scalar, int Rows, int Cols>
struct glz::meta<Eigen::Matrix<Scalar, Rows, Cols>>
{
   static constexpr std::string_view name = detail::join_v<chars<"Eigen::Matrix<">, name_v<Scalar>, chars<",">, //
                                                           chars<num_to_string<Rows>::value>, chars<",">, //
                                                           chars<num_to_string<Cols>::value>, chars<",">, //
                                                           chars<">">>;
};
