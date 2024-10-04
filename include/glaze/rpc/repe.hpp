// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <mutex>
#include <shared_mutex>
#include <thread>

#include "glaze/glaze.hpp"

namespace glz::repe
{
   // read/write are from the client's perspective
   enum struct action : uint32_t
   {
     notify = 1 << 0, // If this message does not require a response
     read = 1 << 1, // Read a value or return the result of invoking a function
     write = 1 << 2 // Write a value or invoke a function
   };
   
   inline constexpr auto no_length_provided = (std::numeric_limits<uint64_t>::max)();

   // C++ pseudocode representing layout
   struct header {
     uint64_t length{no_length_provided}; // Total length of [header, query, body]
     //
     uint16_t spec{0x1507}; // (5383) Magic two bytes to denote the REPE specification
     uint8_t version = 1; // REPE version
     bool error{}; // Whether an error has occurred
     repe::action action{}; // Action to take, multiple actions may be bit-packed together
     //
     uint64_t id{}; // Identifier
     //
     uint64_t query_length{no_length_provided}; // The total length of the query (-1 denotes no size given)
     //
     uint64_t body_length{no_length_provided}; // The total length of the body (-1 denotes no size given)
     //
     uint64_t reserved{}; // Must be set to zero
      
      bool notify() const {
         return bool(uint32_t(action) & uint32_t(repe::action::notify));
      }
      
      bool read() const {
         return bool(uint32_t(action) & uint32_t(repe::action::read));
      }
      
      bool write() const {
         return bool(uint32_t(action) & uint32_t(repe::action::write));
      }
      
      void read(bool enable) {
         if (enable) {
            action = static_cast<repe::action>(uint32_t(action) | uint32_t(action::read));
          } else {
             action = static_cast<repe::action>(uint32_t(action) & ~uint32_t(action::read));
          }
      }
      
      void write(bool enable) {
         if (enable) {
            action = static_cast<repe::action>(uint32_t(action) | uint32_t(action::write));
          } else {
             action = static_cast<repe::action>(uint32_t(action) & ~uint32_t(action::write));
          }
      }
   };
   
   static_assert(sizeof(header) == 48);
   
   struct message final
   {
      repe::header header{};
      std::string query{};
      std::string body{};
   };

   // User interface that will be encoded into a REPE header
   struct user_header final
   {
      std::string_view query = ""; // The JSON pointer path to call or member to access/assign
      uint64_t id{}; // Identifier
      bool error{}; // Whether an error has occurred

      repe::action action{}; // Action to take, multiple actions may be bit-packed together
      
      bool notify() const {
         return bool(uint32_t(action) & uint32_t(repe::action::notify));
      }
      
      bool read() const {
         return bool(uint32_t(action) & uint32_t(repe::action::read));
      }
      
      bool write() const {
         return bool(uint32_t(action) & uint32_t(repe::action::write));
      }
      
      void read(bool enable) {
         if (enable) {
            action = static_cast<repe::action>(uint32_t(action) | uint32_t(action::read));
          } else {
             action = static_cast<repe::action>(uint32_t(action) & ~uint32_t(action::read));
          }
      }
      
      void write(bool enable) {
         if (enable) {
            action = static_cast<repe::action>(uint32_t(action) | uint32_t(action::write));
          } else {
             action = static_cast<repe::action>(uint32_t(action) & ~uint32_t(action::write));
          }
      }
   };
   
   inline repe::header encode(const user_header& h) noexcept
   {
      repe::header ret{.error = h.error, //
            .action = h.action, //
            .id = h.id, //
            .query_length = h.query.size() //
      };
      return ret;
   }
}

namespace glz::repe
{
   enum struct error_e
   {
      ok,
      version_mismatch,
      invalid_header,
      invalid_query,
      invalid_body,
      parse_error,
      method_not_found,
      timeout
   };

   inline constexpr std::string_view error_code_to_sv(const error_e e) noexcept
   {
      using enum error_e;
      switch (e) {
      case ok: {
         return "0 [ok]";
      }
      case version_mismatch: {
         return "1 [version_mismatch]";
      }
      case invalid_header: {
         return "2 [invalid_header]";
      }
      case invalid_query: {
         return "3 [invalid_query]";
      }
      case invalid_body: {
         return "4 [invalid_body]";
      }
      case parse_error: {
         return "5 [parse_error]";
      }
      case method_not_found: {
         return "6 [method_not_found]";
      }
      case timeout: {
         return "7 [timeout]";
      }
      default: {
         return "unknown_error_code";
         break;
      }
      };
   }

   struct error_t final
   {
      error_e code = error_e::ok;
      std::string message = "";

      operator bool() const noexcept { return bool(code); }
   };

   inline std::string format_error(const error_t& e) noexcept
   {
      std::string result = "error: " + std::string(error_code_to_sv(e.code));
      result += "\n";
      result += e.message;
      return result;
   }

