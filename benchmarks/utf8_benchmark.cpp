#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"
#include "glaze/util/parse.hpp"

//
// The utf8_checker class below is copied/adapted from Boost.Beast:
// https://github.com/boostorg/beast/blob/develop/include/boost/beast/websocket/detail/utf8_checker.hpp
//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE-BOOST-1.0.txt in this repository or https://www.boost.org/LICENSE_1_0.txt
//
class utf8_checker
{
   std::size_t need_ = 0; // chars we need to finish the code point
   std::uint8_t* p_ = cp_; // current position in temp buffer
   std::uint8_t cp_[4]; // a temp buffer for the code point

  public:
   /** Prepare to process text as valid utf8
    */

   void reset();

   /** Check that all processed text is valid utf8
    */

   bool finish();

   /** Check if text is valid UTF8

       @return `true` if the text is valid utf8 or false otherwise.
   */

   bool write(const std::uint8_t* in, std::size_t size);

   /** Check if text is valid UTF8

       @return `true` if the text is valid utf8 or false otherwise.
   */
   template <class ConstBufferSequence>
   bool write(const ConstBufferSequence& bs);
};

void utf8_checker::reset()
{
   need_ = 0;
   p_ = cp_;
}

bool utf8_checker::finish()
{
   const auto success = need_ == 0;
   reset();
   return success;
}

