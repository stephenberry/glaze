// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if defined(_WIN32) && !defined(_WIN32_WINNT) && !defined(_WIN32_WINDOWS)
// ASIO requires a Windows target macro; if missing, it warns and assumes Windows 7.
// Set that same default explicitly (0x0601 = Windows 7) only when the build has not
// already provided _WIN32_WINNT/_WIN32_WINDOWS, so project-defined values still win.
#define _WIN32_WINNT 0x0601
#endif

#if __has_include(<asio.hpp>) && !defined(GLZ_USE_BOOST_ASIO)
#include <asio.hpp>
#include <asio/signal_set.hpp>
#ifdef GLZ_ENABLE_SSL
#include <asio/ssl.hpp>
#endif
#elif __has_include(<boost/asio.hpp>)
#ifndef GLZ_USING_BOOST_ASIO
#define GLZ_USING_BOOST_ASIO
#endif
#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#ifdef GLZ_ENABLE_SSL
#include <boost/asio/ssl.hpp>
#endif
#else
static_assert(false, "standalone or boost asio must be included to use glaze/ext/glaze_asio.hpp");
#endif

#include <atomic>
#include <algorithm>
#include <cassert>
#include <coroutine>
#include <iostream>
#include <optional>
#include <span>
#include <utility>

#include "glaze/rpc/registry.hpp"
#include "glaze/rpc/repe/buffer.hpp"
#include "glaze/util/buffer_pool.hpp"
#include "glaze/util/memory_pool.hpp"

namespace glz
{
#if defined(GLZ_ASIO_REPE_TRACE)
   inline void asio_repe_trace(const char* message)
   {
      std::fprintf(stderr, "[glz::asio_repe] %s\n", message);
      std::fflush(stderr);
   }
#else
   inline void asio_repe_trace(const char*) {}
#endif

#if defined(GLZ_USING_BOOST_ASIO)
   namespace asio
   {
      using namespace boost::asio;
      using error_code = boost::system::error_code;
      using system_error = boost::system::system_error;
   }
#endif
   inline void send_buffer(asio::ip::tcp::socket& socket, repe::message& msg)
   {
      if (msg.header.length == 0) {
         repe::encode_error(error_code::invalid_header, msg);
         return;
      }

      std::array<asio::const_buffer, 3> buffers{asio::buffer(&msg.header, sizeof(msg.header)), asio::buffer(msg.query),
                                                asio::buffer(msg.body)};

      asio::error_code e{};
      asio::write(socket, buffers, asio::transfer_exactly(msg.header.length), e);
      if (e) {
         repe::encode_error(error_code::connection_failure, msg, e.message());
         return;
      }
   }

   inline void receive_buffer(asio::ip::tcp::socket& socket, repe::message& msg)
   {
      asio::error_code e{};
      asio::read(socket, asio::buffer(&msg.header, sizeof(msg.header)), asio::transfer_exactly(sizeof(msg.header)), e);
      if (e) {
         repe::encode_error(error_code::connection_failure, msg, e.message());
         return;
      }

      // Validate REPE spec magic bytes
      if (msg.header.spec != repe::repe_magic) {
         repe::encode_error(error_code::invalid_header, msg, "Invalid REPE spec magic bytes");
         return;
      }

      // Validate version
      if (msg.header.version != 1) {
         repe::encode_error(error_code::version_mismatch, msg, "Unsupported REPE version");
         return;
      }

      msg.query.resize(msg.header.query_length);
      asio::read(socket, asio::buffer(msg.query), asio::transfer_exactly(msg.header.query_length));
      msg.body.resize(msg.header.body_length);
      asio::read(socket, asio::buffer(msg.body), asio::transfer_exactly(msg.header.body_length), e);
      if (e) {
         repe::encode_error(error_code::connection_failure, msg, e.message());
         return;
      }
   }

