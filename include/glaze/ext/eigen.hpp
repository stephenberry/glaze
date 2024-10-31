// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if __has_include(<Eigen/Core>)
#include <Eigen/Core>
#else
static_assert(false, "Eigen must be included to use glaze/ext/eigen.hpp");
#endif

#include <span>

#include "glaze/api/std/array.hpp"
#include "glaze/beve/read.hpp"
#include "glaze/beve/write.hpp"
#include "glaze/core/common.hpp"
#include "glaze/core/meta.hpp"
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
      struct from<BEVE, T>
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
            detail::read<BEVE>::op<Opts>(extents, ctx, it, end);

            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::read<BEVE>::op<Opts>(view, ctx, it, end);
         }
      };

      template <matrix_t T>
         requires(T::RowsAtCompileTime < 0 || T::ColsAtCompileTime < 0)
      struct from<BEVE, T>
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
            detail::read<BEVE>::op<Opts>(extents, ctx, it, end);

            std::span<typename T::Scalar> view(value.data(), extents[0] * extents[1]);
            detail::read<BEVE>::op<Opts>(view, ctx, it, end);
         }
      };

      template <matrix_t T>
         requires(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0)
      struct to<BEVE, T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&&... args)
         {
            constexpr uint8_t matrix = 0b00010'000;
            constexpr uint8_t tag = tag::extensions | matrix;
            dump_type(tag, args...);

            constexpr uint8_t layout = uint8_t(!T::IsRowMajor);
            dump_type(layout, args...);

            std::array<Eigen::Index, 2> extents{T::RowsAtCompileTime, T::ColsAtCompileTime};
            detail::write<BEVE>::op<Opts>(extents, ctx, args...);

            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::write<BEVE>::op<Opts>(view, ctx, args...);
         }
      };

      template <matrix_t T>
         requires(T::RowsAtCompileTime < 0 || T::ColsAtCompileTime < 0)
      struct to<BEVE, T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&&... args)
         {
            constexpr uint8_t matrix = 0b00010'000;
            constexpr uint8_t tag = tag::extensions | matrix;
            dump_type(tag, args...);

            constexpr uint8_t layout = uint8_t(!T::IsRowMajor);
            dump_type(layout, args...);

            std::array<Eigen::Index, 2> extents{value.rows(), value.cols()};
            detail::write<BEVE>::op<Opts>(extents, ctx, args...);

            std::span<typename T::Scalar> view(value.data(), extents[0] * extents[1]);
            detail::write<BEVE>::op<Opts>(view, ctx, args...);
         }
      };

      template <matrix_t T>
         requires(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0)
      struct from<JSON, T>
      {
         template <auto Opts>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::read<JSON>::op<Opts>(view, ctx, it, end);
         }
      };

      template <matrix_t T>
         requires(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0)
      struct to<JSON, T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix)
         {
            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::write<JSON>::op<Opts>(view, ctx, b, ix);
         }
      };
   } // namespace detail
} // namespace glaze

template <class Scalar, int Rows, int Cols>
struct glz::meta<Eigen::Matrix<Scalar, Rows, Cols>>
{
   static constexpr std::string_view name = join_v<chars<"Eigen::Matrix<">, name_v<Scalar>, chars<",">, //
                                                   chars<num_to_string<Rows>::value>, chars<",">, //
                                                   chars<num_to_string<Cols>::value>, chars<",">, //
                                                   chars<">">>;
};