bool utf8_checker::write(const std::uint8_t* in, std::size_t size)
{
   const auto valid = [](const std::uint8_t*& p) {
      if (p[0] < 128) {
         ++p;
         return true;
      }
      if ((p[0] & 0xe0) == 0xc0) {
         if ((p[1] & 0xc0) != 0x80 || (p[0] & 0x1e) == 0) // overlong
            return false;
         p += 2;
         return true;
      }
      if ((p[0] & 0xf0) == 0xe0) {
         if ((p[1] & 0xc0) != 0x80 || (p[2] & 0xc0) != 0x80 || (p[0] == 0xe0 && (p[1] & 0x20) == 0) // overlong
             || (p[0] == 0xed && (p[1] & 0x20) == 0x20) // surrogate
             //|| (p[0] == 0xef && p[1] == 0xbf && (p[2] & 0xfe) == 0xbe) // U+FFFE or U+FFFF
         )
            return false;
         p += 3;
         return true;
      }
      if ((p[0] & 0xf8) == 0xf0) {
         if ((p[0] & 0x07) >= 0x05 // invalid F5...FF characters
             || (p[1] & 0xc0) != 0x80 || (p[2] & 0xc0) != 0x80 || (p[3] & 0xc0) != 0x80 ||
             (p[0] == 0xf0 && (p[1] & 0x30) == 0) // overlong
             || (p[0] == 0xf4 && p[1] > 0x8f) || p[0] > 0xf4 // > U+10FFFF
         )
            return false;
         p += 4;
         return true;
      }
      return false;
   };
   const auto fail_fast = [&]() {
      if (cp_[0] < 128) {
         return false;
      }

      const auto& p = cp_; // alias, only to keep this code similar to valid() above
      const auto known_only = p_ - cp_;
      if (known_only == 1) {
         if ((p[0] & 0xe0) == 0xc0) {
            return ((p[0] & 0x1e) == 0); // overlong
         }
         if ((p[0] & 0xf0) == 0xe0) {
            return false;
         }
         if ((p[0] & 0xf8) == 0xf0) {
            return ((p[0] & 0x07) >= 0x05); // invalid F5...FF characters
         }
      }
      else if (known_only == 2) {
         if ((p[0] & 0xe0) == 0xc0) {
            return ((p[1] & 0xc0) != 0x80 || (p[0] & 0x1e) == 0); // overlong
         }
         if ((p[0] & 0xf0) == 0xe0) {
            return ((p[1] & 0xc0) != 0x80 || (p[0] == 0xe0 && (p[1] & 0x20) == 0) // overlong
                    || (p[0] == 0xed && (p[1] & 0x20) == 0x20)); // surrogate
         }
         if ((p[0] & 0xf8) == 0xf0) {
            return ((p[0] & 0x07) >= 0x05 // invalid F5...FF characters
                    || (p[1] & 0xc0) != 0x80 || (p[0] == 0xf0 && (p[1] & 0x30) == 0) // overlong
                    || (p[0] == 0xf4 && p[1] > 0x8f) || p[0] > 0xf4); // > U+10FFFF
         }
      }
      else if (known_only == 3) {
         if ((p[0] & 0xe0) == 0xc0) {
            return ((p[1] & 0xc0) != 0x80 || (p[0] & 0x1e) == 0); // overlong
         }
         if ((p[0] & 0xf0) == 0xe0) {
            return ((p[1] & 0xc0) != 0x80 || (p[2] & 0xc0) != 0x80 || (p[0] == 0xe0 && (p[1] & 0x20) == 0) // overlong
                    || (p[0] == 0xed && (p[1] & 0x20) == 0x20)); // surrogate
            //|| (p[0] == 0xef && p[1] == 0xbf && (p[2] & 0xfe) == 0xbe) // U+FFFE or U+FFFF
         }
         if ((p[0] & 0xf8) == 0xf0) {
            return ((p[0] & 0x07) >= 0x05 // invalid F5...FF characters
                    || (p[1] & 0xc0) != 0x80 || (p[2] & 0xc0) != 0x80 ||
                    (p[0] == 0xf0 && (p[1] & 0x30) == 0) // overlong
                    || (p[0] == 0xf4 && p[1] > 0x8f) || p[0] > 0xf4); // > U+10FFFF
         }
      }
      return true;
   };
   const auto needed = [](const std::uint8_t v) {
      if (v < 128) return 1;
      if (v < 192) return 0;
      if (v < 224) return 2;
      if (v < 240) return 3;
      if (v < 248) return 4;
      return 0;
   };

   const auto end = in + size;

   // Finish up any incomplete code point
   if (need_ > 0) {
      // Calculate what we have
      auto n = (std::min)(size, need_);
      size -= n;
      need_ -= n;

      // Add characters to the code point
      while (n--) *p_++ = *in++;
      assert(p_ <= cp_ + 4);

      // Still incomplete?
      if (need_ > 0) {
         // Incomplete code point
         assert(in == end);

         // Do partial validation on the incomplete
         // code point, this is called "Fail fast"
         // in Autobahn|Testsuite parlance.
         return !fail_fast();
      }

      // Complete code point, validate it
      const std::uint8_t* p = &cp_[0];
      if (!valid(p)) return false;
      p_ = cp_;
   }

   if (size <= sizeof(std::size_t)) goto slow;

   // Align `in` to sizeof(std::size_t) boundary
   {
      const auto in0 = in;
      auto last = reinterpret_cast<const std::uint8_t*>(
         ((reinterpret_cast<std::uintptr_t>(in) + sizeof(std::size_t) - 1) / sizeof(std::size_t)) *
         sizeof(std::size_t));

      // Check one character at a time for low-ASCII
      while (in < last) {
         if (*in & 0x80) {
            // Not low-ASCII so switch to slow loop
            size = size - (in - in0);
            goto slow;
         }
         ++in;
      }
      size = size - (in - in0);
   }

   // Fast loop: Process 4 or 8 low-ASCII characters at a time
   {
      const auto in0 = in;
      auto last = in + size - 7;
      auto constexpr mask = static_cast<std::size_t>(0x8080808080808080 & ~std::size_t{0});
      while (in < last) {
#if 0
            std::size_t temp;
            std::memcpy(&temp, in, sizeof(temp));
            if((temp & mask) != 0)
#else
         // Technically UB but works on all known platforms
         if ((*reinterpret_cast<const std::size_t*>(in) & mask) != 0)
#endif
         {
            size = size - (in - in0);
            goto slow;
         }
         in += sizeof(std::size_t);
      }
      // There's at least one more full code point left
      last += 4;
      while (in < last)
         if (!valid(in)) return false;
      goto tail;
   }

slow:
   // Slow loop: Full validation on one code point at a time
   {
      auto last = in + size - 3;
      while (in < last)
         if (!valid(in)) return false;
   }

tail:
   // Handle the remaining bytes. The last
   // characters could split a code point so
   // we save the partial code point for later.
   //
   // On entry to the loop, `in` points to the
   // beginning of a code point.
   //
   for (;;) {
      // Number of chars left
      auto n = end - in;
      if (!n) break;

      // Chars we need to finish this code point
      const auto need = needed(*in);
      if (need == 0) return false;
      if (need <= n) {
         // Check a whole code point
         if (!valid(in)) return false;
      }
      else {
         // Calculate how many chars we need
         // to finish this partial code point
         need_ = need - n;

         // Save the partial code point
         while (n--) *p_++ = *in++;
         assert(in == end);
         assert(p_ <= cp_ + 4);

         // Do partial validation on the incomplete
         // code point, this is called "Fail fast"
         // in Autobahn|Testsuite parlance.
         return !fail_fast();
      }
   }
   return true;
}

bool check_utf8(const char* p, std::size_t n)
{
   utf8_checker c;
   if (!c.write(reinterpret_cast<const uint8_t*>(p), n)) return false;
   return c.finish();
}