   inline asio::awaitable<void> co_send_buffer(asio::ip::tcp::socket& socket, const repe::message& msg)
   {
      if (msg.header.length == 0) {
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

      // Validate REPE spec magic bytes
      if (msg.header.spec != repe::repe_magic) {
         throw std::runtime_error("Invalid REPE spec magic bytes");
      }

      // Validate version
      if (msg.header.version != 1) {
         throw std::runtime_error("Unsupported REPE version");
      }

      // Validate and read the query
      msg.query.resize(msg.header.query_length);
      co_await asio::async_read(socket, asio::buffer(msg.query), asio::transfer_exactly(msg.header.query_length),
                                asio::use_awaitable);

      // Validate and read the body
      msg.body.resize(msg.header.body_length);
      co_await asio::async_read(socket, asio::buffer(msg.body), asio::transfer_exactly(msg.header.body_length),
                                asio::use_awaitable);
   }

   /// Receive a complete REPE message into a contiguous buffer (zero-copy optimized)
   /// Buffer layout: [header (48 bytes)][query][body]
   /// @param socket The socket to read from
   /// @param buffer Output buffer (will be resized to fit message)
   /// @param max_message_size Maximum allowed message size (prevents DoS) - 0 for unlimited
   inline asio::awaitable<void> co_receive_raw(asio::ip::tcp::socket& socket, std::string& buffer,
                                               size_t max_message_size = 64 * 1024 * 1024)
   {
      // Read just the length field (first 8 bytes of header)
      uint64_t length{};
      co_await asio::async_read(socket, asio::buffer(&length, sizeof(length)), asio::transfer_exactly(sizeof(length)),
                                asio::use_awaitable);

      // SECURITY: Validate length before allocation to prevent DoS
      if (length < sizeof(repe::header)) {
         throw std::runtime_error("Invalid REPE message: length too small for header");
      }
      if (max_message_size > 0 && length > max_message_size) {
         throw std::runtime_error("REPE message exceeds maximum allowed size");
      }

      // Resize buffer for complete message
      buffer.resize(length);

      // Copy the 8 bytes we already read into the buffer
      std::memcpy(buffer.data(), &length, sizeof(length));

      // Read the rest of the message directly into buffer
      const size_t remaining = length - sizeof(length);
      if (remaining > 0) {
         co_await asio::async_read(socket, asio::buffer(buffer.data() + sizeof(length), remaining),
                                   asio::transfer_exactly(remaining), asio::use_awaitable);
      }

      // Validate header fields using safe memcpy (avoids strict aliasing issues)
      uint16_t spec{};
      uint8_t version{};
      std::memcpy(&spec, buffer.data() + offsetof(repe::header, spec), sizeof(spec));
      std::memcpy(&version, buffer.data() + offsetof(repe::header, version), sizeof(version));

      if (spec != repe::repe_magic) {
         throw std::runtime_error("Invalid REPE magic bytes");
      }
      if (version != 1) {
         throw std::runtime_error("Unsupported REPE version");
      }
   }

   /// Send raw bytes directly without any transformation (zero-copy)
   /// @param socket The socket to write to
   /// @param data Span pointing to data (can be any memory: plugin buffer, owned buffer, etc.)
   inline asio::awaitable<void> co_send_raw(asio::ip::tcp::socket& socket, std::span<const char> data)
   {
      if (data.empty()) {
         co_return;
      }
      co_await asio::async_write(socket, asio::buffer(data.data(), data.size()), asio::transfer_exactly(data.size()),
                                 asio::use_awaitable);
   }

   // TODO: Make this socket_pool behave more like the object_pool and return a shared_ptr<socket>
   struct socket_pool
   {
      std::string host{"localhost"}; // host name
      std::string service{""}; // often the port
      std::mutex mtx{};
      std::shared_ptr<asio::io_context> ctx{};
      std::vector<std::shared_ptr<asio::ip::tcp::socket>> sockets{2};
      std::vector<size_t> available{0, 1}; // indices of available sockets
      std::shared_ptr<std::atomic<bool>> is_connected = std::make_shared<std::atomic<bool>>(false);

      ~socket_pool()
      {
         std::unique_lock lock{mtx};
         for (auto& socket : sockets) {
            if (!socket) continue;
            asio::error_code ec{};
            socket->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket->close(ec);
            socket.reset();
         }
         available.clear();
      }

      // provides a pointer to a socket and an index
      std::tuple<std::shared_ptr<asio::ip::tcp::socket>, size_t, std::error_code> get()
      {
         std::unique_lock lock{mtx};

         if (not ctx) {
            // TODO: make this error into an error code
            throw std::runtime_error("asio::io_context is null");
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
         // Check if socket exists and is still open
         if (socket && socket->is_open()) {
            return {socket, index, ec};
         }

         // Socket is null or closed, create a new one
         socket = std::make_shared<asio::ip::tcp::socket>(*ctx);
         asio::ip::tcp::resolver resolver{*ctx};
         const auto endpoint = resolver.resolve(host, service, ec);
         if (ec) {
            return {nullptr, index, ec};
         }
         asio::connect(*socket, endpoint, ec);
         if (ec) {
            return {nullptr, index, ec};
         }
         socket->set_option(asio::ip::tcp::no_delay(true), ec);
         if (ec) {
            return {nullptr, index, ec};
         }
         socket->set_option(asio::socket_base::keep_alive(true), ec);
         if (ec) {
            return {nullptr, index, ec};
         }
         (*is_connected) = true;
         return {socket, index, ec};
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
#if !defined(GLZ_USING_BOOST_ASIO)
      std::error_code ec{};
#else
      boost::system::error_code ec{};
#endif

      std::shared_ptr<asio::ip::tcp::socket> value() { return ptr; }

      const std::shared_ptr<asio::ip::tcp::socket> value() const { return ptr; }

      operator bool() const { return bool(ptr) && bool(pool) && not bool(ec); }

      asio::ip::tcp::socket& operator*() { return *ptr; }

      const asio::ip::tcp::socket& operator*() const { return *ptr; }

      unique_socket(socket_pool* input_pool) : pool(input_pool) { std::tie(ptr, index, ec) = pool->get(); }

      unique_socket(const unique_socket&) = delete;
      unique_socket& operator=(const unique_socket&) = delete;

      unique_socket(unique_socket&& other) noexcept
         : pool(std::exchange(other.pool, nullptr)), ptr(std::move(other.ptr)), index(other.index), ec(other.ec)
      {}

      unique_socket& operator=(unique_socket&& other) noexcept
      {
         if (this != &other) {
            if (pool) pool->free(index);
            pool = std::exchange(other.pool, nullptr);
            ptr = std::move(other.ptr);
            index = other.index;
            ec = other.ec;
         }
         return *this;
      }

      ~unique_socket()
      {
         if (pool) pool->free(index);
      }
   };

   template <auto Opts = opts{}>
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
      std::shared_ptr<glz::socket_pool> socket_pool{};
      std::shared_ptr<glz::memory_pool<repe::message>> message_pool =
         std::make_shared<glz::memory_pool<repe::message>>();

      std::shared_ptr<std::atomic<bool>> is_connected =
         std::make_shared<std::atomic<bool>>(false); // will be set to pool's boolean

      bool connected() const { return *is_connected; }

      [[nodiscard]] error_code init()
      {
         *is_connected = false;
         // create a new socket_pool if we are initilaizing, this is needed because the sockets hold references to the
         // io_context which is being recreated with init()
         socket_pool = std::make_shared<glz::socket_pool>();

         {
            std::unique_lock lock{socket_pool->mtx}; // lock the socket_pool when setting up
            ctx = std::make_shared<asio::io_context>(concurrency);
            socket_pool->ctx = ctx;
            socket_pool->host = host;
            socket_pool->service = service;
            is_connected = socket_pool->is_connected;
         }

         unique_socket socket{socket_pool.get()};
         if (socket) {
            return {}; // connection success
         }
         else {
            return error_code::connection_failure;
         }
      }

      template <class Header = repe::user_header, class... Params>
      void call(Header&& header, repe::message& response, Params&&... params)
      {
         auto request = message_pool->borrow();
         request->body.clear();
         if (not connected()) {
            repe::encode_error(error_code::connection_failure, response, "call failure: NOT CONNECTED");
            return;
         }
         repe::request<Opts>(std::move(header), *request, std::forward<Params>(params)...);
         if (bool(request->error())) {
            repe::encode_error(request->error(), response, "bad request");
            return;
         }

         unique_socket socket{socket_pool.get()};
         if (not socket) {
            socket.ptr.reset();
            (*is_connected) = false;
            repe::encode_error(error_code::send_error, response, "socket failure");
            return;
         }

         send_buffer(*socket, *request);

         if (bool(request->error())) {
            if (request->error() == error_code::connection_failure) {
               socket.ptr.reset();
               (*is_connected) = false;
            }
            repe::encode_error(request->error(), response, "send failure");
            return;
         }

         if (not header.notify) {
            receive_buffer(*socket, response);
            if (bool(response.error())) {
               if (response.error() == error_code::connection_failure) {
                  socket.ptr.reset();
                  (*is_connected) = false;
               }
               return;
            }
         }
      }

      // The `call` method above is designed to be generic and fully-featured.
      // The `call` method handles invoking RPC functions and sending data to query endpoints
      // as well as receiving data back.
      // The API is designed to reduce copies and gives the user control of how the response is handled.
      // This is important for chaining calls and efficiently handling memory.
      // ---
      // The `send` and `receive` methods provide simpler APIs that throw for cleaner code
      // and allocate the response in the function call.

      // Send parameters to a target query function or value
      template <class... Params>
      void set(const std::string_view query, Params&&... params)
      {
         if (not connected()) {
            throw std::runtime_error("asio_client: NOT CONNECTED");
         }

         repe::message response{};
         auto request = message_pool->borrow();
         request->body.clear();
         repe::request<Opts>(repe::user_header{.query = query}, *request, std::forward<Params>(params)...);
         if (bool(request->error())) {
            throw std::runtime_error("bad request");
         }

         unique_socket socket{socket_pool.get()};
         if (not socket) {
            socket.ptr.reset();
            (*is_connected) = false;
            throw std::runtime_error("socket failure");
         }

         send_buffer(*socket, *request);

         if (bool(request->error())) {
            if (request->error() == error_code::connection_failure) {
               socket.ptr.reset();
               (*is_connected) = false;
            }
            throw std::runtime_error(glz::format_error(request->error()));
         }

         receive_buffer(*socket, response);
         if (bool(response.error())) {
            if (response.error() == error_code::connection_failure) {
               socket.ptr.reset();
               (*is_connected) = false;
            }
            std::string error_message = glz::format_error(response.error());
            if (response.body.size()) {
               error_message.append(": ");
               error_message.append(response.body);
            }
            throw std::runtime_error(error_message);
         }
      }

      template <class Output>
      void get(const std::string_view query, Output& output)
      {
         if (not connected()) {
            throw std::runtime_error("asio_client: NOT CONNECTED");
         }

         repe::message response{};
         auto request = message_pool->borrow();
         request->body.clear();
         repe::request<Opts>(repe::user_header{.query = query}, *request);
         if (bool(request->error())) {
            throw std::runtime_error("bad request");
         }

         unique_socket socket{socket_pool.get()};
         if (not socket) {
            socket.ptr.reset();
            (*is_connected) = false;
            throw std::runtime_error("socket failure");
         }

         send_buffer(*socket, *request);

         if (bool(request->error())) {
            if (request->error() == error_code::connection_failure) {
               socket.ptr.reset();
               (*is_connected) = false;
            }
            throw std::runtime_error(glz::format_error(request->error()));
         }

         receive_buffer(*socket, response);
         if (bool(response.error())) {
            if (response.error() == error_code::connection_failure) {
               socket.ptr.reset();
               (*is_connected) = false;
            }
            std::string error_message = glz::format_error(response.error());
            if (response.body.size()) {
               error_message.append(": ");
               error_message.append(response.body);
            }
            throw std::runtime_error(error_message);
         }

         auto ec = read<Opts>(output, response.body);
         if (bool(ec)) {
            throw std::runtime_error(glz::format_error(ec, response.body));
         }
      }

      // Allocating version of `get`
      template <class Output>
      auto get(const std::string_view query)
      {
         Output out{};
         get(query, out);
         return out;
      }

      template <class Input, class Output>
      void inout(const std::string_view query, Input&& input, Output& output)
      {
         if (not connected()) {
            throw std::runtime_error("asio_client: NOT CONNECTED");
         }

         repe::message response{};
         auto request = message_pool->borrow();
         request->body.clear();
         repe::request<Opts>(repe::user_header{.query = query}, *request, std::forward<Input>(input));
         if (bool(request->error())) {
            throw std::runtime_error("bad request");
         }

         unique_socket socket{socket_pool.get()};
         if (not socket) {
            socket.ptr.reset();
            (*is_connected) = false;
            throw std::runtime_error("socket failure");
         }

         send_buffer(*socket, *request);

         if (bool(request->error())) {
            if (request->error() == error_code::connection_failure) {
               socket.ptr.reset();
               (*is_connected) = false;
            }
            throw std::runtime_error(glz::format_error(request->error()));
         }

         receive_buffer(*socket, response);
         if (bool(response.error())) {
            if (response.error() == error_code::connection_failure) {
               socket.ptr.reset();
               (*is_connected) = false;
            }
            std::string error_message = glz::format_error(response.error());
            if (response.body.size()) {
               error_message.append(": ");
               error_message.append(response.body);
            }
            throw std::runtime_error(error_message);
         }

         auto ec = read<Opts>(output, response.body);
         if (bool(ec)) {
            throw std::runtime_error(glz::format_error(ec, response.body));
         }
      }
   };

   template <auto Opts = opts{}>
   struct asio_server
   {
      uint16_t port{}; // 0 will select a random free port
      uint32_t concurrency{1}; // How many threads to use (a call to .run() is inclusive on the main thread)

      /// Maximum message size (default 64MB)
      /// Set to 0 for unlimited (not recommended for untrusted clients)
      size_t max_message_size = 64 * 1024 * 1024;

      /// Buffer pool for coroutine-safe buffer management
      /// Buffers are borrowed by each connection and automatically returned
      buffer_pool pool{};

      // Register a callback that takes a string error message on server/registry errors.
      // Note that we use a std::string to support a wide source of errors and use e.what()
      // IMPORTANT: The code within the callback must be thread safe, as multiple threads could call this
      // simultaneously.
      std::function<void(const std::string&)> error_handler{};

      // Custom call handler - works with raw buffers for zero-copy performance.
      //
      // The handler receives raw request bytes and a response buffer. Leave the buffer
      // empty for no response (e.g., notifications).
      //
      // @param request Raw REPE message bytes
      // @param response_buffer Buffer for building the response (will be resized, empty = no response)
      //
      // Example:
      //   server.call = [&](std::span<const char> request, std::string& response_buffer) {
      //      auto result = parse_request(request);
      //      if (!result) {
      //         encode_error_buffer(result.ec, response_buffer, "error");
      //         return;
      //      }
      //      // ... process request ...
      //      encode_response(response_buffer, data);
      //   };
      //
      // IMPORTANT: Must be set before calling run() and should not be modified during execution.
      std::function<void(std::span<const char> request, std::string& response_buffer)> call{};

      // Callback invoked after the server starts listening and is ready to accept connections.
      // Useful for synchronization in tests (e.g., with std::latch).
      std::function<void()> on_listen{};

      // Whether to set SO_REUSEADDR on the acceptor socket.
      // Allows rebinding to a port in TIME_WAIT state.
      bool reuse_address = false;

      ~asio_server()
      {
         stop();
         // Explicitly join threads before member destruction begins,
         // ensuring coroutines don't access destroyed members (like registry)
         threads.reset();
         work_guard.reset();
         listener_acceptor.reset();
         // Tear down Asio state before other members are destroyed.
         // This avoids pending operation/coroutine cleanup touching members
         // that are already being torn down on some runtimes.
         signals.reset();
         ctx.reset();
      }

      struct glaze
      {
         using T = asio_server;
         static constexpr auto value = glz::object(&T::port, &T::concurrency);
      };

      std::shared_ptr<asio::io_context> ctx{};
      std::shared_ptr<asio::signal_set> signals{};
      std::shared_ptr<std::vector<std::thread>> threads{};
      std::shared_ptr<asio::ip::tcp::acceptor> listener_acceptor{};
      using work_guard_t = decltype(asio::make_work_guard(std::declval<asio::io_context&>()));
      std::optional<work_guard_t> work_guard{};
      std::mutex active_sockets_mutex{};
      std::vector<std::shared_ptr<asio::ip::tcp::socket>> active_sockets{};

      glz::registry<Opts> registry{};

      void clear_registry() { registry.clear(); }

      template <const std::string_view& Root = detail::empty_path, class T>
         requires(glaze_object_t<T> || reflectable<T>)
      void on(T& value)
      {
         registry.template on<Root>(value);
      }

      bool initialized = false;
      std::atomic<bool> stop_requested{false};

      void add_active_socket(const std::shared_ptr<asio::ip::tcp::socket>& socket)
      {
         std::lock_guard lock{active_sockets_mutex};
         active_sockets.push_back(socket);
      }

      void remove_active_socket(const std::shared_ptr<asio::ip::tcp::socket>& socket)
      {
         std::lock_guard lock{active_sockets_mutex};
         std::erase_if(active_sockets, [&](const auto& current) { return current.get() == socket.get(); });
      }

      void init()
      {
         if (!initialized) {
            if (concurrency == 0) {
               throw std::runtime_error("concurrency == 0");
            }
            ctx = std::make_shared<asio::io_context>(concurrency);
#if !defined(_WIN32)
            signals = std::make_shared<asio::signal_set>(*ctx, SIGINT, SIGTERM);
#endif
         }
         initialized = true;
      }

      void run(bool run_on_main_thread = true)
      {
         asio_repe_trace("server.run begin");
         try {
            if (!initialized) {
               init();
            }

            stop_requested.store(false, std::memory_order_relaxed);
            work_guard.emplace(asio::make_work_guard(*ctx));

            // Setup signal handling to stop the server.
            // On Windows, avoid async signal_set wiring because it has shown unstable
            // teardown behavior in fast create/destroy test loops.
#if !defined(_WIN32)
            if (signals) {
               signals->async_wait([this](auto, auto) {
                  stop();
               });
            }
#endif

            // Create the acceptor synchronously so we know the actual port if set to 0 (select random free)
            auto executor = ctx->get_executor();
            listener_acceptor = std::make_shared<asio::ip::tcp::acceptor>(executor);
            auto& acceptor = *listener_acceptor;
            auto try_bind_loopback = [&](const asio::ip::tcp::endpoint& endpoint) {
               asio::error_code ec{};
               acceptor.close(ec);
               acceptor.open(endpoint.protocol(), ec);
               if (ec) {
                  return ec;
               }
               if (reuse_address) {
                  acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
                  if (ec) {
                     return ec;
                  }
               }
               acceptor.bind(endpoint, ec);
               if (ec) {
                  return ec;
               }
               acceptor.listen(asio::socket_base::max_listen_connections, ec);
               return ec;
            };

            // Prefer IPv4 loopback bind for test/process isolation and CI compatibility.
            // Clients in this test path connect to 127.0.0.1, so matching v4 avoids
            // environment-specific IPv6 dual-stack behavior.
            asio::error_code bind_ec{};
            bind_ec = try_bind_loopback({asio::ip::address_v4::loopback(), port});
            if (bind_ec) {
               throw asio::system_error(bind_ec);
            }
            port = acceptor.local_endpoint().port();
            asio_repe_trace("server.run listening");

            if (on_listen) {
               on_listen();
            }
            asio_repe_trace("server.run after on_listen");

            // Start the listener
#if defined(_WIN32)
            asio_repe_trace("server.run before listener async_accept");
            start_listener_windows();
            asio_repe_trace("server.run after listener async_accept");
#else
            auto local_error_handler = error_handler;
            auto report_spawn_exception = [local_error_handler](const char* where, std::exception_ptr ep) {
               if (!ep) {
                  return;
               }
               try {
                  std::rethrow_exception(ep);
               }
               catch (const std::exception& e) {
                  if (local_error_handler) {
                     local_error_handler(e.what());
                  }
                  else {
                     std::fprintf(stderr, "glz::asio_server %s exception: %s\n", where, e.what());
                  }
               }
               catch (...) {
                  if (local_error_handler) {
                     local_error_handler("unknown exception");
                  }
                  else {
                     std::fprintf(stderr, "glz::asio_server %s exception: unknown exception\n", where);
                  }
               }
            };
            asio_repe_trace("server.run before listener co_spawn");
            asio::co_spawn(*ctx, listener_impl(this, listener_acceptor),
                           [report_spawn_exception = std::move(report_spawn_exception)](std::exception_ptr ep) mutable {
                              report_spawn_exception("listener", std::move(ep));
                           });
            asio_repe_trace("server.run after listener co_spawn");
#endif

            // Run the io_context in multiple threads for concurrency
            asio_repe_trace("server.run before thread vector create");
            threads = std::shared_ptr<std::vector<std::thread>>(new std::vector<std::thread>{}, [](auto* ptr) {
               // Join all threads before exiting
               for (auto& thread : *ptr) {
                  if (thread.joinable()) {
                     thread.join();
                  }
               }
               delete ptr;
            });
            asio_repe_trace("server.run after thread vector create");

            threads->reserve(concurrency - uint32_t(run_on_main_thread));
            asio_repe_trace("server.run after reserve");
            for (uint32_t i = uint32_t(run_on_main_thread); i < concurrency; ++i) {
               asio_repe_trace("server.run before worker emplace");
               auto local_ctx = ctx;
               threads->emplace_back([local_ctx]() {
                  asio_repe_trace("server.worker begin");
                  try {
                     local_ctx->run();
                     asio_repe_trace("server.worker end");
                  }
                  catch (const std::exception& e) {
                     std::fprintf(stderr, "glz::asio_server worker exception: %s\n", e.what());
                  }
                  catch (...) {
                     std::fprintf(stderr, "glz::asio_server worker exception: unknown exception\n");
                  }
               });
               asio_repe_trace("server.run after worker emplace");
            }

            if (run_on_main_thread) {
               // Run in the main thread as well, which will block
               asio_repe_trace("server.run main thread run begin");
               ctx->run();
               asio_repe_trace("server.run main thread run end");

               threads.reset(); // join all threads
            }
            asio_repe_trace("server.run end");
         }
         catch (const std::exception& e) {
            stop_requested.store(true, std::memory_order_relaxed);
            if (work_guard) {
               work_guard->reset();
               work_guard.reset();
            }
            if (error_handler) {
               error_handler(e.what());
            }
            else {
               std::fprintf(stderr, "glz::asio_server run error: %s\n", e.what());
            }
         }
      }

      void run_async() { run(false); }

      // stop the server
      void stop()
      {
         asio_repe_trace("server.stop begin");
         stop_requested.store(true, std::memory_order_relaxed);

         std::vector<std::shared_ptr<asio::ip::tcp::socket>> sockets_to_close;
         {
            std::lock_guard lock{active_sockets_mutex};
            sockets_to_close = active_sockets;
         }
         for (auto& socket : sockets_to_close) {
            if (!socket) continue;
            asio::error_code ec{};
            socket->cancel(ec);
            socket->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket->close(ec);
         }

         if (listener_acceptor) {
            asio::error_code ec{};
            listener_acceptor->cancel(ec);
            listener_acceptor->close(ec);
         }
         if (work_guard) {
            work_guard->reset();
            work_guard.reset();
         }
#if !defined(_WIN32)
         if (signals) {
            asio::error_code ec{};
            signals->cancel(ec);
         }
#endif
         asio_repe_trace("server.stop end");
      }

#if defined(_WIN32)
      struct windows_session_state
      {
         asio_server* self{};
         std::shared_ptr<asio::ip::tcp::socket> socket{};
         std::string request{};
         std::string response{};
         uint64_t message_length{};
      };

      static void finish_windows_session(const std::shared_ptr<windows_session_state>& state)
      {
         if (!state || !state->self || !state->socket) {
            return;
         }
         asio::error_code ec{};
         state->socket->cancel(ec);
         state->socket->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
         state->socket->close(ec);
         state->self->remove_active_socket(state->socket);
      }

      static bool handle_windows_session_error(const std::shared_ptr<windows_session_state>& state, const asio::error_code& ec,
                                               const char* context)
      {
         if (!state || !state->self) {
            return true;
         }
         if (!ec) {
            return false;
         }

         if (ec == asio::error::eof) {
            finish_windows_session(state);
            return true;
         }

         const bool shutting_down = state->self->stop_requested.load(std::memory_order_relaxed);
         if (shutting_down &&
             (ec == asio::error::operation_aborted || ec == asio::error::bad_descriptor)) {
            asio_repe_trace("windows session canceled during shutdown");
            finish_windows_session(state);
            return true;
         }

         if (state->self->error_handler) {
            state->self->error_handler(std::string{context} + ": " + ec.message());
         }
         else {
            std::fprintf(stderr, "glz::asio_server %s: %s\n", context, ec.message().c_str());
         }
         finish_windows_session(state);
         return true;
      }

      static void process_windows_request(const std::shared_ptr<windows_session_state>& state);

      static void read_windows_message_body(const std::shared_ptr<windows_session_state>& state)
      {
         if (!state || !state->socket) {
            return;
         }

         const auto remaining = size_t(state->message_length - sizeof(state->message_length));
         if (!remaining) {
            process_windows_request(state);
            return;
         }

         asio::async_read(*state->socket, asio::buffer(state->request.data() + sizeof(state->message_length), remaining),
                          asio::transfer_exactly(remaining), [state](const asio::error_code& ec, size_t) {
                             if (handle_windows_session_error(state, ec, "read body")) {
                                return;
                             }
                             process_windows_request(state);
                          });
      }

      static void read_windows_message_length(const std::shared_ptr<windows_session_state>& state)
      {
         if (!state || !state->self || !state->socket) {
            return;
         }
         if (state->self->stop_requested.load(std::memory_order_relaxed)) {
            finish_windows_session(state);
            return;
         }

         state->request.clear();
         state->response.clear();
         state->message_length = 0;

         asio::async_read(*state->socket, asio::buffer(&state->message_length, sizeof(state->message_length)),
                          asio::transfer_exactly(sizeof(state->message_length)), [state](const asio::error_code& ec, size_t) {
                             if (handle_windows_session_error(state, ec, "read length")) {
                                return;
                             }

                             if (state->message_length < sizeof(repe::header)) {
                                if (state->self->error_handler) {
                                   state->self->error_handler("Invalid REPE message: length too small for header");
                                }
                                else {
                                   std::fprintf(stderr, "glz::asio_server error: Invalid REPE message: length too small for header\n");
                                }
                                finish_windows_session(state);
                                return;
                             }
                             if (state->self->max_message_size > 0 && state->message_length > state->self->max_message_size) {
                                if (state->self->error_handler) {
                                   state->self->error_handler("REPE message exceeds maximum allowed size");
                                }
                                else {
                                   std::fprintf(stderr, "glz::asio_server error: REPE message exceeds maximum allowed size\n");
                                }
                                finish_windows_session(state);
                                return;
                             }

                             state->request.resize(size_t(state->message_length));
                             std::memcpy(state->request.data(), &state->message_length, sizeof(state->message_length));
                             read_windows_message_body(state);
                          });
      }

      static void write_windows_response(const std::shared_ptr<windows_session_state>& state)
      {
         if (!state || !state->socket) {
            return;
         }
         if (state->response.empty()) {
            read_windows_message_length(state);
            return;
         }

         asio::async_write(*state->socket, asio::buffer(state->response.data(), state->response.size()),
                           asio::transfer_exactly(state->response.size()), [state](const asio::error_code& ec, size_t) {
                              if (handle_windows_session_error(state, ec, "write response")) {
                                 return;
                              }
                              read_windows_message_length(state);
                           });
      }

      static void process_windows_request(const std::shared_ptr<windows_session_state>& state)
      {
         if (!state || !state->self) {
            return;
         }

         uint16_t spec{};
         uint8_t version{};
         std::memcpy(&spec, state->request.data() + offsetof(repe::header, spec), sizeof(spec));
         std::memcpy(&version, state->request.data() + offsetof(repe::header, version), sizeof(version));

         if (spec != repe::repe_magic) {
            if (state->self->error_handler) {
               state->self->error_handler("Invalid REPE magic bytes");
            }
            else {
               std::fprintf(stderr, "glz::asio_server error: Invalid REPE magic bytes\n");
            }
            finish_windows_session(state);
            return;
         }
         if (version != 1) {
            if (state->self->error_handler) {
               state->self->error_handler("Unsupported REPE version");
            }
            else {
               std::fprintf(stderr, "glz::asio_server error: Unsupported REPE version\n");
            }
            finish_windows_session(state);
            return;
         }

         try {
            if (state->self->call) {
               state->self->call(std::span<const char>(state->request), state->response);
            }
            else {
               state->self->registry.call(std::span<const char>(state->request), state->response);
            }
         }
         catch (const std::exception& e) {
            if (state->self->error_handler) {
               state->self->error_handler(e.what());
            }
            auto id = repe::extract_id(state->request);
            repe::encode_error_buffer(error_code::invalid_call, state->response, e.what(), id);
         }
         catch (...) {
            if (state->self->error_handler) {
               state->self->error_handler("unknown error");
            }
            auto id = repe::extract_id(state->request);
            repe::encode_error_buffer(error_code::invalid_call, state->response, "unknown error", id);
         }

         write_windows_response(state);
      }

      void start_session_windows(const std::shared_ptr<asio::ip::tcp::socket>& socket_ptr)
      {
         if (!socket_ptr) {
            return;
         }

         asio::error_code ec{};
         socket_ptr->set_option(asio::ip::tcp::no_delay(true), ec);
         if (ec) {
            remove_active_socket(socket_ptr);
            return;
         }
         socket_ptr->set_option(asio::socket_base::keep_alive(true), ec);
         if (ec) {
            remove_active_socket(socket_ptr);
            return;
         }

         auto state = std::make_shared<windows_session_state>();
         state->self = this;
         state->socket = socket_ptr;
         read_windows_message_length(state);
      }

      void start_listener_windows()
      {
         auto acceptor = listener_acceptor;
         if (!acceptor) {
            return;
         }

         acceptor->async_accept([this, acceptor](const asio::error_code& ec, asio::ip::tcp::socket socket) {
            if (ec) {
               const bool shutting_down = stop_requested.load(std::memory_order_relaxed);
               if (shutting_down &&
                   (ec == asio::error::operation_aborted || ec == asio::error::bad_descriptor)) {
                  asio_repe_trace("listener canceled during shutdown");
                  return;
               }
               if (error_handler) {
                  error_handler(ec.message());
               }
               else {
                  std::fprintf(stderr, "glz::asio_server listener error: %s\n", ec.message().c_str());
               }
            }
            else {
               auto socket_ptr = std::make_shared<asio::ip::tcp::socket>(std::move(socket));
               asio_repe_trace("listener accepted socket");
               add_active_socket(socket_ptr);
               start_session_windows(socket_ptr);
            }

            if (!stop_requested.load(std::memory_order_relaxed) && acceptor->is_open()) {
               start_listener_windows();
            }
         });
      }
#endif

      static asio::awaitable<void> run_instance_impl(asio_server* self, std::shared_ptr<asio::ip::tcp::socket> socket_ptr)
      {
         asio_repe_trace("run_instance begin");
         if (!self || !socket_ptr) {
            asio_repe_trace("run_instance null socket");
            co_return;
         }
         auto& socket = *socket_ptr;
         bool socket_removed = false;
         auto remove_socket_once = [&]() {
            if (!socket_removed) {
               self->remove_active_socket(socket_ptr);
               socket_removed = true;
            }
         };

         asio::error_code ec{};
         socket.set_option(asio::ip::tcp::no_delay(true), ec);
         if (ec) {
            asio_repe_trace("run_instance no_delay failed");
            remove_socket_once();
            co_return;
         }
         socket.set_option(asio::socket_base::keep_alive(true), ec);
         if (ec) {
            asio_repe_trace("run_instance keep_alive failed");
            remove_socket_once();
            co_return;
         }

#if defined(_WIN32)
         // On Windows, keep per-connection buffers local to the coroutine frame.
         // This avoids relying on cross-object pool lifetimes during asynchronous teardown.
         std::string request_storage{};
         std::string response_storage{};
         auto& request = request_storage;
         auto& response = response_storage;
#else
         // Borrow buffers from pool - RAII ensures automatic return when coroutine ends.
         auto request_buf = self->pool.borrow();
         auto response_buf = self->pool.borrow();
         auto& request = request_buf.value();
         auto& response = response_buf.value();
#endif

         try {
            if (self->call) {
               // Custom call handler path with buffer pool
               while (true) {
                  co_await co_receive_raw(socket, request, self->max_message_size);

                  try {
                     self->call(std::span<const char>(request), response);
                  }
                  catch (const std::exception& e) {
                     if (self->error_handler) {
                        self->error_handler(e.what());
                     }
                     auto id = repe::extract_id(request);
                     repe::encode_error_buffer(error_code::invalid_call, response, e.what(), id);
                  }
                  catch (...) {
                     if (self->error_handler) {
                        self->error_handler("unknown error");
                     }
                     auto id = repe::extract_id(request);
                     repe::encode_error_buffer(error_code::invalid_call, response, "unknown error", id);
                  }

                  // Send response if buffer is not empty
                  if (!response.empty()) {
                     co_await co_send_raw(socket, response);
                  }
               }
            }
            else {
               // Standard registry path with span-based call
               while (true) {
                  co_await co_receive_raw(socket, request, self->max_message_size);

                  try {
                     self->registry.call(std::span<const char>(request), response);
                  }
                  catch (const std::exception& e) {
                     if (self->error_handler) {
                        self->error_handler(e.what());
                     }
                     auto id = repe::extract_id(request);
                     repe::encode_error_buffer(error_code::invalid_call, response, e.what(), id);
                  }
                  catch (...) {
                     if (self->error_handler) {
                        self->error_handler("unknown error");
                     }
                     auto id = repe::extract_id(request);
                     repe::encode_error_buffer(error_code::invalid_call, response, "unknown error", id);
                  }

                  // Send response if buffer is not empty
                  if (!response.empty()) {
                     co_await co_send_raw(socket, response);
                  }
               }
            }
         }
         catch (const asio::system_error& e) {
            // EOF indicates normal client disconnect, not an error
            if (e.code() == asio::error::eof) {
               remove_socket_once();
               co_return;
            }
            if (self->stop_requested.load(std::memory_order_relaxed) &&
                (e.code() == asio::error::operation_aborted || e.code() == asio::error::bad_descriptor)) {
               asio_repe_trace("run_instance canceled during shutdown");
               remove_socket_once();
               co_return;
            }
            if (self->error_handler) {
               self->error_handler(e.what());
            }
            else {
               std::fprintf(stderr, "glz::asio_server error: %s\n", e.what());
            }
         }
         catch (const std::exception& e) {
            if (self->error_handler) {
               self->error_handler(e.what());
            }
            else {
               std::fprintf(stderr, "glz::asio_server error: %s\n", e.what());
            }
         }
         remove_socket_once();
         asio_repe_trace("run_instance end");
         // Buffers automatically returned to pool when scoped_buffers are destroyed
      }

      static asio::awaitable<void> listener_impl(asio_server* self, std::shared_ptr<asio::ip::tcp::acceptor> acceptor)
      {
         asio_repe_trace("listener begin");
         if (!self || !acceptor) {
            asio_repe_trace("listener null acceptor");
            co_return;
         }
         auto executor = co_await asio::this_coro::executor;
         try {
            auto local_error_handler = self->error_handler;
            while (true) {
               auto socket = std::make_shared<asio::ip::tcp::socket>(co_await acceptor->async_accept(asio::use_awaitable));
               asio_repe_trace("listener accepted socket");
               self->add_active_socket(socket);
               asio::co_spawn(executor, run_instance_impl(self, socket), [local_error_handler](std::exception_ptr ep) {
                  if (!ep) {
                     return;
                  }
                  try {
                     std::rethrow_exception(ep);
                  }
                  catch (const std::exception& e) {
                     if (local_error_handler) {
                        local_error_handler(e.what());
                     }
                     else {
                        std::fprintf(stderr, "glz::asio_server run_instance exception: %s\n", e.what());
                     }
                  }
                  catch (...) {
                     if (local_error_handler) {
                        local_error_handler("unknown exception");
                     }
                     else {
                        std::fprintf(stderr, "glz::asio_server run_instance exception: unknown exception\n");
                     }
                  }
               });
            }
         }
         catch (const asio::system_error& e) {
            // operation_aborted/bad_descriptor are expected while shutting down.
            // If we are not shutting down, surface them through the normal error path.
            const bool shutting_down = self->stop_requested.load(std::memory_order_relaxed);
            if (shutting_down &&
                (e.code() == asio::error::operation_aborted || e.code() == asio::error::bad_descriptor)) {
               asio_repe_trace("listener canceled during shutdown");
               co_return;
            }
            if (self->error_handler) {
               self->error_handler(e.what());
            }
            else {
               std::fprintf(stderr, "glz::asio_server listener error: %s\n", e.what());
            }
         }
         catch (const std::exception& e) {
            if (self->error_handler) {
               self->error_handler(e.what());
            }
            else {
               std::fprintf(stderr, "glz::asio_server listener error: %s\n", e.what());
            }
         }
         asio_repe_trace("listener end");
      }
   };
}
