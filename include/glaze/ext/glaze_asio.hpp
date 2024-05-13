// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if __has_include(<asio.hpp>)
#include <asio.hpp>
#else
static_assert(false, "standalone asio must be included to use glaze/ext/glaze_asio.hpp");
#endif

#include <cassert>
#include <coroutine>
#include <iostream>

#include "glaze/rpc/repe.hpp"

namespace glz
{
   inline void send_buffer(asio::ip::tcp::socket& socket, const std::string_view str)
   {
      const uint64_t size = str.size();
      std::array<asio::const_buffer, 2> buffers{asio::buffer(&size, sizeof(uint64_t)), asio::buffer(str)};

      asio::write(socket, buffers, asio::transfer_exactly(sizeof(uint64_t) + size));
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
      std::array<asio::const_buffer, 2> buffers{asio::buffer(&size, sizeof(uint64_t)), asio::buffer(str)};

      co_await asio::async_write(socket, buffers, asio::transfer_exactly(sizeof(uint64_t) + size), asio::use_awaitable);
   }

   inline asio::awaitable<void> co_receive_buffer(asio::ip::tcp::socket& socket, std::string& str)
   {
      uint64_t size;
      co_await asio::async_read(socket, asio::buffer(&size, sizeof(size)), asio::transfer_exactly(sizeof(uint64_t)),
                                asio::use_awaitable);
      str.resize(size);
      co_await asio::async_read(socket, asio::buffer(str), asio::transfer_exactly(size), asio::use_awaitable);
   }

   inline asio::awaitable<void> call_rpc(asio::ip::tcp::socket& socket, std::string& buffer)
   {
      co_await co_send_buffer(socket, buffer);
      co_await co_receive_buffer(socket, buffer);
   }

   template <class>
   struct func_traits;

   template <class Result>
   struct func_traits<Result()>
   {
      using result_type = Result;
      using params_type = void;
      using std_func_sig = std::function<Result()>;
   };

   template <class Result, class Params>
   struct func_traits<Result(Params)>
   {
      using result_type = Result;
      using params_type = Params;
      using std_func_sig = std::function<Result(Params)>;
   };

   template <class T>
   using func_result_t = typename func_traits<T>::result_type;

   template <class T>
   using func_params_t = typename func_traits<T>::params_type;

   template <class T>
   using std_func_sig_t = typename func_traits<T>::std_func_sig;

   template <opts Opts = opts{}>
   struct asio_client
   {
      std::string host{"localhost"};
      std::string service{""};
      uint32_t concurrency{1};

      struct glaze
      {
         using T = asio_client;
         static constexpr auto value = glz::object(&T::host, &T::service, &T::concurrency);
      };

      std::shared_ptr<asio::io_context> ctx{};
      std::shared_ptr<asio::ip::tcp::socket> socket{};

      std::string buffer{};

      [[nodiscard]] std::error_code init()
      {
         ctx = std::make_shared<asio::io_context>(concurrency);
         socket = std::make_shared<asio::ip::tcp::socket>(*ctx);
         asio::ip::tcp::resolver resolver{*ctx};
         const auto endpoint = resolver.resolve(host, service);
         std::error_code ec{};
         asio::connect(*socket, endpoint, ec);
         if (ec) {
            return ec;
         }
         return socket->set_option(asio::ip::tcp::no_delay(true), ec);
      }

      template <class Params>
      void notify(repe::header&& header, Params&& params)
      {
         header.action |= repe::notify;
         repe::request<Opts>(std::move(header), std::forward<Params>(params), buffer);

         send_buffer(*socket, buffer);
      }

      template <class Result>
      [[nodiscard]] repe::error_t get(repe::header&& header, Result&& result)
      {
         header.action &= ~repe::notify; // clear invalid notify
         header.action |= repe::empty; // no params
         repe::request<Opts>(std::move(header), nullptr, buffer);

         send_buffer(*socket, buffer);
         receive_buffer(*socket, buffer);

         return repe::decode_response<Opts>(std::forward<Result>(result), buffer);
      }

