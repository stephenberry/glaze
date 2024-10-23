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
static_assert(false, "standalone or boost asio must be included to use glaze/ext/glaze_asio.hpp");
#endif

#include <cassert>
#include <coroutine>
#include <iostream>

#include "glaze/rpc/repe/registry.hpp"
#include "glaze/util/memory_pool.hpp"

namespace glz
{
#if defined(GLZ_USING_BOOST_ASIO)
   namespace asio = boost::asio;
#endif
   inline void send_buffer(asio::ip::tcp::socket& socket, const repe::message& msg)
   {
      if (msg.header.length == repe::no_length_provided) {
         throw std::runtime_error("No length provided in REPE header");
      }

      std::array<asio::const_buffer, 3> buffers{asio::buffer(&msg.header, sizeof(msg.header)), asio::buffer(msg.query),
                                                asio::buffer(msg.body)};

      asio::write(socket, buffers, asio::transfer_exactly(msg.header.length));
   }

   inline void receive_buffer(asio::ip::tcp::socket& socket, repe::message& msg)
   {
      asio::read(socket, asio::buffer(&msg.header, sizeof(msg.header)), asio::transfer_exactly(sizeof(msg.header)));
      if (msg.header.query_length == repe::no_length_provided) {
         throw std::runtime_error("No query_length provided in REPE header");
      }
      msg.query.resize(msg.header.query_length);
      asio::read(socket, asio::buffer(msg.query), asio::transfer_exactly(msg.header.query_length));
      if (msg.header.body_length == repe::no_length_provided) {
         throw std::runtime_error("No body_length provided in REPE header");
      }
      msg.body.resize(msg.header.body_length);
      asio::read(socket, asio::buffer(msg.body), asio::transfer_exactly(msg.header.body_length));
   }

   inline asio::awaitable<void> co_send_buffer(asio::ip::tcp::socket& socket, const repe::message& msg)
   {
      if (msg.header.length == repe::no_length_provided) {
         throw std::runtime_error("No length provided in REPE header");
      }
      if (msg.header.query_length != msg.query.size()) {
         throw std::runtime_error("Query length mismatch in REPE header");
      }
      if (msg.header.body_length != msg.body.size()) {
         throw std::runtime_error("Body length mismatch in REPE header");
      }

      // Prepare the buffers: header, query, and body
      std::array<asio::const_buffer, 3> buffers{asio::buffer(&msg.header, sizeof(msg.header)), asio::buffer(msg.query),
                                                asio::buffer(msg.body)};

      // Calculate the total bytes to send
      std::size_t total_length = sizeof(msg.header) + msg.header.query_length + msg.header.body_length;

      // Asynchronously write the buffers to the socket
      co_await asio::async_write(socket, buffers, asio::transfer_exactly(total_length), asio::use_awaitable);
   }

   inline asio::awaitable<void> co_receive_buffer(asio::ip::tcp::socket& socket, repe::message& msg)
   {
      // Asynchronously read the header
      co_await asio::async_read(socket, asio::buffer(&msg.header, sizeof(msg.header)),
                                asio::transfer_exactly(sizeof(msg.header)), asio::use_awaitable);

      // Validate and read the query
      if (msg.header.query_length == repe::no_length_provided) {
         throw std::runtime_error("No query_length provided in REPE header");
      }
      msg.query.resize(msg.header.query_length);
      co_await asio::async_read(socket, asio::buffer(msg.query), asio::transfer_exactly(msg.header.query_length),
                                asio::use_awaitable);

      // Validate and read the body
      if (msg.header.body_length == repe::no_length_provided) {
         throw std::runtime_error("No body_length provided in REPE header");
      }
      msg.body.resize(msg.header.body_length);
      co_await asio::async_read(socket, asio::buffer(msg.body), asio::transfer_exactly(msg.header.body_length),
                                asio::use_awaitable);
   }

   // TODO: Make this socket_pool behave more like the object_pool and return a shared_ptr<socket>
   struct socket_pool
   {
      std::string host{"localhost"}; // host name
      std::string service{""}; // often the port
      std::mutex mtx{};
      std::vector<std::shared_ptr<asio::ip::tcp::socket>> sockets{2};
      std::vector<size_t> available{0, 1}; // indices of available sockets

      std::shared_ptr<asio::io_context> ctx{};
      std::shared_ptr<std::atomic<bool>> is_connected = std::make_shared<std::atomic<bool>>(false);

