// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/coroutine/io_scheduler.hpp"
#include "glaze/network/socket.hpp"

namespace glz
{
   /**
    * By default the socket
    * created will be in non-blocking mode, meaning that any sending or receiving of data should
    * poll for event readiness prior.
    */
   struct client
   {
      std::string address{"127.0.0.1"};
      uint16_t port{8080};
      std::shared_ptr<glz::io_scheduler> scheduler{};
      ip_version ipv{};
      glz::socket socket{};
      /// Cache the status of the connect in the event the user calls connect() again.
      std::optional<net::connect_status> m_connect_status{};

      /**
       * Connects to the address+port with the given timeout.  Once connected calling this function
       * only returns the connected status, it will not reconnect.
       * @param timeout How long to wait for the connection to establish? Timeout of zero is indefinite.
       * @return The result status of trying to connect.
       */
      coro::task<std::error_code> connect(std::chrono::milliseconds timeout = std::chrono::milliseconds{0})
      {
         // Only allow the user to connect per tcp client once, if they need to re-connect they should
         // make a new tcp::client.
         if (m_connect_status.has_value()) {
            co_return m_connect_status.value();
         }

         // This enforces the connection status is aways set on the client object upon returning.
         auto return_value = [this](connect_status s) -> connect_status {
            m_connect_status = s;
            return s;
         };

         sockaddr_in server_addr;
         server_addr.sin_family = int(ipv);
         server_addr.sin_port = htons(port);
         ::inet_pton(int(ipv), address.c_str(), &server_addr.sin_addr);

         auto result = ::connect(socket.socket_fd, (sockaddr*)&server_addr, sizeof(server_addr));
         if (result == 0) {
            co_return return_value(connect_status::connected);
         }
         else if (result == -1) {
            // If the connect is happening in the background poll for write on the socket to trigger
            // when the connection is established.
            if (errno == EAGAIN || errno == EINPROGRESS) {
               auto pstatus = co_await io_scheduler->poll(m_socket, poll_op::write, timeout);
               if (pstatus == poll_status::event) {
                  int result{0};
                  socklen_t result_length{sizeof(result)};
                  if (getsockopt(m_socket.native_handle(), SOL_SOCKET, SO_ERROR, &result, &result_length) < 0) {
                     std::cerr << "connect failed to getsockopt after write poll event\n";
                  }

                  if (result == 0) {
                     co_return return_value(connect_status::connected);
                  }
               }
               else if (pstatus == poll_status::timeout) {
                  co_return return_value(connect_status::timeout);
               }
            }
         }

         co_return return_value(connect_status::error);
      }

      /**
       * Polls for the given operation on this client's tcp socket.  This should be done prior to
       * calling recv and after a send that doesn't send the entire buffer.
       * @param op The poll operation to perform, use read for incoming data and write for outgoing.
       * @param timeout The amount of time to wait for the poll event to be ready.  Use zero for infinte timeout.
       * @return The status result of th poll operation.  When poll_status::event is returned then the
       *         event operation is ready.
       */
      auto poll(coro::poll_op op, std::chrono::milliseconds timeout = std::chrono::milliseconds{0})
         -> coro::task<poll_status>
      {
         return m_io_scheduler->poll(m_socket, op, timeout);
      }

      /**
       * Receives incoming data into the given buffer.  By default since all tcp client sockets are set
       * to non-blocking use co_await poll() to determine when data is ready to be received.
       * @param buffer Received bytes are written into this buffer up to the buffers size.
       * @return The status of the recv call and a span of the bytes recevied (if any).  The span of
       *         bytes will be a subspan or full span of the given input buffer.
       */
      template <concepts::mutable_buffer buffer_type>
      auto recv(buffer_type&& buffer) -> std::pair<recv_status, std::span<char>>
      {
         // If the user requested zero bytes, just return.
         if (buffer.empty()) {
            return {recv_status::ok, std::span<char>{}};
         }

         auto bytes_recv = ::recv(m_socket.native_handle(), buffer.data(), buffer.size(), 0);
         if (bytes_recv > 0) {
            // Ok, we've recieved some data.
            return {recv_status::ok, std::span<char>{buffer.data(), static_cast<size_t>(bytes_recv)}};
         }
         else if (bytes_recv == 0) {
            // On TCP stream sockets 0 indicates the connection has been closed by the peer.
            return {recv_status::closed, std::span<char>{}};
         }
         else {
            // Report the error to the user.
            return {static_cast<recv_status>(errno), std::span<char>{}};
         }
      }

      /**
       * Sends outgoing data from the given buffer.  If a partial write occurs then use co_await poll()
       * to determine when the tcp client socket is ready to be written to again.  On partial writes
       * the status will be 'ok' and the span returned will be non-empty, it will contain the buffer
       * span data that was not written to the client's socket.
       * @param buffer The data to write on the tcp socket.
       * @return The status of the send call and a span of any remaining bytes not sent.  If all bytes
       *         were successfully sent the status will be 'ok' and the remaining span will be empty.
       */
      template <concepts::const_buffer buffer_type>
      auto send(const buffer_type& buffer) -> std::pair<send_status, std::span<const char>>
      {
         // If the user requested zero bytes, just return.
         if (buffer.empty()) {
            return {send_status::ok, std::span<const char>{buffer.data(), buffer.size()}};
         }

         auto bytes_sent = ::send(m_socket.native_handle(), buffer.data(), buffer.size(), 0);
         if (bytes_sent >= 0) {
            // Some or all of the bytes were written.
            return {send_status::ok, std::span<const char>{buffer.data() + bytes_sent, buffer.size() - bytes_sent}};
         }
         else {
            // Due to the error none of the bytes were written.
            return {static_cast<send_status>(errno), std::span<const char>{buffer.data(), buffer.size()}};
         }
      }
   };
}
