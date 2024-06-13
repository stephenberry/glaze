// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if __has_include(<asio.hpp>) && !defined(GLZ_USE_BOOST_ASIO)
#include <asio.hpp>
#elif __has_include(<boost/asio.hpp>)
#ifndef GLZ_USING_BOOST_ASIO
#define GLZ_USING_BOOST_ASIO
#endif
#include <boost/asio.hpp>
#else
static_assert(false, "standalone asio must be included to use glaze/ext/glaze_asio.hpp");
#endif

#include <cassert>
#include <coroutine>
#include <iostream>

#include "glaze/rpc/repe.hpp"

namespace glz
{
#if defined(GLZ_USING_BOOST_ASIO)
   namespace asio = boost::asio;
#endif
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
      std::string host{"localhost"}; // host name
      std::string service{""}; // often the port
      uint32_t concurrency{1}; // how many threads to use

      struct glaze
      {
         using T = asio_client;
         static constexpr auto value = glz::object(&T::host, &T::service, &T::concurrency);
      };

      std::shared_ptr<asio::io_context> ctx{};
      std::shared_ptr<asio::ip::tcp::socket> socket{};

      std::shared_ptr<repe::buffer_pool> buffer_pool = std::make_shared<repe::buffer_pool>();

      [[nodiscard]] std::error_code init()
      {
         ctx = std::make_shared<asio::io_context>(concurrency);
         socket = std::make_shared<asio::ip::tcp::socket>(*ctx);
         asio::ip::tcp::resolver resolver{*ctx};
         const auto endpoint = resolver.resolve(host, service);
#if !defined(GLZ_USING_BOOST_ASIO)
         std::error_code ec{};
#else
         boost::system::error_code ec{};
#endif
         asio::connect(*socket, endpoint, ec);
         if (ec) {
            return ec;
         }
         return socket->set_option(asio::ip::tcp::no_delay(true), ec);
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

         send_buffer(*socket, buffer);
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

         send_buffer(*socket, buffer);
         receive_buffer(*socket, buffer);

         return repe::decode_response<Opts>(std::forward<Result>(result), buffer);
      }

      template <class Result = glz::raw_json>
      [[nodiscard]] glz::expected<Result, repe::error_t> get(repe::header&& header)
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

         send_buffer(*socket, buffer);
         receive_buffer(*socket, buffer);

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

         send_buffer(*socket, buffer);
         receive_buffer(*socket, buffer);

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
            header.empty = true;
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
            header.empty = false;
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
      [[deprecated("We use a buffer pool now, so this would cause allocations")]] [[nodiscard]] std::string call_raw(
         repe::header&& header, Params&& params, repe::error_t& error)
      {
         std::string buffer{};

         header.notify = false;
         const auto ec = repe::request<Opts>(std::move(header), std::forward<Params>(params), buffer);
         if (bool(ec)) [[unlikely]] {
            error = {repe::error_e::invalid_params, glz::format_error(ec, buffer)};
            return buffer;
         }

         send_buffer(*socket, buffer);
         receive_buffer(*socket, buffer);
         return buffer;
      }
   };

   template <opts Opts = opts{}>
   struct asio_server
   {
      uint16_t port{};
      uint32_t concurrency{1}; // how many threads to use

      struct glaze
      {
         using T = asio_server;
         static constexpr auto value = glz::object(&T::port, &T::concurrency);
      };

      std::shared_ptr<asio::io_context> ctx{};
      std::shared_ptr<asio::signal_set> signals{};

      repe::registry<Opts> registry{};

      void clear_registry() { registry.clear(); }

      template <const std::string_view& Root = repe::detail::empty_path, class T>
         requires(glz::detail::glaze_object_t<T> || glz::detail::reflectable<T>)
      void on(T& value)
      {
         registry.template on<Root>(value);
      }

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

      // stop the server
      void stop()
      {
         if (ctx) {
            ctx->stop();
         }
      }

      asio::awaitable<void> run_instance(asio::ip::tcp::socket socket)
      {
         socket.set_option(asio::ip::tcp::no_delay(true));
         std::string buffer{};

         try {
            while (true) {
               co_await co_receive_buffer(socket, buffer);
               auto response = registry.call(buffer);
               if (response) {
                  co_await co_send_buffer(socket, response->value());
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
