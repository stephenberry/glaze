// Tests that glz::streaming_connection serializes its writes: sends made back to back, or
// from several threads at once, reach the client whole and in the order each sender made
// them, close() waits for the writes queued before it, and a client that goes away fails
// the writes still queued instead of leaving their handlers uncalled, as does a send made
// after close().

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "glaze/net/http_server.hpp"
#include "ut/ut.hpp"

#if defined(GLZ_USING_BOOST_ASIO)
namespace asio
{
   using namespace boost::asio;
   using error_code = boost::system::error_code;
}
#endif

using namespace ut;
using namespace std::chrono_literals;

namespace
{
   // Larger than the 64 KiB asio::async_write hands the socket per write_some, so every
   // event takes several writes and two overlapping async_writes can interleave.
   constexpr size_t payload_size = 70000;

   std::string make_payload(size_t sender, size_t seq)
   {
      std::string payload = "sender=" + std::to_string(sender) + " seq=" + std::to_string(seq) + " ";
      payload.append(payload_size, char('a' + (sender * 7 + seq) % 26));
      return payload;
   }

   std::string make_id(size_t sender, size_t seq) { return std::to_string(sender) + "-" + std::to_string(seq); }

   struct test_server
   {
      glz::http_server<> server;
      std::thread server_thread;
      uint16_t port = 0;

      // Threads a handler starts; joined before the server stops
      std::mutex workers_mutex;
      std::vector<std::thread> workers;

      void add_worker(std::thread t)
      {
         std::lock_guard lock(workers_mutex);
         workers.push_back(std::move(t));
      }

      bool start(size_t threads)
      {
         server.on_error([](std::error_code, std::source_location) {});
         server.bind("127.0.0.1", 0);
         port = server.port();
         server_thread = std::thread([this, threads] { server.start(threads); });
         std::this_thread::sleep_for(200ms);
         return port != 0;
      }

      ~test_server()
      {
         {
            std::lock_guard lock(workers_mutex);
            for (auto& t : workers) {
               if (t.joinable()) t.join();
            }
         }
         server.stop();
         if (server_thread.joinable()) server_thread.join();
      }
   };

   // Sends a GET for target and reads the reply until the server closes the connection.
   // Reads stop after limit bytes or at the deadline, whichever comes first.
   std::string fetch_raw(uint16_t port, const std::string& target, std::chrono::seconds deadline = 30s)
   {
      asio::io_context ctx;
      asio::ip::tcp::socket socket(ctx);
      asio::error_code ec;
      socket.connect({asio::ip::make_address("127.0.0.1"), port}, ec);
      if (ec) return {};
      const std::string request = "GET " + target + " HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
      asio::write(socket, asio::buffer(request), ec);
      if (ec) return {};

      std::string raw;
      asio::async_read(socket, asio::dynamic_buffer(raw), [](asio::error_code, std::size_t) {});
      ctx.run_for(deadline);
      return raw;
   }

   struct decoded_response
   {
      std::string headers;
      std::string body;
      bool terminated = false; // The zero-length chunk was seen
      std::string error; // First framing error, empty if the framing is valid
   };

   // Splits the header section off and undoes the chunked framing, failing on anything
   // that is not a well-formed chunk. Interleaved writes show up here as a chunk size
   // line that is not hex or a chunk not followed by CRLF.
   decoded_response decode_chunked(const std::string& raw)
   {
      decoded_response out;
      const auto header_end = raw.find("\r\n\r\n");
      if (header_end == std::string::npos) {
         out.error = "no header section";
         return out;
      }
      out.headers = raw.substr(0, header_end + 4);
      size_t pos = header_end + 4;
      while (true) {
         const auto line_end = raw.find("\r\n", pos);
         if (line_end == std::string::npos) {
            out.error = "truncated chunk size line at " + std::to_string(pos);
            return out;
         }
         size_t size = 0;
         const char* first = raw.data() + pos;
         const char* last = raw.data() + line_end;
         auto [ptr, ec] = std::from_chars(first, last, size, 16);
         if (ec != std::errc{} || ptr != last || first == last) {
            out.error = "bad chunk size line at " + std::to_string(pos) + ": " +
                        raw.substr(pos, std::min<size_t>(40, line_end - pos));
            return out;
         }
         pos = line_end + 2;
         if (size == 0) {
            if (raw.compare(pos, 2, "\r\n") != 0) {
               out.error = "terminator not followed by CRLF";
               return out;
            }
            out.terminated = true;
            if (pos + 2 != raw.size()) {
               out.error = "bytes after the terminating chunk";
            }
            return out;
         }
         if (raw.size() < pos + size + 2) {
            out.error = "truncated chunk at " + std::to_string(pos);
            return out;
         }
         out.body.append(raw, pos, size);
         pos += size;
         if (raw.compare(pos, 2, "\r\n") != 0) {
            out.error = "chunk data not followed by CRLF at " + std::to_string(pos);
            return out;
         }
         pos += 2;
      }
   }

