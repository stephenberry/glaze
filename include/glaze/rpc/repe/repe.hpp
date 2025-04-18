// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/rpc/repe/header.hpp"

namespace glz::repe
{
   struct state final
   {
      repe::message& in;
      repe::message& out;
      
      bool notify() const { return in.header.notify(); }
      
      bool read() const { return in.header.read(); }
      
      bool write() const { return in.header.write(); }
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
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
      else {
         const auto ec = write<Opts>(std::forward<Value>(value), out.body);
         if (bool(ec)) [[unlikely]] {
            out.header.ec = ec.ec;
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
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
      else {
         const auto ec = write<Opts>(nullptr, out.body);
         if (bool(ec)) [[unlikely]] {
            out.header.ec = ec.ec;
         }
         out.query.clear();
         out.header.query_length = out.query.size();
         out.header.body_length = out.body.size();
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
   }
   
   // returns 0 on error
   template <auto Opts, class Value>
   size_t read_params(Value&& value, auto&& state)
   {
      glz::context ctx{};
      auto [b, e] = read_iterators<Opts>(state.in.body);
      if (state.in.body.empty()) [[unlikely]] {
         ctx.error = error_code::no_read_input;
      }
      if (bool(ctx.error)) [[unlikely]] {
         return 0;
      }
      auto start = b;
      
      glz::parse<Opts.format>::template op<Opts>(std::forward<Value>(value), ctx, b, e);
      
      if (bool(ctx.error)) {
         state.out.header.ec = ctx.error;
         error_ctx ec{ctx.error, ctx.custom_error_message, size_t(b - start), ctx.includer_error};
         
         auto& in = state.in;
         auto& out = state.out;
         
         std::string error_message = format_error(ec, in.body);
         const uint32_t n = uint32_t(error_message.size());
         out.header.body_length = 4 + n;
         out.body.resize(out.header.body_length);
         std::memcpy(out.body.data(), &n, 4);
         std::memcpy(out.body.data() + 4, error_message.data(), n);
         
         write_response<Opts>(state);
         return 0;
      }
      
      return size_t(b - start);
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
            msg.header.read(true); // because no value provided
            msg.query = std::string{h.query};
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
            msg.header.write(true);
            // TODO: Handle potential write errors and put in msg
            std::ignore = glz::write<Opts>(std::forward<Value>(value), msg.body);
            msg.header.body_length = msg.body.size();
            msg.header.length = sizeof(repe::header) + msg.query.size() + msg.body.size();
            return msg;
         }
         
         void operator()(const user_header& h, message& msg) const
         {
            msg.header = encode(h);
            msg.header.read(true); // because no value provided
            msg.query = std::string{h.query};
            msg.header.body_length = msg.body.size();
            msg.header.length = sizeof(repe::header) + msg.query.size() + msg.body.size();
         }
         
         template <class Value>
         void operator()(const user_header& h, message& msg, Value&& value) const
         {
            msg.header = encode(h);
            msg.query = std::string{h.query};
            msg.header.write(true);
            // TODO: Handle potential write errors and put in msg
            std::ignore = glz::write<Opts>(std::forward<Value>(value), msg.body);
            msg.header.body_length = msg.body.size();
            msg.header.length = sizeof(repe::header) + msg.query.size() + msg.body.size();
         }
      };
   }
   
   template <auto Opts>
   inline constexpr auto request = detail::request_impl<Opts>{};
   
   inline constexpr auto request_beve = request<opts{BEVE}>;
   inline constexpr auto request_json = request<opts{JSON}>;
}
