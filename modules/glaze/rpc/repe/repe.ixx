// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/rpc/repe/repe.hpp"
// glz:header std=<concepts>
// glz:header std=<cstddef>
// glz:header std=<cstdint>
// glz:header std=<cstring>
// glz:header std=<span>
// glz:header std=<string>
// glz:header std=<string_view>
// glz:header std=<tuple>
// glz:header std=<type_traits>
// glz:header std=<utility>
export module glaze.rpc.repe;

import std;

import glaze.rpc.registry_fwd;
import glaze.rpc.repe.header;

import glaze.core.context;
import glaze.core.opts;
import glaze.core.reflect;
import glaze.core.write;

import glaze.json.write;

using std::size_t;
using std::uint32_t;
using std::uint64_t;

namespace glz::repe
{
   struct state final
   {
      repe::message& in;
      repe::message& out;

      bool notify() const { return bool(in.header.notify); }

      bool has_body() const { return bool(in.header.body_length); }
   };

   template <class T>
   concept is_state = std::same_as<std::decay_t<T>, state>;

   template <auto Opts, class Value>
   void write_response(Value&& value, is_state auto&& state)
   {
      auto& in = state.in;
      auto& out = state.out;
      out.header.id = in.header.id;
      if (bool(out.header.ec)) {
         out.header.query_length = out.query.size();
         out.header.body_length = out.body.size();
         out.header.body_format = repe::body_format::UTF8; // Error messages are UTF-8
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
      else {
         const auto ec = write<Opts>(std::forward<Value>(value), out.body);
         if (bool(ec)) [[unlikely]] {
            out.header.ec = ec.ec;
            out.header.body_format = repe::body_format::UTF8; // Error messages are UTF-8
         }
         else {
            if constexpr (Opts.format == JSON) {
               out.header.body_format = repe::body_format::JSON;
            }
            else if constexpr (Opts.format == BEVE) {
               out.header.body_format = repe::body_format::BEVE;
            }
         }
         out.header.query_length = out.query.size();
         out.header.body_length = out.body.size();
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
   }

   template <auto Opts>
   void write_response(is_state auto&& state)
   {
      auto& in = state.in;
      auto& out = state.out;
      out.header.id = in.header.id;
      if (bool(out.header.ec)) {
         out.header.query_length = out.query.size();
         out.header.body_length = out.body.size();
         out.header.body_format = repe::body_format::UTF8; // Error messages are UTF-8
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
      else {
         const auto ec = write<Opts>(nullptr, out.body);
         if (bool(ec)) [[unlikely]] {
            out.header.ec = ec.ec;
            out.header.body_format = repe::body_format::UTF8; // Error messages are UTF-8
         }
         else {
            if constexpr (Opts.format == JSON) {
               out.header.body_format = repe::body_format::JSON;
            }
            else if constexpr (Opts.format == BEVE) {
               out.header.body_format = repe::body_format::BEVE;
            }
         }
         out.query.clear();
         out.header.query_length = out.query.size();
         out.header.body_length = out.body.size();
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
   }

   // Returns false on error, having written the error response -- unless the request is a
   // notification, which is answered by silence whether the read succeeds or fails. A caller that
   // returns immediately on false is correct in both cases; one that inspects state.out afterwards
   // has to allow for it being untouched.
   //
   // The byte count the reader reports cannot stand in for success here: without a null terminator
   // a variant alternative that resolves at the end of the buffer rewinds its iterator, so a
   // completed read can report zero bytes consumed.
   template <auto Opts, class Value>
   bool read_params(Value&& value, auto&& state)
   {
      glz::context ctx{};
      auto [b, e] = read_iterators<Opts>(state.in.body);
      auto start = b;

      // An empty body is a failure like any other and has to answer like one. Returning early left
      // a non-notify request with no response at all: every caller is told the error response is
      // written whenever this returns false, and every registered endpoint returns immediately on
      // it. A parameterized function endpoint reads without a has_body() guard -- it has nothing to
      // call the function with otherwise -- so an empty body reached here and the client was left
      // waiting on a reply that was never sent.
      if (state.in.body.empty()) [[unlikely]] {
         ctx.error = error_code::no_read_input;
      }
      else {
         glz::parse<Opts.format>::template op<is_padded_off<Opts>()>(std::forward<Value>(value), ctx, b, e);
         // This bypasses glz::read, so the bookkeeping glz::read performs after the parse has to be
         // repeated here: a value that finishes exactly at the end of the body reports end_reached,
         // which is a completed read rather than an error, while a body that held no value at all is
         // not. finalize_top_level_read draws that line.
         finalize_top_level_read<Opts>(ctx, start, b, e);
      }

      if (bool(ctx.error)) {
         // A notification is a request the sender has said it will not read a reply to, so a failed
         // read of one is reported by not answering, exactly as a successful read of one is.
         if (state.notify()) {
            return false;
         }

         state.out.header.ec = ctx.error;
         error_ctx ec{size_t(b - start), ctx.error, ctx.custom_error_message};

         auto& in = state.in;
         auto& out = state.out;

         std::string error_message = format_error(ec, in.body);
         out.header.body_length = uint32_t(error_message.size());
         out.header.body_format = repe::body_format::UTF8; // Error messages are UTF-8
         out.body = error_message;

         write_response<Opts>(state);
         return false;
      }

      return true;
   }

   namespace detail
   {
      template <auto Opts>
      struct request_impl
      {
         message operator()(const user_header& h) const
         {
            message msg{};
            msg.header = encode(h);
            msg.query = std::string{h.query};
            msg.header.query_format = repe::query_format::JSON_POINTER;
            msg.header.body_length = msg.body.size();
            msg.header.length = sizeof(repe::header) + msg.query.size() + msg.body.size();
            return msg;
         }

         template <class Value>
         message operator()(const user_header& h, Value&& value) const
         {
            message msg{};
            msg.header = encode(h);
            msg.query = std::string{h.query};
            msg.header.query_format = repe::query_format::JSON_POINTER;
            // TODO: Handle potential write errors and put in msg
            std::ignore = glz::write<Opts>(std::forward<Value>(value), msg.body);
            msg.header.body_length = msg.body.size();
            if constexpr (Opts.format == JSON) {
               msg.header.body_format = repe::body_format::JSON;
            }
            else if constexpr (Opts.format == BEVE) {
               msg.header.body_format = repe::body_format::BEVE;
            }
            msg.header.length = sizeof(repe::header) + msg.query.size() + msg.body.size();
            return msg;
         }

         void operator()(const user_header& h, message& msg) const
         {
            msg.header = encode(h);
            msg.query = std::string{h.query};
            msg.header.query_format = repe::query_format::JSON_POINTER;
            msg.header.body_length = msg.body.size();
            msg.header.length = sizeof(repe::header) + msg.query.size() + msg.body.size();
         }

         template <class Value>
         void operator()(const user_header& h, message& msg, Value&& value) const
         {
            msg.header = encode(h);
            msg.query = std::string{h.query};
            msg.header.query_format = repe::query_format::JSON_POINTER;
            // TODO: Handle potential write errors and put in msg
            std::ignore = glz::write<Opts>(std::forward<Value>(value), msg.body);
            msg.header.body_length = msg.body.size();
            if constexpr (Opts.format == JSON) {
               msg.header.body_format = repe::body_format::JSON;
            }
            else if constexpr (Opts.format == BEVE) {
               msg.header.body_format = repe::body_format::BEVE;
            }
            msg.header.length = sizeof(repe::header) + msg.query.size() + msg.body.size();
         }
      };
   }

   export template <auto Opts>
   inline constexpr auto request = detail::request_impl<Opts>{};

   export inline constexpr auto request_beve = request<opts{BEVE}>;
   export inline constexpr auto request_json = request<opts{JSON}>;

   // ============================================================
   // View Types (zero-copy for query/body, stack-copy for header)
   // ============================================================

   /// View into a REPE request buffer
   /// Header is copied (48 bytes via memcpy) for alignment safety
   /// Query and body are views into the original buffer
   /// Lifetime: Query/body views valid only while underlying buffer exists
   export struct request_view
   {
      header hdr{}; // Copied from buffer (stack allocated, 48 bytes)
      std::string_view query{}; // View into buffer
      std::string_view body{}; // View into buffer

      // Convenience accessors
      [[nodiscard]] uint64_t id() const noexcept { return hdr.id; }
      [[nodiscard]] bool is_notify() const noexcept { return hdr.notify != 0; }
      [[nodiscard]] error_code error() const noexcept { return hdr.ec; }
      [[nodiscard]] body_format format() const noexcept { return hdr.body_format; }
   };

   /// Result of parsing a buffer into a request_view
   export struct parse_result
   {
      request_view request{};
      error_code ec{error_code::none};

      [[nodiscard]] explicit operator bool() const noexcept { return ec == error_code::none; }
   };

   /// Parse a buffer into a request_view
   /// Header is copied via memcpy (48 bytes) for alignment safety
   /// Query and body are views into the original buffer
   /// @param buffer Raw REPE message bytes
   /// @return parse_result with request_view on success, error_code on failure
   export [[nodiscard]] inline parse_result parse_request(std::span<const char> buffer) noexcept
   {
      parse_result result{};

      if (buffer.size() < sizeof(header)) {
         result.ec = error_code::invalid_header;
         return result;
      }

      // Copy header via memcpy for alignment safety (48 bytes, ~2-5ns)
      std::memcpy(&result.request.hdr, buffer.data(), sizeof(header));

      if (result.request.hdr.spec != repe_magic) {
         result.ec = error_code::invalid_header;
         return result;
      }

      if (result.request.hdr.version != 1) {
         result.ec = error_code::version_mismatch;
         return result;
      }

      const auto& hdr = result.request.hdr;
      uint64_t expected_length{};
      if (!checked_message_length(hdr, expected_length)) {
         result.ec = error_code::invalid_header;
         return result;
      }

      // Validate header.length field matches expected value
      if (hdr.length != expected_length) {
         result.ec = error_code::invalid_header;
         return result;
      }

      if (uint64_t(buffer.size()) < expected_length) {
         result.ec = error_code::invalid_body;
         return result;
      }

      // Set up views into the original buffer
      result.request.query = {buffer.data() + sizeof(header), static_cast<size_t>(hdr.query_length)};
      result.request.body = {buffer.data() + sizeof(header) + hdr.query_length, static_cast<size_t>(hdr.body_length)};
      result.ec = error_code::none;

      return result;
   }

   // ============================================================
   // Response Builder
   // ============================================================

   /// Builds a REPE response efficiently
   /// Can write to either:
   /// - A raw buffer (for span-based zero-copy path)
   /// - A repe::message directly (for message-based calls)
   export struct response_builder
   {
      std::string* buffer_{};
      message* msg_{};
      header hdr_{}; // Tracks header state for buffer mode

      void init_header(header& hdr, uint64_t id) noexcept
      {
         hdr = {};
         hdr.spec = repe_magic;
         hdr.version = 1;
         hdr.id = id;
      }

      /// Construct with external buffer (for span-based calls)
      explicit response_builder(std::string& buffer) noexcept : buffer_(&buffer) {}

      /// Construct with message (for message-based calls, writes directly)
      explicit response_builder(message& msg) noexcept : msg_(&msg) {}

      // Non-copyable but movable
      response_builder(const response_builder&) = delete;
      response_builder& operator=(const response_builder&) = delete;
      response_builder(response_builder&&) = default;
      response_builder& operator=(response_builder&&) = default;

      /// Reset for new response, optionally copying ID from request
      void reset(uint64_t id = 0) noexcept
      {
         if (msg_) {
            init_header(msg_->header, id);
            msg_->query.clear();
            msg_->body.clear();
         }
         else {
            buffer_->clear();
            init_header(hdr_, id);
         }
      }

      /// Reset using request_view to copy relevant header fields
      void reset(const request_view& request) noexcept { reset(request.id()); }

      /// Set error state with optional message
      void set_error(error_code ec, std::string_view error_message = {})
      {
         if (msg_) {
            msg_->header.ec = ec;
            msg_->header.body_format = body_format::UTF8;
            msg_->header.query_length = 0;
            msg_->header.body_length = error_message.size();
            msg_->header.length = sizeof(header) + error_message.size();
            msg_->query.clear();
            msg_->body = error_message;
         }
         else {
            hdr_.ec = ec;
            hdr_.body_format = body_format::UTF8;
            hdr_.query_length = 0;
            hdr_.body_length = error_message.size();
            hdr_.length = sizeof(header) + error_message.size();

            buffer_->resize(hdr_.length);
            std::memcpy(buffer_->data(), &hdr_, sizeof(header));
            if (!error_message.empty()) {
               std::memcpy(buffer_->data() + sizeof(header), error_message.data(), error_message.size());
            }
         }
      }

      /// Set error and return true (convenience for early return in handlers)
      /// Usage: if (invalid) { return resp.fail(error_code::invalid_params, "msg"); }
      bool fail(error_code ec, std::string_view error_message = {})
      {
         set_error(ec, error_message);
         return true;
      }

      /// Set body by serializing a value
      template <auto Opts = opts{}, class T>
      error_code set_body(T&& value)
      {
         if (msg_) {
            // Write directly to message body
            auto ec = glz::write<Opts>(std::forward<T>(value), msg_->body);
            if (ec) {
               return ec.ec;
            }

            msg_->header.ec = error_code::none;
            if constexpr (Opts.format == JSON) {
               msg_->header.body_format = body_format::JSON;
            }
            else if constexpr (Opts.format == BEVE) {
               msg_->header.body_format = body_format::BEVE;
            }
            else {
               msg_->header.body_format = body_format::JSON;
            }
            msg_->header.query_length = 0;
            msg_->header.body_length = msg_->body.size();
            msg_->header.length = sizeof(header) + msg_->body.size();
            return error_code::none;
         }
         else {
            // Serialize body to temporary buffer first (glz::write clears the buffer)
            std::string body_buffer;
            auto ec = glz::write<Opts>(std::forward<T>(value), body_buffer);
            if (ec) {
               return ec.ec;
            }

            hdr_.ec = error_code::none;
            if constexpr (Opts.format == JSON) {
               hdr_.body_format = body_format::JSON;
            }
            else if constexpr (Opts.format == BEVE) {
               hdr_.body_format = body_format::BEVE;
            }
            else {
               hdr_.body_format = body_format::JSON;
            }
            hdr_.query_length = 0;
            hdr_.body_length = body_buffer.size();
            hdr_.length = sizeof(header) + body_buffer.size();

            // Write header then body
            buffer_->resize(hdr_.length);
            std::memcpy(buffer_->data(), &hdr_, sizeof(header));
            if (!body_buffer.empty()) {
               std::memcpy(buffer_->data() + sizeof(header), body_buffer.data(), body_buffer.size());
            }
            return error_code::none;
         }
      }

      /// Set body from raw bytes
      void set_body_raw(std::string_view body, body_format fmt = body_format::JSON)
      {
         if (msg_) {
            msg_->header.ec = error_code::none;
            msg_->header.body_format = fmt;
            msg_->header.query_length = 0;
            msg_->header.body_length = body.size();
            msg_->header.length = sizeof(header) + body.size();
            msg_->query.clear();
            msg_->body = body;
         }
         else {
            hdr_.ec = error_code::none;
            hdr_.body_format = fmt;
            hdr_.query_length = 0;
            hdr_.body_length = body.size();
            hdr_.length = sizeof(header) + body.size();

            buffer_->resize(hdr_.length);
            std::memcpy(buffer_->data(), &hdr_, sizeof(header));
            if (!body.empty()) {
               std::memcpy(buffer_->data() + sizeof(header), body.data(), body.size());
            }
         }
      }

      /// Get the finalized response as a span (buffer mode only)
      [[nodiscard]] std::span<const char> finish() const noexcept
      {
         if (buffer_) {
            return {buffer_->data(), buffer_->size()};
         }
         return {};
      }

      /// Get the finalized response as string_view (buffer mode only)
      [[nodiscard]] std::string_view view() const noexcept
      {
         if (buffer_) {
            return *buffer_;
         }
         return {};
      }

      /// Check if response is empty (e.g., for notifications)
      [[nodiscard]] bool empty() const noexcept
      {
         if (msg_) {
            return msg_->body.empty() && msg_->header.ec == error_code::none;
         }
         return buffer_->empty();
      }

      /// Clear without deallocating (for buffer reuse)
      void clear() noexcept
      {
         if (msg_) {
            msg_->query.clear();
            msg_->body.clear();
         }
         else {
            buffer_->clear();
         }
      }

      /// Access underlying buffer for advanced use (buffer mode only)
      [[nodiscard]] std::string* buffer_ptr() noexcept { return buffer_; }
      [[nodiscard]] const std::string* buffer_ptr() const noexcept { return buffer_; }

      /// Access underlying message (message mode only)
      [[nodiscard]] message* message_ptr() noexcept { return msg_; }
      [[nodiscard]] const message* message_ptr() const noexcept { return msg_; }
   };

   // ============================================================
   // Zero-Copy State for Procedures
   // ============================================================

   /// Zero-copy state for RPC procedures
   /// Input is a view into the original request buffer
   /// Output writes directly to the response buffer
   export struct state_view final
   {
      const request_view& in;
      response_builder& out;

      [[nodiscard]] bool notify() const noexcept { return in.is_notify(); }
      [[nodiscard]] bool has_body() const noexcept { return !in.body.empty(); }
   };

   template <class T>
   concept is_state_view = std::same_as<std::decay_t<T>, state_view>;

   /// Read parameters from state_view (zero-copy from input buffer)
   /// Returns false on error, with the error set in state.out -- except for a notification, which
   /// is left unanswered and so leaves state.out untouched. See the note on the overload above for
   /// why this is not a byte count.
   export template <auto Opts, class Value>
   bool read_params(Value&& value, state_view& state)
   {
      glz::context ctx{};
      auto body = state.in.body;
      auto b = body.data();
      auto e = b + body.size();
      auto start = b;

      // An empty body answers like any other failure; see the note in the message overload above.
      if (body.empty()) [[unlikely]] {
         ctx.error = error_code::no_read_input;
      }
      else {
         glz::parse<Opts.format>::template op<is_padded_off<Opts>()>(std::forward<Value>(value), ctx, b, e);
         // See the note in the message overload above for why glz::read's bookkeeping is repeated.
         finalize_top_level_read<Opts>(ctx, start, b, e);
      }

      if (bool(ctx.error)) {
         // A notification is a request the sender has said it will not read a reply to, so a failed
         // read of one is reported by not answering, exactly as a successful read of one is.
         if (state.notify()) {
            return false;
         }

         error_ctx ec{size_t(b - start), ctx.error, ctx.custom_error_message};
         std::string error_message = format_error(ec, body);
         state.out.reset(state.in);
         state.out.set_error(ctx.error, error_message);
         return false;
      }

      return true;
   }

   /// Write response with a value (zero-copy to output buffer)
   export template <auto Opts, class Value>
   void write_response(Value&& value, state_view& state)
   {
      state.out.reset(state.in);
      auto ec = state.out.template set_body<Opts>(std::forward<Value>(value));
      if (bool(ec)) [[unlikely]] {
         state.out.set_error(ec, "Failed to serialize response");
      }
   }

   /// Write response without a value (null body)
   export template <auto Opts>
   void write_response(state_view& state)
   {
      state.out.reset(state.in);
      auto ec = state.out.template set_body<Opts>(nullptr);
      if (bool(ec)) [[unlikely]] {
         state.out.set_error(ec, "Failed to serialize response");
      }
   }
}