   struct state final
   {
      const std::string_view message{};
      repe::header& header;
      std::string& response;
      error_t& error;
   };

   template <class T>
   concept is_state = std::same_as<std::decay_t<T>, state>;

   namespace detail
   {
      struct string_hash
      {
         using is_transparent = void;
         [[nodiscard]] size_t operator()(const char* txt) const { return std::hash<std::string_view>{}(txt); }
         [[nodiscard]] size_t operator()(std::string_view txt) const { return std::hash<std::string_view>{}(txt); }
         [[nodiscard]] size_t operator()(const std::string& txt) const { return std::hash<std::string>{}(txt); }
      };
   }

   // returns 0 on error
   template <opts Opts, class Value>
   size_t read_params(Value&& value, auto&& state, auto&& response)
   {
      glz::context ctx{};
      auto [b, e] = read_iterators<Opts>(ctx, state.message);
      if (bool(ctx.error)) [[unlikely]] {
         return 0;
      }
      auto start = b;

      glz::detail::read<Opts.format>::template op<Opts>(std::forward<Value>(value), ctx, b, e);

      if (bool(ctx.error)) {
         error_ctx ec{ctx.error, ctx.custom_error_message, size_t(b - start), ctx.includer_error};
         std::ignore =
            write<Opts>(std::forward_as_tuple(header{.error = true},
                                              error_t{error_e::parse_error, format_error(ec, state.message)}),
                        response);
         return 0;
      }

      return size_t(b - start);
   }

   template <opts Opts, class Value>
   void write_response(Value&& value, is_state auto&& state)
   {
      if (state.error) {
         std::ignore = write<Opts>(std::forward_as_tuple(header{.error = true}, state.error), state.response);
      }
      else {
         const auto ec = write<Opts>(std::forward_as_tuple(state.header, std::forward<Value>(value)), state.response);
         if (bool(ec)) [[unlikely]] {
            std::ignore = write<Opts>(std::forward_as_tuple(header{.error = true}, ec.ec), state.response);
         }
      }
   }

   template <opts Opts>
   void write_response(is_state auto&& state)
   {
      if (state.error) {
         std::ignore = write<Opts>(std::forward_as_tuple(header{.error = true}, state.error), state.response);
      }
      else {
         const auto ec = write<Opts>(std::forward_as_tuple(state.header, nullptr), state.response);
         if (bool(ec)) [[unlikely]] {
            std::ignore = write<Opts>(std::forward_as_tuple(header{.error = true}, ec.ec), state.response);
         }
      }
   }

   struct ignore_result final
   {};

   template <opts Opts, class Result>
   [[nodiscard]] error_t decode_response(Result&& result, auto& buffer)
   {
      repe::header h;
      context ctx{};
      auto [b, e] = read_iterators<Opts>(ctx, buffer);
      if (bool(ctx.error)) [[unlikely]] {
         return error_t{error_e::parse_error};
      }
      auto start = b;

      auto handle_error = [&](auto& it) {
         ctx.error = error_code::syntax_error;
         error_ctx pe{ctx.error, ctx.custom_error_message, size_t(it - start), ctx.includer_error};
         return error_t{error_e::parse_error, format_error(pe, buffer)};
      };

      if (*b == '[') {
         ++b;
      }
      else {
         return handle_error(b);
      }

      glz::detail::read<Opts.format>::template op<Opts>(h, ctx, b, e);

      if (bool(ctx.error)) {
         error_ctx pe{ctx.error, ctx.custom_error_message, size_t(b - start), ctx.includer_error};
         return {error_e::parse_error, format_error(pe, buffer)};
      }

      if (*b == ',') {
         ++b;
      }
      else {
         return handle_error(b);
      }

      if (h.error) {
         error_t error{};
         glz::detail::read<Opts.format>::template op<Opts>(error, ctx, b, e);
         return error;
      }

      if constexpr (!std::same_as<std::decay_t<Result>, ignore_result>) {
         glz::detail::read<Opts.format>::template op<Opts>(result, ctx, b, e);

         if (bool(ctx.error)) {
            error_ctx pe{ctx.error, ctx.custom_error_message, size_t(b - start), ctx.includer_error};
            return {error_e::parse_error, format_error(pe, buffer)};
         }
      }

      return {};
   }

   template <opts Opts>
   [[nodiscard]] error_t decode_response(auto& buffer)
   {
      return decode_response<Opts>(ignore_result{}, buffer);
   }

   template <opts Opts, class Value, class H = header>
   [[nodiscard]] auto request(H&& h, Value&& value)
   {
      message msg{.header = encode(h), .query = std::string{h.query}};
      std::ignore = glz::write<Opts>(std::forward<Value>(value), msg.body);
      return msg;
   }

   template <class H = user_header>
   [[nodiscard]] auto request_binary(H&& h)
   {
      h.read(true); // because no value provided
      message msg{.header = encode(h), .query = std::string{h.query}};
      return msg;
   }