struct websocket_payload_case
{
   const char* name;
   std::string payload;
   size_t fragment_size;
};

static std::string build_exact_payload(const std::string_view prefix, const std::string_view repeated,
                                       const std::string_view suffix, const size_t target_size)
{
   assert(prefix.size() + suffix.size() <= target_size);

   std::string out;
   out.reserve(target_size);
   out.append(prefix.data(), prefix.size());

   while (out.size() + repeated.size() + suffix.size() <= target_size) {
      out.append(repeated.data(), repeated.size());
   }

   static constexpr std::string_view ascii_pad =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,:;_-";
   while (out.size() + suffix.size() < target_size) {
      const auto remaining = target_size - suffix.size() - out.size();
      const auto n = (std::min)(remaining, ascii_pad.size());
      out.append(ascii_pad.data(), n);
   }

   out.append(suffix.data(), suffix.size());
   return out;
}

static std::vector<websocket_payload_case> build_websocket_payload_cases()
{
   std::vector<websocket_payload_case> cases;
   cases.reserve(8);

   cases.push_back({"WS subscribe JSON 124 B (small payload)",
                    build_exact_payload(R"({"op":"subscribe","channel":"prices:eurusd","last_id":8321,"trace":")",
                                        "a7c42", R"("})", 124),
                    31});

   cases.push_back(
      {"WS subscribe ack 126 B (extended-16 boundary)",
       build_exact_payload(R"({"op":"subscribed","channel":"prices:eurusd","server_ts":1718181024123,"cursor":")", "42",
                           R"(","compression":"none"})", 126),
       63});

   cases.push_back(
      {"WS chat message 512 B (emoji and text)",
       build_exact_payload(R"({"type":"chat.message","room":"general","user":"mila","text":")",
                           "thanks for the update \xF0\x9F\x91\x8D status looks good; next deploy at 14:00 CET. ",
                           R"(","mentions":["ops"],"id":"msg-83421"})", 512),
       125});

   cases.push_back(
      {"WS presence snapshot 1 KiB (mostly ASCII JSON)",
       build_exact_payload(R"({"type":"presence.snapshot","room":"lobby","members":")",
                           "u0001 online idle typing; u0002 away; u0003 online mobile; ", R"(","seq":1842203})", 1024),
       256});

   cases.push_back({"WS collaborative edit 4 KiB (mixed UTF-8)",
                    build_exact_payload(R"({"type":"doc.patch","doc":"roadmap","ops_text":")",
                                        "replace /sections/12/body with caf\xC3\xA9 notes "
                                        "\xE2\x82\xAC budget marker and editor reaction \xF0\x9F\x93\x9D; ",
                                        R"(","rev":4892})", 4 * 1024),
                    512});

   cases.push_back(
      {"WS telemetry batch 16 KiB (ASCII log lines)",
       build_exact_payload(R"({"type":"telemetry.batch","source":"edge-ws-17","lines":")",
                           "INFO 2026-06-13T18:42:11Z ws route=/v1/stream latency_ms=4 bytes_out=918 status=ok\\n",
                           R"(","dropped":0})", 16 * 1024),
       4 * 1024});

   cases.push_back(
      {"WS app state snapshot 64 KiB+1 (extended-64 boundary)",
       build_exact_payload(R"({"type":"state.snapshot","app":"trading","version":8891,"state":")",
                           "EURUSD bid=1.08421 ask=1.08423 qty=125000; BTCUSD bid=67210.5 ask=67211.0 qty=0.42; ",
                           R"(","complete":true})", 64 * 1024 + 1),
       4 * 1024});

   cases.push_back({"WS chat history 256 KiB (multilingual transcript)",
                    build_exact_payload(R"({"type":"chat.history","room":"support","transcript":")",
                                        "agent: received message \xF0\x9F\x93\xA8; user: "
                                        "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"
                                        " / "
                                        "\xE4\xB8\x96\xE7\x95\x8C"
                                        " / "
                                        "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF"
                                        " status ok\\n",
                                        R"(","page":7})", 256 * 1024),
                    16 * 1024});

   return cases;
}

static bool validate_stream_whole(const std::string_view s) noexcept
{
   glz::utf8_stream_validator validator;
   return validator.consume(s.data(), s.size()) && validator.complete();
}

static bool validate_stream_chunks(const std::string_view s, const size_t chunk_size) noexcept
{
   glz::utf8_stream_validator validator;
   size_t offset = 0;
   while (offset < s.size()) {
      const size_t n = (std::min)(chunk_size, s.size() - offset);
      if (!validator.consume(s.data() + offset, n)) {
         return false;
      }
      offset += n;
   }
   return validator.complete();
}