      template <class Result = glz::raw_json>
      [[nodiscard]] glz::expected<Result, repe::error_t> get(repe::header&& header)
      {
         header.action &= ~repe::notify; // clear invalid notify
         header.action |= repe::empty; // no params
         repe::request<Opts>(std::move(header), nullptr, buffer);

         send_buffer(*socket, buffer);
         receive_buffer(*socket, buffer);

         std::decay_t<Result> result{};
         const auto error = repe::decode_response<Opts>(result, buffer);
         if (error) {
            return glz::unexpected(error);
         }
         else {
            return result;
         }
      }

      template <class Params>
      [[nodiscard]] repe::error_t set(repe::header&& header, Params&& params)
      {
         header.action &= ~repe::notify; // clear invalid notify
         repe::request<Opts>(std::move(header), std::forward<Params>(params), buffer);

         send_buffer(*socket, buffer);
         receive_buffer(*socket, buffer);

         return repe::decode_response<Opts>(buffer);
      }

      template <class Params, class Result>
      [[nodiscard]] repe::error_t call(repe::header&& header, Params&& params, Result&& result)
      {
         header.action &= ~repe::notify; // clear invalid notify
         repe::request<Opts>(std::move(header), std::forward<Params>(params), buffer);

         send_buffer(*socket, buffer);
         receive_buffer(*socket, buffer);

         return repe::decode_response<Opts>(std::forward<Result>(result), buffer);
      }

      [[nodiscard]] repe::error_t call(repe::header&& header)
      {
         header.action &= ~repe::notify; // clear invalid notify
         header.action |= repe::empty; // because no value provided
         glz::write_json(std::forward_as_tuple(std::move(header), nullptr), buffer);

         send_buffer(*socket, buffer);
         receive_buffer(*socket, buffer);

         return repe::decode_response<Opts>(buffer);
      }

      template <class Func>
      [[nodiscard]] std_func_sig_t<Func> callable(repe::header&& header)
      {
         using Params = func_params_t<Func>;
         using Result = func_result_t<Func>;
         if constexpr (std::same_as<Params, void>) {
            header.action |= repe::empty;
            return [this, h = std::move(header)]() mutable -> Result {
               std::decay_t<Result> result{};
               const auto e = call(repe::header{h}, result);
               if (e) {
                  throw std::runtime_error(glz::write_json(e));
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

         send_buffer(*socket, buffer);
         receive_buffer(*socket, buffer);
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
      uint16_t port{};
      uint32_t concurrency{1};

      struct glaze
      {
         using T = asio_server;
         static constexpr auto value = glz::object(&T::port, &T::concurrency);
      };

      std::shared_ptr<asio::io_context> ctx{};
      std::shared_ptr<asio::signal_set> signals{};

      std::function<void(Registry&)> init_registry{};

      bool initialized = false;

      void init()
      {
         if (!initialized) {
            ctx = std::make_shared<asio::io_context>(concurrency);
            signals = std::make_shared<asio::signal_set>(*ctx, SIGINT, SIGTERM);
         }
         initialized = true;
      }

      void run()
      {
         if (!initialized) {
            init();
         }

         signals->async_wait([&](auto, auto) { ctx->stop(); });

         asio::co_spawn(*ctx, listener(), asio::detached);

         ctx->run();
      }

      asio::awaitable<void> run_instance(asio::ip::tcp::socket socket)
      {
         socket.set_option(asio::ip::tcp::no_delay(true));
         Registry registry{};
         std::string buffer{};

         try {
            if (init_registry) {
               init_registry(registry);
            }

            while (true) {
               co_await co_receive_buffer(socket, buffer);
               if (registry.call(buffer)) {
                  co_await co_send_buffer(socket, registry.response);
               }
            }
         }
         catch (const std::exception& e) {
            std::fprintf(stderr, "%s\n", e.what());
         }
      }

      asio::awaitable<void> listener()
      {
         auto executor = co_await asio::this_coro::executor;
         asio::ip::tcp::acceptor acceptor(executor, {asio::ip::tcp::v6(), port});
         while (true) {
            auto socket = co_await acceptor.async_accept(asio::use_awaitable);
            asio::co_spawn(executor, run_instance(std::move(socket)), asio::detached);
         }
      }
   };
}