   template <class H = user_header>
   [[nodiscard]] auto request_json(H&& h)
   {
      h.read(true); // because no value provided
      message msg{.header = encode(h), .query = std::string{h.query}};
      return msg;
   }

   template <class Value, class H = user_header>
   [[nodiscard]] auto request_json(H&& h, Value&& value)
   {
      message msg{.header = encode(h), .query = std::string{h.query}};
      std::ignore = glz::write_json(std::forward<Value>(value), msg.body);
      return msg;
   }

   template <class Value, class H = user_header>
   [[nodiscard]] auto request_binary(H&& h, Value&& value)
   {
      message msg{.header = encode(h), .query = std::string{h.query}};
      std::ignore = glz::write_beve(std::forward<Value>(value), msg.body);
      return msg;
   }

   // DESIGN NOTE: It might appear that we are locking ourselves into a poor design choice by using a runtime
   // std::unordered_map. However, we can actually improve this in the future by allowing glz::meta specialization on
   // the server to list out the method names and allow us to build a compile time map prior to function registration.
   // This is made possible by constinit, and the same approach used for compile time maps and struct reflection.
   // This will then be an opt-in performance improvement where a bit more code must be written by the user to
   // list the method names.

   namespace detail
   {
      static constexpr std::string_view empty_path = "";
   }

   template <class T, const std::string_view& parent = detail::empty_path>
      requires(glz::detail::glaze_object_t<T> || glz::detail::reflectable<T>)
   constexpr auto make_mutex_map()
   {
      using Mtx = std::mutex*;
      using namespace glz::detail;
      constexpr auto N = reflect<T>::size;
      return [&]<size_t... I>(std::index_sequence<I...>) {
         return normal_map<sv, Mtx, N>(
            std::array<pair<sv, Mtx>, N>{pair<sv, Mtx>{join_v<parent, chars<"/">, key_name_v<I, T>>, Mtx{}}...});
      }(std::make_index_sequence<N>{});
   }

   struct buffer_pool
   {
      std::mutex mtx{};
      std::deque<std::string> buffers{2};
      std::vector<size_t> available{0, 1}; // indices of available buffers

      // provides a pointer to a buffer and an index
      std::tuple<std::string*, size_t> get()
      {
         std::unique_lock lock{mtx};
         if (available.empty()) {
            const auto current_size = buffers.size();
            const auto new_size = buffers.size() * 2;
            buffers.resize(new_size);
            for (size_t i = current_size; i < new_size; ++i) {
               available.emplace_back(i);
            }
         }

         const auto index = available.back();
         available.pop_back();
         return {&buffers[index], index};
      }

      void free(const size_t index)
      {
         std::unique_lock lock{mtx};
         available.emplace_back(index);
      }
   };

   struct unique_buffer
   {
      buffer_pool* pool{};
      std::string* ptr{};
      size_t index{};

      std::string& value() { return *ptr; }

      const std::string& value() const { return *ptr; }

      unique_buffer(buffer_pool* input_pool) : pool(input_pool) { std::tie(ptr, index) = pool->get(); }
      unique_buffer(const unique_buffer&) = default;
      unique_buffer(unique_buffer&&) = default;
      unique_buffer& operator=(const unique_buffer&) = default;
      unique_buffer& operator=(unique_buffer&&) = default;

      ~unique_buffer() { pool->free(index); }
   };

   using shared_buffer = std::shared_ptr<unique_buffer>;

   struct mutex_link
   {
      std::shared_mutex route{};
      std::shared_mutex endpoint{};
   };

   using mutex_chain = std::vector<mutex_link*>;

   [[nodiscard]] inline uint64_t& timeout_duration_ns()
   {
      static thread_local uint64_t timeout_ns = 1'000'000'000; // 1 second default
      return timeout_ns;
   }

   struct shared_mutex
   {
      shared_mutex(std::shared_mutex& mtx) : mtx_(mtx) {}
      void lock() { mtx_.lock_shared(); }
      void unlock() { mtx_.unlock_shared(); }
      bool try_lock() { return mtx_.try_lock_shared(); }

     private:
      std::shared_mutex& mtx_;
   };

   template <class Clock, class Duration, class L0, class L1>
   [[nodiscard]] inline bool try_lock_until(std::chrono::time_point<Clock, Duration> t, L0& l0, L1& l1)
   {
      while (true) {
         {
            std::unique_lock<L0> u0(l0);
            if (l1.try_lock()) {
               u0.release();
               break;
            }
         }
         std::this_thread::yield();
         {
            std::unique_lock<L1> u1(l1);
            if (l0.try_lock()) {
               u1.release();
               break;
            }
         }
         std::this_thread::yield();
         if (std::chrono::steady_clock::now() >= t) {
            return false;
         }
      }
      return true;
   }

