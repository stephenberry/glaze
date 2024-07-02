// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/network/socket.hpp"
#include "glaze/network/socket_io.hpp"
#include "glaze/rpc/repe.hpp"

namespace glz
{
   template <opts Opts = opts{}>
   struct repe_client
   {
      std::string hostname{"127.0.0.1"};
      uint16_t port{};
      glz::socket socket{};

      struct glaze
      {
         using T = repe_client;
         static constexpr auto value = glz::object(&T::hostname, &T::port);
      };

      std::shared_ptr<repe::buffer_pool> buffer_pool = std::make_shared<repe::buffer_pool>();

      [[nodiscard]] std::error_code init()
      {
         if (auto ec = socket.connect(hostname, port)) {
            return ec;
         }
         
         if (not socket.no_delay()) {
            return {ip_error::socket_bind_failed, ip_error_category::instance()};
         }
         
         return {};
      }

      template <class Params>
      [[nodiscard]] repe::error_t notify(repe::header&& header, Params&& params)
      {
         repe::unique_buffer ubuffer{buffer_pool.get()};
         auto& buffer = ubuffer.value();

         header.notify = true;
         const auto ec = repe::request<Opts>(std::move(header), std::forward<Params>(params), buffer);
         if (bool(ec)) [[unlikely]] {
            return {repe::error_e::invalid_params, glz::format_error(ec, buffer)};
         }

         if (auto ec = send<Opts>(socket, buffer)) {
            return {ec.value(), ec.message()};
         }
         return {};
      }

      template <class Result>
      [[nodiscard]] repe::error_t get(repe::header&& header, Result&& result)
      {
         repe::unique_buffer ubuffer{buffer_pool.get()};
         auto& buffer = ubuffer.value();

         header.notify = false;
         header.empty = true; // no params
         const auto ec = repe::request<Opts>(std::move(header), nullptr, buffer);
         if (bool(ec)) [[unlikely]] {
            return {repe::error_e::invalid_params, glz::format_error(ec, buffer)};
         }

         if (auto ec = send<Opts>(socket, buffer)) {
            return {ec.value(), ec.message()};
         }
         if (auto ec = receive<Opts>(socket, buffer)) {
            return {ec.value(), ec.message()};
         }

         return repe::decode_response<Opts>(std::forward<Result>(result), buffer);
      }

      template <class Result = glz::raw_json>
      [[nodiscard]] glz::expected<Result, std::error_code> get(repe::header&& header)
      {
         std::decay_t<Result> result{};
         const auto error = get<Result>(std::move(header), result);
         if (error) {
            return glz::unexpected(error);
         }
         else {
            return {result};
         }
      }

      template <class Params>
      [[nodiscard]] repe::error_t set(repe::header&& header, Params&& params)
      {
         repe::unique_buffer ubuffer{buffer_pool.get()};
         auto& buffer = ubuffer.value();

         header.notify = false;
         const auto ec = repe::request<Opts>(std::move(header), std::forward<Params>(params), buffer);
         if (bool(ec)) [[unlikely]] {
            return {repe::error_e::invalid_params, glz::format_error(ec, buffer)};
         }

         if (auto ec = send<Opts>(socket, buffer)) {
            return {ec.value(), ec.message()};
         }
         if (auto ec = receive<Opts>(socket, buffer)) {
            return {ec.value(), ec.message()};
         }

         return repe::decode_response<Opts>(buffer);
      }

      template <class Params, class Result>
      [[nodiscard]] repe::error_t call(repe::header&& header, Params&& params, Result&& result)
      {
         repe::unique_buffer ubuffer{buffer_pool.get()};
         auto& buffer = ubuffer.value();

         header.notify = false;
         const auto ec = repe::request<Opts>(std::move(header), std::forward<Params>(params), buffer);
         if (bool(ec)) [[unlikely]] {
            return {repe::error_e::invalid_params, glz::format_error(ec, buffer)};
         }

         if (auto ec = send<Opts>(socket, buffer)) {
            return {ec.value(), ec.message()};
         }
         if (auto ec = receive<Opts>(socket, buffer)) {
            return {ec.value(), ec.message()};
         }

         return repe::decode_response<Opts>(std::forward<Result>(result), buffer);
      }

      [[nodiscard]] repe::error_t call(repe::header&& header)
      {
         repe::unique_buffer ubuffer{buffer_pool.get()};
         auto& buffer = ubuffer.value();

         header.notify = false;
         header.empty = true; // because no value provided
         const auto ec = glz::write_json(std::forward_as_tuple(std::move(header), nullptr), buffer);
         if (bool(ec)) [[unlikely]] {
            return {repe::error_e::invalid_params, glz::format_error(ec, buffer)};
         }

         if (auto ec = send<Opts>(socket, buffer)) {
            return {ec.value(), ec.message()};
         }
         if (auto ec = receive<Opts>(socket, buffer)) {
            return {ec.value(), ec.message()};
         }

         return repe::decode_response<Opts>(buffer);
      }
   };
}