      // provides a pointer to a socket and an index
      std::tuple<std::shared_ptr<asio::ip::tcp::socket>, size_t, std::error_code> get()
      {
         std::unique_lock lock{mtx};

         // reset all socket pointers if a connection failed
         if (not *is_connected) {
            for (auto& socket : sockets) {
               socket.reset();
            }
         }

         if (available.empty()) {
            const auto current_size = sockets.size();
            const auto new_size = sockets.size() * 2;
            sockets.resize(new_size);
            for (size_t i = current_size; i < new_size; ++i) {
               available.emplace_back(i);
            }
         }

         const auto index = available.back();
         available.pop_back();
         auto& socket = sockets[index];
#if !defined(GLZ_USING_BOOST_ASIO)
         std::error_code ec{};
#else
         boost::system::error_code ec{};
#endif
         if (socket) {
            return {socket, index, ec};
         }
         else {
            socket = std::make_shared<asio::ip::tcp::socket>(*ctx);
            asio::ip::tcp::resolver resolver{*ctx};
            const auto endpoint = resolver.resolve(host, service);
            asio::connect(*socket, endpoint, ec);
            if (ec) {
               return {nullptr, index, ec};
            }
            socket->set_option(asio::ip::tcp::no_delay(true), ec);
            if (ec) {
               return {nullptr, index, ec};
            }
            (*is_connected) = true;
            return {socket, index, ec};
         }
      }

      void free(const size_t index)
      {
         std::unique_lock lock{mtx};
         available.emplace_back(index);
      }
   };

   struct unique_socket
   {
      socket_pool* pool{};
      std::shared_ptr<asio::ip::tcp::socket> ptr{};
      size_t index{};
      std::error_code ec{};

      std::shared_ptr<asio::ip::tcp::socket> value() { return ptr; }

      const std::shared_ptr<asio::ip::tcp::socket> value() const { return ptr; }

      asio::ip::tcp::socket& operator*() { return *ptr; }

      const asio::ip::tcp::socket& operator*() const { return *ptr; }

      unique_socket(socket_pool* input_pool) : pool(input_pool) { std::tie(ptr, index, ec) = pool->get(); }
      unique_socket(const unique_socket&) = default;
      unique_socket(unique_socket&&) = default;
      unique_socket& operator=(const unique_socket&) = default;
      unique_socket& operator=(unique_socket&&) = default;

      ~unique_socket() { pool->free(index); }
   };

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
      std::shared_ptr<glz::socket_pool> socket_pool = std::make_shared<glz::socket_pool>();
      std::shared_ptr<glz::memory_pool<repe::message>> message_pool =
         std::make_shared<glz::memory_pool<repe::message>>();

      std::shared_ptr<std::atomic<bool>> is_connected =
         std::make_shared<std::atomic<bool>>(false); // will be set to pool's boolean

      bool connected() const { return *is_connected; }

      [[nodiscard]] std::error_code init()
      {
         ctx = std::make_shared<asio::io_context>(concurrency);
         socket_pool->ctx = ctx;
         socket_pool->host = host;
         socket_pool->service = service;
         is_connected = socket_pool->is_connected;

         unique_socket socket{socket_pool.get()};
         if (socket.value()) {
            return {}; // connection success
         }
         else {
            return socket.ec; // connection failure
         }
      }

      template <class Params>
      [[nodiscard]] repe::error_t notify(repe::user_header&& header, Params&& params)
      {
         auto request = repe::request<Opts>(std::move(header), std::forward<Params>(params));
         request.header.notify(true);

         unique_socket socket{socket_pool.get()};

         try {
            send_buffer(*socket, request);
         }
         catch (const std::exception& e) {
            socket.ptr.reset();
            (*is_connected) = false;
            return {error_code::send_error, "asio send failure"};
         }

         return {};
      }

      template <class Result>
      [[nodiscard]] repe::error_t get(repe::user_header&& header, Result&& result)
      {
         auto request = repe::request<Opts>(std::move(header), nullptr);
         request.header.notify(false);
         request.header.read(true);

         unique_socket socket{socket_pool.get()};

         try {
            send_buffer(*socket, request);
         }
         catch (const std::exception& e) {
            socket.ptr.reset();
            (*is_connected) = false;
            return {error_code::send_error, "asio send failure"};
         }

         repe::message response{};

         try {
            receive_buffer(*socket, response);
         }
         catch (const std::exception& e) {
            socket.ptr.reset();
            (*is_connected) = false;
            return {error_code::send_error, "asio receive failure"};
         }

         return repe::decode_response<Opts>(std::forward<Result>(result), response);
      }