   template <class L0, class L1>
   [[nodiscard]] inline bool try_lock_for(L0& l0, L1& l1)
   {
      return try_lock_until(std::chrono::steady_clock::now() + std::chrono::nanoseconds(timeout_duration_ns()), l0, l1);
   }

   [[nodiscard]] inline bool lock_shared(auto& mtx0, auto& mtx1)
   {
      shared_mutex wrapper0{mtx0};
      shared_mutex wrapper1{mtx1};
      return try_lock_for(wrapper0, wrapper1);
   }

   template <class Clock, class Duration, class L0>
   [[nodiscard]] inline bool try_lock_until(std::chrono::time_point<Clock, Duration> t, L0& l0)
   {
      while (true) {
         if (l0.try_lock()) {
            break;
         }
         std::this_thread::yield();
         if (std::chrono::steady_clock::now() >= t) {
            return false;
         }
      }
      return true;
   }

   template <class L0>
   [[nodiscard]] inline bool try_lock_for(L0& l0)
   {
      return try_lock_until(std::chrono::steady_clock::now() + std::chrono::nanoseconds(timeout_duration_ns()), l0);
   }

   // Any unique lock down the chain will block further access.
   // We perform a unique lock on any read/write
   // This means we cannot have asynchronous reads of the same value (address in the future).
   // But, this allows asynchronous reads/writes as long as they lead down different paths.

   namespace detail
   {
      // lock reading into a value (writing to C++ memory)
      [[nodiscard]] inline bool lock_read(mutex_chain& chain)
      {
         const auto n = chain.size();
         if (n == 0) {
            return true;
         }
         else if (n == 1) {
            return try_lock_for(chain[0]->route, chain[0]->endpoint);
         }
         else {
            for (size_t i = 0; i < (n - 1); ++i) {
               shared_mutex route{chain[i]->route};
               shared_mutex endpoint{chain[i]->endpoint};
               const auto valid = try_lock_for(route, endpoint);
               if (not valid) {
                  return false;
               }
            }
            return try_lock_for(chain.back()->route, chain.back()->endpoint);
         }
      }

      inline void unlock_read(mutex_chain& chain)
      {
         const auto n = chain.size();
         if (n == 0) {
            return;
         }
         else if (n == 1) {
            chain[0]->endpoint.unlock();
            chain[0]->route.unlock();
         }
         else {
            for (size_t i = 0; i < (n - 1); ++i) {
               chain[i]->endpoint.unlock_shared();
               chain[i]->route.unlock_shared();
            }
            chain.back()->endpoint.unlock();
            chain.back()->route.unlock();
         }
      }

      // lock writing out a value (reading from C++ memory)
      [[nodiscard]] inline bool lock_write(mutex_chain& chain)
      {
         const auto n = chain.size();
         if (n == 0) {
            return true;
         }
         else if (n == 1) {
            return try_lock_for(chain[0]->route, chain[0]->endpoint);
         }
         else {
            for (size_t i = 0; i < (n - 1); ++i) {
               shared_mutex route{chain[i]->route};
               shared_mutex endpoint{chain[i]->endpoint};
               const auto valid = try_lock_for(route, endpoint);
               if (not valid) {
                  return false;
               }
            }
            return try_lock_for(chain.back()->route, chain.back()->endpoint);
         }
      }

      inline void unlock_write(mutex_chain& chain)
      {
         const auto n = chain.size();
         if (n == 0) {
            return;
         }
         else if (n == 1) {
            chain[0]->endpoint.unlock();
            chain[0]->route.unlock();
         }
         else {
            for (size_t i = 0; i < (n - 1); ++i) {
               chain[i]->endpoint.unlock_shared();
               chain[i]->route.unlock_shared();
            }
            chain.back()->endpoint.unlock();
            chain.back()->route.unlock();
         }
      }

      // lock invoking a function, which locks same depth and lower (supports member functions that manipulate class
      // state) treated as writing to C++ memory at the function depth and its parent depth
      [[nodiscard]] inline bool lock_invoke(mutex_chain& chain)
      {
         const auto n = chain.size();
         if (n == 0) {
            return true;
         }
         else if (n == 1) {
            return try_lock_for(chain[0]->route, chain[0]->endpoint);
         }
         else {
            for (size_t i = 0; i < (n - 2); ++i) {
               shared_mutex route{chain[i]->route};
               shared_mutex endpoint{chain[i]->endpoint};
               const bool valid = try_lock_for(route, endpoint);
               if (not valid) {
                  return false;
               }
            }
            auto valid = try_lock_for(chain[n - 2]->route, chain[n - 2]->endpoint);
            if (not valid) {
               return false;
            }
            return try_lock_for(chain.back()->route, chain.back()->endpoint);
         }
      }

