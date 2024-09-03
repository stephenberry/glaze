// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <exec/any_sender_of.hpp>
#include <exec/async_scope.hpp>
#include <exec/repeat_effect_until.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/task.hpp>
#include <stdexec/execution.hpp>

#include "glaze/core/error.hpp"
#include "glaze/glaze.hpp"
#include "glaze/network/socket.hpp"

namespace glz
{
   template <class Scheduler>
   auto async_connect(Scheduler&& sched, socket& sock, const std::string& address, const int port)
   {
      return stdexec::just() | stdexec::then([&sock, address, port]() { return sock.connect(address, port); }) |
             stdexec::on(std::forward<Scheduler>(sched));
   }

   template <class Scheduler>
   auto async_bind_and_listen(Scheduler&& sched, socket& sock, int port)
   {
      return stdexec::just() | stdexec::then([&sock, port]() { return sock.bind_and_listen(port); }) |
             stdexec::on(std::forward<Scheduler>(sched));
   }

   template <typename Scheduler>
   auto async_send(Scheduler&& sched, socket& sock, const std::string_view buffer)
   {
      return stdexec::just() | stdexec::let_value([&sock, buffer = std::string(buffer)]() mutable {
                size_t bytes_sent = 0;
                return exec::repeat_effect_until(
                   stdexec::then([&]() {
                      auto result = sock.send(buffer, bytes_sent);
                      if (result == socket::io_result::error) {
                         throw std::system_error(errno, std::system_category(), "Send failed");
                      }
                      return result == socket::io_result::completed;
                   }) |
                   stdexec::upon_error([](auto&&) { return true; }));
             }) |
             stdexec::on(std::forward<Scheduler>(sched));
   }

   template <ip_header Header, typename Scheduler>
   auto async_receive(Scheduler&& sched, socket& sock)
   {
      return stdexec::just() | stdexec::let_value([&sock]() {
                Header header{};
                std::string buffer;
                size_t bytes_received = 0;
                return exec::repeat_effect_until(stdexec::then([&]() {
                                                       auto result = sock.receive(header, buffer, bytes_received);
                                                       if (result == socket::io_result::error) {
                                                          throw std::system_error(errno,
                                                                                  std::system_category(),
                                                                                  "Receive failed");
                                                       }
                                                       return result == socket::io_result::completed;
                                                    }) |
                                                    stdexec::upon_error([](auto&&) { return true; })) |
                       stdexec::then([header = std::move(header), buffer = std::move(buffer)]() mutable {
                          return std::make_pair(std::move(header), std::move(buffer));
                       });
             }) |
             stdexec::on(std::forward<Scheduler>(sched));
   }
}

namespace glz
{
   template <opts Opts = opts{.format = binary}, class T, typename Scheduler>
   auto async_receive_value(Scheduler&& sched, socket& sckt, T& value)
   {
       return stdexec::just() |
              stdexec::let_value([&sckt, &value, &sched]() {
                  return async_receive<uint64_t>(sched, sckt)
                      | stdexec::then([&value](auto&& result) {
                            auto [header, buffer] = std::move(result);
                            auto ec = glz::read<Opts>(value, buffer);
                            if (ec) {
                                throw std::system_error(int(ec.ec), error_category::instance());
                            }
                        });
              }) |
              stdexec::on(std::forward<Scheduler>(sched));
   }

   template <opts Opts = opts{.format = binary}, class T, typename Scheduler>
   auto async_send_value(Scheduler&& sched, socket& sckt, const T& value)
   {
       return stdexec::just() |
              stdexec::let_value([&sckt, &value, &sched]() {
                  std::string buffer;
                  auto ec = glz::write<Opts>(value, buffer);
                  if (ec) {
                      throw std::system_error(int(ec.ec), error_category::instance());
                  }

                  uint64_t header = uint64_t(buffer.size());
                  return async_send(sched, sckt, std::string_view{reinterpret_cast<char*>(&header), sizeof(header)})
                      | stdexec::then([&sckt, buffer = std::move(buffer), &sched]() mutable {
                            return async_send(sched, sckt, std::move(buffer));
                        });
              }) |
              stdexec::on(std::forward<Scheduler>(sched));
   }
}

namespace glz
{
   /*template <opts Opts = opts{.format = binary}, class Buffer>
   [[nodiscard]] std::error_code receive(socket& sckt, Buffer& buffer)
   {
      uint64_t header{};
      if (auto ec = sckt.receive(header, buffer)) {
         return ec;
      }
      return {};
   }

   template <opts Opts = opts{.format = binary}, class Buffer>
   [[nodiscard]] std::error_code send(socket& sckt, Buffer& buffer)
   {
      uint64_t header = uint64_t(buffer.size());

      if (auto ec = sckt.send(sv{reinterpret_cast<char*>(&header), sizeof(header)})) {
         return ec;
      }

      if (auto ec = sckt.send(buffer)) {
         return ec;
      }

      return {};
   }*/

   template <opts Opts = opts{.format = binary}, class T>
   [[nodiscard]] std::error_code receive_value(socket& sckt, T&& value)
   {
      static thread_local std::string buffer{};

      uint64_t header{};
      if (auto ec = sckt.receive(header, buffer)) {
         return ec;
      }

      if (auto ec = glz::read<Opts>(std::forward<T>(value), buffer)) {
         return {int(ec.ec), error_category::instance()};
      }

      return {};
   }

   template <opts Opts = opts{.format = binary}, class T>
   [[nodiscard]] std::error_code send_value(socket& sckt, T&& value)
   {
      static thread_local std::string buffer{};

      if (auto ec = glz::write<Opts>(std::forward<T>(value), buffer)) {
         return {int(ec.ec), error_category::instance()};
      }

      uint64_t header = uint64_t(buffer.size());

      if (auto ec = sckt.send(sv{reinterpret_cast<char*>(&header), sizeof(header)})) {
         return ec;
      }

      if (auto ec = sckt.send(buffer)) {
         return ec;
      }

      return {};
   }
}