static bool validate_beast_chunks(const std::string_view s, const size_t chunk_size)
{
   utf8_checker checker;
   size_t offset = 0;
   while (offset < s.size()) {
      const size_t n = (std::min)(chunk_size, s.size() - offset);
      if (!checker.write(reinterpret_cast<const uint8_t*>(s.data() + offset), n)) {
         return false;
      }
      offset += n;
   }
   return checker.finish();
}

static size_t repetitions_per_run(const size_t payload_size) noexcept
{
   if (payload_size < 256) return 256;
   if (payload_size < 1024) return 64;
   if (payload_size < 4 * 1024) return 16;
   return 1;
}

static void bench_whole_buffer(const char* name, const std::string& input)
{
   bencher::stage stage{name};
   stage.min_execution_count = 100;
   const auto repetitions = repetitions_per_run(input.size());

   stage.run("glz::validate_utf8", [&] {
      bool valid = true;
      for (size_t i = 0; i < repetitions; ++i) {
         const bool iteration_valid = glz::validate_utf8(input.data(), input.size());
         bencher::do_not_optimize(iteration_valid);
         valid &= iteration_valid;
      }
      bencher::do_not_optimize(valid);
      return input.size() * repetitions;
   });

   stage.run("utf8_stream_validator whole buffer", [&] {
      bool valid = true;
      for (size_t i = 0; i < repetitions; ++i) {
         const bool iteration_valid = validate_stream_whole(input);
         bencher::do_not_optimize(iteration_valid);
         valid &= iteration_valid;
      }
      bencher::do_not_optimize(valid);
      return input.size() * repetitions;
   });

   stage.run("boost.beast utf8_checker whole buffer", [&] {
      bool valid = true;
      for (size_t i = 0; i < repetitions; ++i) {
         const bool iteration_valid = check_utf8(input.data(), input.size());
         bencher::do_not_optimize(iteration_valid);
         valid &= iteration_valid;
      }
      bencher::do_not_optimize(valid);
      return input.size() * repetitions;
   });

   bencher::print_results(stage);
}

static void bench_fragmented(const char* name, const std::string& input, const size_t chunk_size)
{
   assert(chunk_size > 0);

   bencher::stage stage{name};
   stage.min_execution_count = 100;
   const auto repetitions = repetitions_per_run(input.size());

   stage.run("utf8_stream_validator chunks", [&] {
      bool valid = true;
      for (size_t i = 0; i < repetitions; ++i) {
         const bool iteration_valid = validate_stream_chunks(input, chunk_size);
         bencher::do_not_optimize(iteration_valid);
         valid &= iteration_valid;
      }
      bencher::do_not_optimize(valid);
      return input.size() * repetitions;
   });

   stage.run("boost.beast utf8_checker chunks", [&] {
      bool valid = true;
      for (size_t i = 0; i < repetitions; ++i) {
         const bool iteration_valid = validate_beast_chunks(input, chunk_size);
         bencher::do_not_optimize(iteration_valid);
         valid &= iteration_valid;
      }
      bencher::do_not_optimize(valid);
      return input.size() * repetitions;
   });

   bencher::print_results(stage);
}

static void verify_payload(const websocket_payload_case& payload_case)
{
   const auto input = std::string_view{payload_case.payload};
   const bool glz_valid = glz::validate_utf8(input.data(), input.size());
   const bool stream_valid = validate_stream_chunks(input, payload_case.fragment_size);
   const bool beast_valid = validate_beast_chunks(input, payload_case.fragment_size);

   if (!glz_valid || !stream_valid || !beast_valid) {
      std::fprintf(stderr, "Invalid UTF-8 payload generated for case: %s\n", payload_case.name);
      std::abort();
   }
}

int main()
{
   const auto cases = build_websocket_payload_cases();

   std::printf("=== WebSocket Text Payload UTF-8 Whole Buffer Validation Benchmarks ===\n\n");

   for (const auto& payload_case : cases) {
      verify_payload(payload_case);
      bench_whole_buffer(payload_case.name, payload_case.payload);
   }

   std::printf("\n=== Fragmented WebSocket Text Payload UTF-8 Streaming Validator Benchmarks ===\n\n");

   for (const auto& payload_case : cases) {
      const auto name =
         std::string{payload_case.name} + " / " + std::to_string(payload_case.fragment_size) + "-byte chunks";
      bench_fragmented(name.c_str(), payload_case.payload, payload_case.fragment_size);
   }

   return 0;
}