      inline void unlock_invoke(mutex_chain& chain)
      {
         const auto n = chain.size();
         if (n == 0) {
            return;
         }
         else if (n == 1) {
            chain[0]->endpoint.unlock();
            chain[0]->route.unlock();
         }
         else {
            for (size_t i = 0; i < (n - 2); ++i) {
               chain[i]->endpoint.unlock_shared();
               chain[i]->route.unlock_shared();
            }
            chain.back()->endpoint.unlock();
            chain.back()->route.unlock();
            chain[n - 2]->endpoint.unlock();
            chain[n - 2]->route.unlock();
         }
      }
   }

   struct chain_read_lock final
   {
      mutex_chain& chain;
      bool lock_aquired = false;

      chain_read_lock(mutex_chain& input) : chain(input) { lock_aquired = detail::lock_read(chain); }

      operator bool() const { return lock_aquired; }

      chain_read_lock(const chain_read_lock&) = delete;
      chain_read_lock(chain_read_lock&&) = default;
      chain_read_lock& operator=(const chain_read_lock&) = delete;
      chain_read_lock& operator=(chain_read_lock&&) = delete;

      ~chain_read_lock() { detail::unlock_read(chain); }
   };

   struct chain_write_lock final
   {
      mutex_chain& chain;
      bool lock_aquired = false;

      chain_write_lock(mutex_chain& input) : chain(input) { lock_aquired = detail::lock_write(chain); }

      operator bool() const { return lock_aquired; }

      chain_write_lock(const chain_write_lock&) = delete;
      chain_write_lock(chain_write_lock&&) = default;
      chain_write_lock& operator=(const chain_write_lock&) = delete;
      chain_write_lock& operator=(chain_write_lock&&) = delete;

      ~chain_write_lock() { detail::unlock_write(chain); }
   };

   struct chain_invoke_lock final
   {
      mutex_chain& chain;
      bool lock_aquired = false;

      chain_invoke_lock(mutex_chain& input) : chain(input) { lock_aquired = detail::lock_invoke(chain); }

      operator bool() const { return lock_aquired; }

      chain_invoke_lock(const chain_invoke_lock&) = delete;
      chain_invoke_lock(chain_invoke_lock&&) = default;
      chain_invoke_lock& operator=(const chain_invoke_lock&) = delete;
      chain_invoke_lock& operator=(chain_invoke_lock&&) = delete;

      ~chain_invoke_lock() { detail::unlock_invoke(chain); }
   };

   // This registry does not support adding methods from RPC calls or adding methods once RPC calls can be made.
   template <opts Opts = opts{}>
   struct registry
   {
      using procedure = std::function<void(state&&)>; // RPC method
      std::unordered_map<sv, procedure, detail::string_hash, std::equal_to<>> methods;

      void clear() { methods.clear(); }

      // TODO: replace this std::map with a std::flat_map with a std::deque (to not invalidate references)
      std::map<sv, mutex_link> mtxs; // only hashes during initialization

      mutex_chain get_chain(const sv json_ptr)
      {
         std::vector<sv> paths = glz::detail::json_ptr_children(json_ptr);
         mutex_chain v{};
         v.reserve(paths.size());
         for (auto& path : paths) {
            v.emplace_back(&mtxs[path]);
         }
         return v;
      }

      // used for writing to C++ memory
      template <string_literal json_ptr>
      auto lock()
      {
         // TODO: use a std::array and calculate number of path segments
         static thread_local mutex_chain chain = [&] {
            std::vector<sv> paths = glz::detail::json_ptr_children(json_ptr.sv());
            mutex_chain chain{};
            chain.resize(paths.size());
            for (size_t i = 0; i < paths.size(); ++i) {
               chain[i] = &mtxs[paths[i]];
            }
            return chain;
         }();
         return chain_read_lock{chain};
      }

      // used for reading from C++ memory
      template <string_literal json_ptr>
      auto read_only_lock()
      {
         // TODO: use a std::array and calculate number of path segments
         static thread_local mutex_chain chain = [&] {
            std::vector<sv> paths = glz::detail::json_ptr_children(json_ptr.sv());
            mutex_chain mtx_chain{};
            mtx_chain.resize(paths.size());
            for (size_t i = 0; i < paths.size(); ++i) {
               mtx_chain[i] = &mtxs[paths[i]];
            }
            return mtx_chain;
         }();
         return chain_write_lock{chain};
      }

      template <string_literal json_ptr>
      auto invoke_lock()
      {
         // TODO: use a std::array and calculate number of path segments
         static thread_local mutex_chain chain = [&] {
            std::vector<sv> paths = glz::detail::json_ptr_children(json_ptr.sv());
            mutex_chain chain{};
            chain.resize(paths.size());
            for (size_t i = 0; i < paths.size(); ++i) {
               chain[i] = &mtxs[paths[i]];
            }
            return chain;
         }();
         return chain_invoke_lock{chain};
      }

      buffer_pool buffers{};

