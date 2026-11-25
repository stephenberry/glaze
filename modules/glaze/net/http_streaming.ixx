// Glaze Library
// For the license information refer to glaze.ixx
module;
#include "glaze/ext/asio_include.hpp"

export module glaze.net.http_streaming;

import std;

import glaze.json.write;

namespace glz
{
   // Interface for streaming connections (type-erased for HTTP/HTTPS compatibility)
   export struct streaming_connection_interface
   {
      using data_sent_handler = std::function<void(std::error_code)>;
      using disconnect_handler = std::function<void()>;

      virtual ~streaming_connection_interface() = default;

      // Send initial headers for streaming response
      virtual void send_headers(int status_code, const std::unordered_map<std::string, std::string>& headers = {},
                                data_sent_handler handler = {}) = 0;

      // Send a chunk of data
      virtual void send_chunk(std::string_view data, data_sent_handler handler = {}) = 0;

      // Send Server-Sent Event
      virtual void send_event(std::string_view event_type, std::string_view data, std::string_view id = {},
                              data_sent_handler handler = {}) = 0;

      // Close the streaming connection
      virtual void close(disconnect_handler handler = {}) = 0;

      // Set disconnect handler for client disconnection
      virtual void on_disconnect(disconnect_handler handler) = 0;

      // Check if connection is still alive
      virtual bool is_open() const = 0;

      // Get remote endpoint info
      virtual std::string remote_address() const = 0;
      virtual std::uint16_t remote_port() const = 0;

      // Check if headers have been sent
      virtual bool is_headers_sent() const = 0;

      // Get executor for async operations (timers, etc.)
      virtual asio::any_io_executor get_executor() const = 0;

      // Send JSON as Server-Sent Event (convenience method using send_event)
      template <class T>
      void send_json_event(const T& data, std::string_view event_type = "message", std::string_view id = {},
                           data_sent_handler handler = {})
      {
         std::string json_str;
         auto ec = glz::write_json(data, json_str);
         if (!ec) {
            send_event(event_type, json_str, id, handler);
         }
         else if (handler) {
            handler(std::make_error_code(std::errc::invalid_argument));
         }
      }
   };

   // Enhanced response class with streaming support
   export struct streaming_response
   {
      std::shared_ptr<streaming_connection_interface> stream;

      streaming_response(std::shared_ptr<streaming_connection_interface> conn) : stream(std::move(conn)) {}

      // Send headers and start streaming
      streaming_response& start_stream(int status_code = 200,
                                       const std::unordered_map<std::string, std::string>& headers = {})
      {
         if (stream) {
            stream->send_headers(status_code, headers);
         }
         return *this;
      }

      // Send a chunk of data
      streaming_response& send(std::string_view data)
      {
         if (stream) {
            stream->send_chunk(data);
         }
         return *this;
      }

      // Send JSON data
      template <class T>
      streaming_response& send_json(const T& data)
      {
         if (stream) {
            std::string json_str;
            auto ec = glz::write_json(data, json_str);
            if (!ec) {
               stream->send_chunk(json_str);
            }
         }
         return *this;
      }

      // Send Server-Sent Event
      streaming_response& send_event(std::string_view event_type, std::string_view data, std::string_view id = {})
      {
         if (stream) {
            stream->send_event(event_type, data, id);
         }
         return *this;
      }

      // Helper for SSE setup
      streaming_response& as_event_stream()
      {
         return start_stream(200, {{"Content-Type", "text/event-stream"},
                                   {"Cache-Control", "no-cache"},
                                   {"Access-Control-Allow-Origin", "*"}});
      }

      // Close the stream
      void close()
      {
         if (stream) {
            stream->close();
         }
      }
   };
}
