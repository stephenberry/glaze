// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <asio.hpp>
#include <cassert>
#include <coroutine>

#include "glaze/rpc/repe.hpp"

namespace glz
{
   inline void send_buffer(asio::ip::tcp::socket& socket, const std::string_view str)
   {
      uint64_t size = str.size();
      asio::write(socket, asio::buffer(&size, sizeof(uint64_t)), asio::transfer_exactly(sizeof(uint64_t)));
      asio::write(socket, asio::buffer(str), asio::transfer_exactly(size));
   }
   
   inline void receive_buffer(asio::ip::tcp::socket& socket, std::string& str)
   {
      uint64_t size;
      asio::read(socket, asio::buffer(&size, sizeof(size)), asio::transfer_exactly(sizeof(uint64_t)));
      str.resize(size);
      asio::read(socket, asio::buffer(str), asio::transfer_exactly(size));
   }
   
   inline asio::awaitable<void> co_send_buffer(asio::ip::tcp::socket& socket, const std::string_view str)
   {
      const uint64_t size = str.size();
      co_await asio::async_write(socket, asio::buffer(&size, sizeof(uint64_t)), asio::use_awaitable);
      co_await asio::async_write(socket, asio::buffer(str), asio::use_awaitable);
   }
   
   inline asio::awaitable<void> co_receive_buffer(asio::ip::tcp::socket& socket, std::string& str)
   {
      uint64_t size;
      co_await asio::async_read(socket, asio::buffer(&size, sizeof(size)), asio::use_awaitable);
      str.resize(size);
      co_await asio::async_read(socket, asio::buffer(str), asio::use_awaitable);
   }
   
   inline asio::awaitable<void> call_rpc(asio::ip::tcp::socket& socket, std::string& buffer)
   {
      co_await co_send_buffer(socket, buffer);
      co_await co_receive_buffer(socket, buffer);
   }
   
   template <opts Opts = opts{}>
   struct asio_client
   {
      std::string_view host{"localhost"};
      std::string_view service{"1234"};
      asio::io_context ctx{1};
      asio::ip::tcp::socket socket{ctx};
      asio::ip::tcp::resolver resolver{ctx};

      std::string buffer{};

      asio_client()
      {
         auto endpoints = resolver.resolve(host, service);
         asio::connect(socket, endpoints);
      }

      template <class Params>
      void notify(repe::header&& header, Params&& params)
      {
         header.action |= repe::notify;
         repe::request<Opts>(std::move(header), std::forward<Params>(params), buffer);

         send_buffer(socket, buffer);
      }
      
      template <class Result>
      [[nodiscard]] repe::error_t call(repe::header&& header, Result&& result)
      {
         header.action &= ~repe::notify; // clear invalid notify
         header.action |= repe::empty; // no params
         repe::request<Opts>(std::move(header), nullptr, buffer);

         send_buffer(socket, buffer);
         receive_buffer(socket, buffer);

         return repe::decode_response<Opts>(std::forward<Result>(result), buffer);
      }

      template <class Params, class Result>
      [[nodiscard]] repe::error_t call(repe::header&& header, Params&& params, Result&& result)
      {
         header.action &= ~repe::notify; // clear invalid notify
         repe::request<Opts>(std::move(header), std::forward<Params>(params), buffer);

         send_buffer(socket, buffer);
         receive_buffer(socket, buffer);

         return repe::decode_response<Opts>(std::forward<Result>(result), buffer);
      }
      
      template <class Result, class Params>
      [[nodiscard]] std::function<Result(Params)> callable(repe::header&& header) {
         if constexpr (std::same_as<Params, void>) {
            header.action |= repe::empty;
            return [this, h = std::move(header)]() mutable -> Result {
               std::decay_t<Result> result{};
               const auto e = call(repe::header{h}, result);
               if (e) {
                  throw std::runtime_error(e.message);
               }
               return result;
            };
         }
         else {
            header.action &= ~repe::empty;
            return [this, h = std::move(header)](Params params) mutable -> Result {
               std::decay_t<Result> result{};
               const auto e = call(repe::header{h}, params, result);
               if (e) {
                  throw std::runtime_error(e.message);
               }
               return result;
            };
         }
      }

      template <class Params>
      [[nodiscard]] std::string& call_raw(repe::header&& header, Params&& params)
      {
         header.action &= ~repe::notify; // clear invalid notify
         repe::request<Opts>(std::move(header), std::forward<Params>(params), buffer);

         send_buffer(socket, buffer);
         receive_buffer(socket, buffer);
         return buffer;
      }
   };
   
   template <class T>
   concept is_registry = requires(T t) {
      {
         t.call(std::declval<std::string&>())
      };
   };

   template <is_registry Registry>
   struct asio_server
   {
      asio::io_context ctx{1}; // number of background threads

      asio::signal_set signals{ctx, SIGINT, SIGTERM};

      std::function<void(Registry&)> init{};

      void run()
      {
         signals.async_wait([&](auto, auto) { ctx.stop(); });

         asio::co_spawn(ctx, listener(), asio::detached);

         ctx.run();
      }

      asio::awaitable<void> run_instance(asio::ip::tcp::socket socket)
      {
         Registry server{};
         std::string buffer{};

         try {
            if (init) {
               init(server);
            }

            while (true) {
               co_await co_receive_buffer(socket, buffer);
               if (server.call(buffer)) {
                  co_await co_send_buffer(socket, server.response);
               }
            }
         }
         catch (std::exception& e) {
            std::fprintf(stderr, "%s\n", e.what());
         }
      }

      asio::awaitable<void> listener()
      {
         auto executor = co_await asio::this_coro::executor;
         asio::ip::tcp::acceptor acceptor(executor, {asio::ip::tcp::v4(), 1234});
         while (true) {
            auto socket = co_await acceptor.async_accept(asio::use_awaitable);
            asio::co_spawn(executor, run_instance(std::move(socket)), asio::detached);
         }
      }
   };
}