      template <const std::string_view& root = detail::empty_path, class T, const std::string_view& parent = root>
         requires(glz::detail::glaze_object_t<T> || glz::detail::reflectable<T>)
      void on(T& value)
      {
         using namespace glz::detail;
         static constexpr auto N = reflect<T>::size;

         [[maybe_unused]] decltype(auto) t = [&]() -> decltype(auto) {
            if constexpr (reflectable<T> && requires { to_tuple(value); }) {
               return to_tuple(value);
            }
            else {
               return nullptr;
            }
         }();

         if constexpr (parent == root && (glaze_object_t<T> ||
                                          reflectable<T>)&&!std::same_as<std::decay_t<decltype(t)>, std::nullptr_t>) {
            // build read/write calls to the top level object
            methods[root] = [&value, chain = get_chain(root)](repe::state&& state) mutable {
               if (state.header.write()) {
                  chain_read_lock lock{chain};
                  if (not lock) {
                     state.error = {error_e::timeout, std::string(root)};
                     write_response<Opts>(state);
                     return;
                  }
                  if (read_params<Opts>(value, state, state.response) == 0) {
                     return;
                  }
               }

               if (state.header.notify()) {
                  return;
               }

               if (state.header.read()) {
                  chain_write_lock lock{chain};
                  if (not lock) {
                     state.error = {error_e::timeout, std::string(root)};
                     write_response<Opts>(state);
                     return;
                  }
                  write_response<Opts>(value, state);
               }
               else {
                  write_response<Opts>(state);
               }
            };
         }

         for_each<N>([&](auto I) {
            decltype(auto) func = [&]<size_t I>() -> decltype(auto) {
               if constexpr (reflectable<T>) {
                  return get_member(value, get<I>(t));
               }
               else {
                  return get_member(value, get<I>(reflect<T>::values));
               }
            }.template operator()<I>();

            static constexpr auto key = reflect<T>::keys[I];

            static constexpr std::string_view full_key = [&] {
               if constexpr (parent == detail::empty_path) {
                  return join_v<chars<"/">, key>;
               }
               else {
                  return join_v<parent, chars<"/">, key>;
               }
            }();

            // static_assert(std::same_as<decltype(func), refl_t<T, I>>);
            using E = std::remove_cvref_t<decltype(func)>;

            // This logic chain should match glz::cli_menu
            using Func = decltype(func);
            if constexpr (std::is_invocable_v<Func>) {
               using Result = std::decay_t<std::invoke_result_t<Func>>;
               if constexpr (std::same_as<Result, void>) {
                  methods[full_key] = [callback = func, chain = get_chain(full_key)](repe::state&& state) mutable {
                     {
                        callback();
                        if (state.header.notify()) {
                           return;
                        }
                     }
                     write_response<Opts>(state);
                  };
               }
               else {
                  methods[full_key] = [callback = func, chain = get_chain(full_key)](repe::state&& state) mutable {
                     {
                        if (state.header.notify()) {
                           std::ignore = callback();
                           return;
                        }
                        write_response<Opts>(callback(), state);
                     }
                  };
               }
            }
            else if constexpr (is_invocable_concrete<std::remove_cvref_t<Func>>) {
               using Tuple = invocable_args_t<std::remove_cvref_t<Func>>;
               constexpr auto N = glz::tuple_size_v<Tuple>;
               static_assert(N == 1, "Only one input is allowed for your function");

               using Params = glz::tuple_element_t<0, Tuple>;
               // using Result = std::invoke_result_t<Func, Params>;

               methods[full_key] = [callback = func, chain = get_chain(full_key)](repe::state&& state) mutable {
                  static thread_local std::decay_t<Params> params{};
                  // no need lock locals
                  if (read_params<Opts>(params, state, state.response) == 0) {
                     return;
                  }

                  {
                     if (state.header.notify()) {
                        std::ignore = callback(params);
                        return;
                     }
                     write_response<Opts>(callback(params), state);
                  }
               };
            }
            else if constexpr (glaze_object_t<E> || reflectable<E>) {
               on<root, E, full_key>(func);

               // build read/write calls to the object as a variable
               methods[full_key] = [&func, chain = get_chain(full_key)](repe::state&& state) mutable {
                  if (state.header.write()) {
                     chain_read_lock lock{chain};
                     if (not lock) {
                        state.error = {error_e::timeout, std::string(full_key)};
                        write_response<Opts>(state);
                        return;
                     }
                     if (read_params<Opts>(func, state, state.response) == 0) {
                        return;
                     }
                  }

                  if (state.header.notify()) {
                     return;
                  }

                  if (state.header.read()) {
                     chain_write_lock lock{chain};
                     if (not lock) {
                        state.error = {error_e::timeout, std::string(full_key)};
                        write_response<Opts>(state);
                        return;
                     }
                     write_response<Opts>(func, state);
                  }
                  else {
                     write_response<Opts>(state);
                  }
               };
            }
            else if constexpr (not std::is_lvalue_reference_v<Func>) {
               // For glz::custom, glz::manage, etc.
               methods[full_key] = [func, chain = get_chain(full_key)](repe::state&& state) mutable {
                  if (state.header.write()) {
                     chain_read_lock lock{chain};
                     if (not lock) {
                        state.error = {error_e::timeout, std::string(full_key)};
                        write_response<Opts>(state);
                        return;
                     }
                     if (read_params<Opts>(func, state, state.response) == 0) {
                        return;
                     }
                  }

                  if (state.header.notify()) {
                     return;
                  }

                  if (state.header.read()) {
                     chain_write_lock lock{chain};
                     if (not lock) {
                        state.error = {error_e::timeout, std::string(full_key)};
                        write_response<Opts>(state);
                        return;
                     }
                     write_response<Opts>(func, state);
                  }
                  else {
                     write_response<Opts>(state);
                  }
               };
            }
            else {
               static_assert(std::is_lvalue_reference_v<Func>);

               if constexpr (std::is_member_function_pointer_v<std::decay_t<Func>>) {
                  using F = std::decay_t<Func>;
                  using Ret = typename return_type<F>::type;
                  using Tuple = typename inputs_as_tuple<F>::type;
                  constexpr auto n_args = glz::tuple_size_v<Tuple>;
                  if constexpr (std::is_void_v<Ret>) {
                     if constexpr (n_args == 0) {
                        methods[full_key] = [&value, &func, chain = get_chain(full_key)](repe::state&& state) mutable {
                           {
                              (value.*func)();
                           }

                           if (state.header.notify()) {
                              return;
                           }

                           write_response<Opts>(state);
                        };
                     }
                     else if constexpr (n_args == 1) {
                        using Input = std::decay_t<glz::tuple_element_t<0, Tuple>>;
                        methods[full_key] = [&value, &func, chain = get_chain(full_key)](repe::state&& state) mutable {
                           static thread_local Input input{};
                           if (state.header.write()) {
                              if (read_params<Opts>(input, state, state.response) == 0) {
                                 return;
                              }
                           }

                           {
                              (value.*func)(input);
                           }

                           if (state.header.notify()) {
                              return;
                           }

                           write_response<Opts>(state);
                        };
                     }
                     else {
                        static_assert(false_v<Func>, "function cannot have more than one input");
                     }
                  }
                  else {
                     // Member function pointers
                     if constexpr (n_args == 0) {
                        methods[full_key] = [&value, &func, chain = get_chain(full_key)](repe::state&& state) mutable {
                           // using Result = std::decay_t<decltype((value.*func)())>;
                           {
                              if (state.header.notify()) {
                                 std::ignore = (value.*func)();
                                 return;
                              }

                              write_response<Opts>((value.*func)(), state);
                           }
                        };
                     }
                     else if constexpr (n_args == 1) {
                        using Input = std::decay_t<glz::tuple_element_t<0, Tuple>>;
                        methods[full_key] = [this, &value, &func,
                                             chain = get_chain(full_key)](repe::state&& state) mutable {
                           static thread_local Input input{};

                           if (state.header.write()) {
                              if (read_params<Opts>(input, state, state.response) == 0) {
                                 return;
                              }
                           }

                           // using Result = std::decay_t<decltype((value.*func)(input))>;
                           {
                              if (state.header.notify()) {
                                 std::ignore = (value.*func)(input);
                                 return;
                              }

                              write_response<Opts>((value.*func)(input), state);
                           }
                        };
                     }
                     else {
                        static_assert(false_v<Func>, "function cannot have more than one input");
                     }
                  }
               }
               else {
                  // this is a variable and not a function, so we build RPC read/write calls
                  methods[full_key] = [&func, chain = get_chain(full_key)](repe::state&& state) mutable {
                     if (state.header.write()) {
                        chain_read_lock lock{chain};
                        if (not lock) {
                           state.error = {error_e::timeout, std::string(full_key)};
                           write_response<Opts>(state);
                           return;
                        }
                        if (read_params<Opts>(func, state, state.response) == 0) {
                           return;
                        }
                     }

                     if (state.header.notify()) {
                        return;
                     }

                     if (state.header.read()) {
                        chain_write_lock lock{chain};
                        if (not lock) {
                           state.error = {error_e::timeout, std::string(full_key)};
                           write_response<Opts>(state);
                           return;
                        }
                        write_response<Opts>(func, state);
                     }
                     else {
                        write_response<Opts>(state);
                     }
                  };
               }
            }
         });
      }