   struct sse_event
   {
      std::string id;
      std::string event;
      std::string data;
   };

   // Parses the event stream the server writes: "id:", "event:" and one "data:" line
   // each, an empty line after every event. Anything else is a parse error.
   bool parse_events(const std::string& body, std::vector<sse_event>& events, std::string& error)
   {
      size_t pos = 0;
      while (pos < body.size()) {
         const auto end = body.find("\n\n", pos);
         if (end == std::string::npos) {
            error = "unterminated event at " + std::to_string(pos);
            return false;
         }
         sse_event ev;
         std::string_view block(body.data() + pos, end - pos);
         while (!block.empty()) {
            const auto nl = block.find('\n');
            const auto line = block.substr(0, nl);
            block = nl == std::string_view::npos ? std::string_view{} : block.substr(nl + 1);
            if (line.starts_with("id: "))
               ev.id = line.substr(4);
            else if (line.starts_with("event: "))
               ev.event = line.substr(7);
            else if (line.starts_with("data: "))
               ev.data = line.substr(6);
            else {
               error = "unexpected line at " + std::to_string(pos) + ": " +
                       std::string(line.substr(0, std::min<size_t>(40, line.size())));
               return false;
            }
         }
         events.push_back(std::move(ev));
         pos = end + 2;
      }
      return true;
   }

   // Checks that the events are exactly senders x per_sender events, each with the
   // payload its id names, and that every sender's events arrive in the order it sent them.
   void expect_events(const std::vector<sse_event>& events, size_t senders, size_t per_sender)
   {
      expect(events.size() == senders * per_sender) << "event count " << events.size();
      std::vector<size_t> next_seq(senders, 0);
      size_t bad = 0;
      for (const auto& ev : events) {
         const auto dash = ev.id.find('-');
         size_t sender = 0, seq = 0;
         if (dash == std::string::npos ||
             std::from_chars(ev.id.data(), ev.id.data() + dash, sender).ec != std::errc{} ||
             std::from_chars(ev.id.data() + dash + 1, ev.id.data() + ev.id.size(), seq).ec != std::errc{} ||
             sender >= senders) {
            ++bad;
            continue;
         }
         if (seq != next_seq[sender]) ++bad;
         next_seq[sender] = seq + 1;
         if (ev.event != "tick" || ev.data != make_payload(sender, seq)) ++bad;
      }
      expect(bad == 0) << bad << " events out of order or damaged";
   }

   template <class Pred>
   bool wait_until(Pred pred, std::chrono::milliseconds timeout = 10s)
   {
      const auto deadline = std::chrono::steady_clock::now() + timeout;
      while (!pred()) {
         if (std::chrono::steady_clock::now() > deadline) return false;
         std::this_thread::sleep_for(1ms);
      }
      return true;
   }
}