      template <class Result = glz::raw_json>
      [[nodiscard]] glz::expected<Result, repe::error_t> get(repe::user_header&& header)
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
      [[nodiscard]] repe::error_t set(repe::user_header&& header, Params&& params)
      {
         auto request = repe::request<Opts>(std::move(header), std::forward<Params>(params));

         unique_socket socket{socket_pool.get()};

         try {
            send_buffer(*socket, request);
         }
         catch (const std::exception& e) {
            socket.ptr.reset();
            (*is_connected) = false;
            return {error_code::send_error, "asio send failure"};
         }

         repe::message response{};
         try {
            receive_buffer(*socket, response);
         }
         catch (const std::exception& e) {
            socket.ptr.reset();
            (*is_connected) = false;
            return {error_code::send_error, "asio receive failure"};
         }

         return repe::decode_response<Opts>(response);
      }

      template <class Params, class Result>
      [[nodiscard]] repe::error_t call(repe::user_header&& header, Params&& params, Result&& result)
      {
         auto request = message_pool->borrow();
         std::ignore = repe::request<Opts>(*request, std::move(header), std::forward<Params>(params));

         unique_socket socket{socket_pool.get()};

         try {
            send_buffer(*socket, *request);
         }
         catch (const std::exception& e) {
            socket.ptr.reset();
            (*is_connected) = false;
            return {error_code::send_error, "asio send failure"};
         }

         auto response = message_pool->borrow();
         try {
            receive_buffer(*socket, *response);
         }
         catch (const std::exception& e) {
            socket.ptr.reset();
            (*is_connected) = false;
            return {error_code::send_error, "asio receive failure"};
         }

         return repe::decode_response<Opts>(std::forward<Result>(result), response->body);
      }

      [[nodiscard]] repe::error_t call(repe::user_header&& header)
      {
         auto request = repe::request<Opts>(std::move(header));

         unique_socket socket{socket_pool.get()};

         try {
            send_buffer(*socket, request);
         }
         catch (const std::exception& e) {
            socket.ptr.reset();
            (*is_connected) = false;
            return {error_code::send_error, "asio send failure"};
         }

         repe::message response{};
         try {
            receive_buffer(*socket, response);
         }
         catch (const std::exception& e) {
            socket.ptr.reset();
            (*is_connected) = false;
            return {error_code::send_error, "asio receive failure"};
         }

         return repe::decode_response<Opts>(response);
      }
   };

   template <opts Opts = opts{}>
   struct asio_server
   {
      uint16_t port{};
      uint32_t concurrency{1}; // how many threads to use

      ~asio_server() { stop(); }

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

      std::atomic<bool> stop_server{false}; // Atomic flag to control stopping the server

      void run()
      {
         if (!initialized) {
            init();
         }

         // Setup signal handling to stop the server
         signals->async_wait([&](auto, auto) { ctx->stop(); });

         // Start the listener coroutine
         asio::co_spawn(*ctx, listener(), asio::detached);

         stop_server = false;

         std::thread stop_thread([this]() {
            // Wait for stop_server to be set to true
            while (!stop_server) {
               std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            ctx->stop(); // Stop the server's io_context
         });

         // Run the io_context in multiple threads for concurrency
         std::vector<std::thread> threads;
         for (uint32_t i = 0; i < concurrency; ++i) {
            threads.emplace_back([this]() { ctx->run(); });
         }

         // Run in the main thread as well
         ctx->run();

         // Join all threads before exiting
         for (auto& thread : threads) {
            if (thread.joinable()) {
               thread.join();
            }
         }

         if (stop_thread.joinable()) {
            stop_thread.join();
         }
      }

      // stop the server
      void stop() { stop_server = true; }

      asio::awaitable<void> run_instance(asio::ip::tcp::socket socket)
      {
         socket.set_option(asio::ip::tcp::no_delay(true));
         repe::message request{};

         try {
            while (true) {
               co_await co_receive_buffer(socket, request);
               repe::message response{};
               registry.call(request, response);
               co_await co_send_buffer(socket, response);
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