      template <class Value, class H = header>
      [[nodiscard]] bool call(H&& header, Value&& value)
      {
         return call(request<Opts>(std::forward<H>(header), std::forward<Value>(value)));
      }

      template <class Value, class H = header>
      [[nodiscard]] bool call(H&& header, Value&& value, auto& buffer)
      {
         return call(request<Opts>(std::forward<H>(header), std::forward<Value>(value), buffer));
      }
      
      // returns null for notifications
      [[nodiscard]] shared_buffer call(message& msg)
      {
         shared_buffer u_buffer = std::make_shared<unique_buffer>(&buffers);
         auto& response = *(u_buffer->ptr);

         if (auto it = methods.find(msg.query); it != methods.end()) {
            static thread_local error_t error{};
            it->second(state{msg.body, msg.header, response, error}); // handle the body
         }
         else {
            static constexpr error_e code = error_e::method_not_found;
            static constexpr sv body{error_code_to_sv(code)};
            
            const auto body_length = 8 + body.size(); // 4 bytes for code, 4 bytes for size, + message
            
            response.resize(sizeof(header) + body_length);
            header h{.error = true, .body_length = body_length};
            std::memcpy(response.data(), &h, sizeof(header));
            std::memcpy(response.data() + sizeof(header), &code, 4);
            std::memcpy(response.data() + sizeof(header) + 4, &body_length, 4);
            std::memcpy(response.data() + sizeof(header) + 8, body.data(), body.size());
            return u_buffer;
         }
         
         if (msg.header.notify()) {
            return {};
         }
         else {
            return u_buffer;
         }
      }