suite stream_write_queue_tests = [] {
   "concurrent_send_event_from_many_threads"_test = [] {
      constexpr size_t senders = 4;
      constexpr size_t per_sender = 40;

      struct counters
      {
         std::atomic<size_t> completed{0};
         std::atomic<size_t> errors{0};
      };
      auto state = std::make_shared<counters>();

      test_server ts;
      ts.server.stream_get("/events", [&ts, state](glz::request&, glz::streaming_response& res) {
         res.as_event_stream();
         auto stream = res.stream;
         ts.add_worker(std::thread([stream, state] {
            std::vector<std::thread> threads;
            for (size_t s = 0; s < senders; ++s) {
               threads.emplace_back([stream, state, s] {
                  for (size_t i = 0; i < per_sender; ++i) {
                     stream->send_event("tick", make_payload(s, i), make_id(s, i), [state](std::error_code ec) {
                        if (ec) ++state->errors;
                        ++state->completed;
                     });
                  }
               });
            }
            for (auto& t : threads) t.join();
            stream->close();
         }));
      });
      expect(ts.start(4));

      const auto raw = fetch_raw(ts.port, "/events");
      const auto decoded = decode_chunked(raw);
      expect(decoded.error.empty()) << decoded.error;
      expect(decoded.terminated);

      std::vector<sse_event> events;
      std::string parse_error;
      expect(parse_events(decoded.body, events, parse_error)) << parse_error;
      expect_events(events, senders, per_sender);

      expect(wait_until([&] { return state->completed == senders * per_sender; }))
         << "completed " << state->completed.load();
      expect(state->errors == 0u);
   };

   "back_to_back_sends_then_close_deliver_everything"_test = [] {
      constexpr size_t count = 30;

      struct counters
      {
         std::atomic<size_t> completed{0};
         std::atomic<size_t> errors{0};
         std::atomic<bool> closed{false};
      };
      auto state = std::make_shared<counters>();

      test_server ts;
      ts.server.stream_get("/burst", [state](glz::request&, glz::streaming_response& res) {
         res.as_event_stream();
         // All queued from the handler without waiting for any to complete, then closed
         // while most of them are still waiting their turn.
         for (size_t i = 0; i < count; ++i) {
            res.stream->send_event("tick", make_payload(0, i), make_id(0, i), [state](std::error_code ec) {
               if (ec) ++state->errors;
               ++state->completed;
            });
         }
         res.stream->close([state] { state->closed = true; });
      });
      expect(ts.start(2));

      const auto raw = fetch_raw(ts.port, "/burst");
      const auto decoded = decode_chunked(raw);
      expect(decoded.error.empty()) << decoded.error;
      expect(decoded.terminated);

      std::vector<sse_event> events;
      std::string parse_error;
      expect(parse_events(decoded.body, events, parse_error)) << parse_error;
      expect_events(events, 1, count);

      expect(wait_until([&] { return state->completed == count; })) << "completed " << state->completed.load();
      expect(state->errors == 0u);
      expect(wait_until([&] { return state->closed.load(); }));
   };

   "client_disconnect_fails_queued_writes"_test = [] {
      // Far more than the socket buffers hold, so most are still queued when the client goes away
      constexpr size_t count = 200;

      struct counters
      {
         std::atomic<size_t> completed{0};
         std::atomic<size_t> errors{0};
         std::atomic<bool> all_queued{false};
      };
      auto state = std::make_shared<counters>();

      test_server ts;
      ts.server.stream_get("/large", [state](glz::request&, glz::streaming_response& res) {
         res.as_event_stream();
         res.stream->on_disconnect([] {});
         for (size_t i = 0; i < count; ++i) {
            res.stream->send_event("tick", make_payload(0, i), make_id(0, i), [state](std::error_code ec) {
               if (ec) ++state->errors;
               ++state->completed;
            });
         }
         state->all_queued = true;
      });
      expect(ts.start(2));

      {
         asio::io_context ctx;
         asio::ip::tcp::socket socket(ctx);
         socket.connect({asio::ip::make_address("127.0.0.1"), ts.port});
         const std::string request = "GET /large HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
         asio::write(socket, asio::buffer(request));
         // Only once every send is queued, take some of the stream, then reset the
         // connection mid-event
         expect(wait_until([&] { return state->all_queued.load(); }));
         std::string raw;
         asio::error_code ec;
         while (raw.size() < 1'000'000 && !ec) {
            std::array<char, 65536> buf;
            const auto n = socket.read_some(asio::buffer(buf), ec);
            raw.append(buf.data(), n);
         }
         socket.set_option(asio::socket_base::linger(true, 0), ec);
         socket.close(ec);
      }

      // Every queued send has its handler called, the ones that could not be sent with an error
      expect(wait_until([&] { return state->completed == count; })) << "completed " << state->completed.load();
      expect(state->errors > 0u);
   };

   "send_after_close_reports_error"_test = [] {
      struct counters
      {
         std::atomic<size_t> completed{0};
         std::atomic<size_t> errors{0};
      };
      auto state = std::make_shared<counters>();

      test_server ts;
      ts.server.stream_get("/closed", [state](glz::request&, glz::streaming_response& res) {
         res.as_event_stream();
         res.stream->send_event("tick", make_payload(0, 0), make_id(0, 0));
         res.stream->close();
         // Refused, since the stream has ended, but the handlers still run
         const auto on_sent = [state](std::error_code ec) {
            if (ec) ++state->errors;
            ++state->completed;
         };
         res.stream->send_event("tick", make_payload(0, 1), make_id(0, 1), on_sent);
         res.stream->send_chunk("late", on_sent);
      });
      expect(ts.start(2));

      const auto raw = fetch_raw(ts.port, "/closed");
      const auto decoded = decode_chunked(raw);
      expect(decoded.error.empty()) << decoded.error;
      expect(decoded.terminated);

      std::vector<sse_event> events;
      std::string parse_error;
      expect(parse_events(decoded.body, events, parse_error)) << parse_error;
      expect_events(events, 1, 1);

      expect(wait_until([&] { return state->completed == 2u; })) << "completed " << state->completed.load();
      expect(state->errors == 2u);
   };
};

int main() { return 0; }