      // returns null for notifications
      /*[[nodiscard]] shared_buffer call(const sv msg)
      {
         shared_buffer u_buffer = std::make_shared<unique_buffer>(&buffers);
         auto& response = *(u_buffer->ptr);
         
         if (msg.size() < sizeof(header)) {
            static constexpr error_e code = error_e::invalid_header;
            static constexpr sv body{error_code_to_sv(code)};
            
            const auto body_length = 8 + body.size(); // 4 bytes for code, 4 bytes for size, + message
            
            response.resize(sizeof(header) + body_length);
            header h{.error = true, .body_length = body_length};
            std::memcpy(response.data(), &h, sizeof(header));
            std::memcpy(response.data() + sizeof(header), &code, 4);
            std::memcpy(response.data() + sizeof(header) + 4, &body_length, 4);
            std::memcpy(response.data() + sizeof(header) + 8, body.data(), body.size());
            return u_buffer;
         }
         
         header h{};
         std::memcpy(&h, msg.data(), sizeof(header));
         
         if (sizeof(header) + h.query_length > msg.size()) {
            static constexpr error_e code = error_e::invalid_query;
            static constexpr sv body{error_code_to_sv(code)};
            
            const auto body_length = 8 + body.size(); // 4 bytes for code, 4 bytes for size, + message
            
            response.resize(sizeof(header) + body_length);
            header h{.error = true, .body_length = body_length};
            std::memcpy(response.data(), &h, sizeof(header));
            std::memcpy(response.data() + sizeof(header), &code, 4);
            std::memcpy(response.data() + sizeof(header) + 4, &body_length, 4);
            std::memcpy(response.data() + sizeof(header) + 8, body.data(), body.size());
            return u_buffer;
         }
         
         if (sizeof(header) + h.query_length + h.body_length > msg.size()) {
            static constexpr error_e code = error_e::invalid_body;
            static constexpr sv body{error_code_to_sv(code)};
            
            const auto body_length = 8 + body.size(); // 4 bytes for code, 4 bytes for size, + message
            
            response.resize(sizeof(header) + body_length);
            header h{.error = true, .body_length = body_length};
            std::memcpy(response.data(), &h, sizeof(header));
            std::memcpy(response.data() + sizeof(header), &code, 4);
            std::memcpy(response.data() + sizeof(header) + 4, &body_length, 4);
            std::memcpy(response.data() + sizeof(header) + 8, body.data(), body.size());
            return u_buffer;
         }
         
         const sv query{msg.data() + sizeof(header), h.query_length};
         const sv body{msg.data() + sizeof(header) + h.query_length, h.body_length};

         if (auto it = methods.find(query); it != methods.end()) {
            static thread_local error_t error{};
            it->second(state{body, h, response, error}); // handle the body
         }
         else {
            static constexpr error_e code = error_e::method_not_found;
            static constexpr sv body{error_code_to_sv(code)};
            
            const auto body_length = 8 + body.size(); // 4 bytes for code, 4 bytes for size, + message
            
            response.resize(sizeof(header) + body_length);
            header h{.error = true, .body_length = body_length};
            std::memcpy(response.data(), &h, sizeof(header));
            std::memcpy(response.data() + sizeof(header), &code, 4);
            std::memcpy(response.data() + sizeof(header) + 4, &body_length, 4);
            std::memcpy(response.data() + sizeof(header) + 8, body.data(), body.size());
            return u_buffer;
         }
         
         if (h.notify()) {
            return {};
         }
         else {
            return u_buffer;
         }
      }*/
   };
}
