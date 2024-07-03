// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/coroutine/scheduler.hpp"
#include "glaze/network/ip.hpp"
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
      std::shared_ptr<glz::scheduler> scheduler{};
      std::string address{"127.0.0.1"};
      uint16_t port{8080};
      ip_version ipv{};
      std::shared_ptr<glz::socket> socket = make_aysync_socket();
      /// Cache the status of the connect in the event the user calls connect() again.
      glz::ip_status connect_status{};

      /**
       * Connects to the address+port with the given timeout.  Once connected calling this function
       * only returns the connected status, it will not reconnect.
       * @param timeout How long to wait for the connection to establish? Timeout of zero is indefinite.
       * @return The result status of trying to connect.
       */
      task<glz::ip_status> connect(std::chrono::milliseconds timeout = std::chrono::milliseconds{0})
      {
         // Only allow the user to connect per tcp client once, if they need to re-connect they should
         // make a new tcp::client.
         if (connect_status != glz::ip_status::unset) {
            co_return connect_status;
         }

         // This enforces the connection status is aways set on the client object upon returning.
         auto return_value = [this](glz::ip_status s) -> glz::ip_status {
            connect_status = s;
            return s;
         };

         sockaddr_in server_addr;
         server_addr.sin_family = glz::net::asize_t(ipv);
         server_addr.sin_port = htons(port);
         ::inet_pton(glz::net::asize_t(ipv), address.c_str(), &server_addr.sin_addr);
         
         if (socket->socket_fd == net::invalid_socket) {
            co_return return_value(ip_status::invalid_socket);
         }

         auto result = ::connect(socket->socket_fd, (sockaddr*)&server_addr, sizeof(server_addr));
         if (result == 0) {
            co_return return_value(ip_status::connected);
         }
         else if (result == -1) {
            // If the connect is happening in the background, poll for write on the socket to trigger
            // when the connection is established.
            if (errno == EAGAIN || errno == EINPROGRESS) {
               auto pstatus = co_await scheduler->poll(socket->socket_fd, poll_op::write, timeout);
               if (pstatus == poll_status::event) {
                  result = 0;
                  socklen_t result_length{sizeof(result)};

                  #if defined(_WIN32)
                  if (getsockopt(socket.socket_fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&result),
                     #else
                  if (getsockopt(socket->socket_fd, SOL_SOCKET, SO_ERROR, &result,
                  #endif

                                 &result_length) < 0) {
                     std::cerr << "connect failed to getsockopt after write poll event\n";
                  }

                  if (result == 0) {
                     co_return return_value(ip_status::connected);
                  }
               }
               else if (pstatus == poll_status::timeout) {
                  co_return return_value(ip_status::timeout);
               }
            }
         }

         std::cerr << "connect: " << get_socket_error_message(errno) << '\n';
         co_return return_value(ip_status::error);
      }

      /**
       * Polls for the given operation on this client's tcp socket.  This should be done prior to
       * calling recv and after a send that doesn't send the entire buffer.
       * @param op The poll operation to perform, use read for incoming data and write for outgoing.
       * @param timeout The amount of time to wait for the poll event to be ready.  Use zero for infinte timeout.
       * @return The status result of th poll operation.  When poll_status::event is returned then the
       *         event operation is ready.
       */
      task<poll_status> poll(glz::poll_op op, std::chrono::milliseconds timeout = std::chrono::milliseconds{0})
      {
         return scheduler->poll(socket->socket_fd, op, timeout);
      }

      /**
       * Receives incoming data into the given buffer.  By default since all tcp client sockets are set
       * to non-blocking use co_await poll() to determine when data is ready to be received.
       * @param buffer Received bytes are written into this buffer up to the buffers size.
       * @return The status of the recv call and a span of the bytes recevied (if any).  The span of
       *         bytes will be a subspan or full span of the given input buffer.
       */
      template <mutable_buffer Buffer>
      std::pair<ip_status, std::span<char>> recv(Buffer&& buffer)
      {
         // If the user requested zero bytes, just return.
         if (buffer.empty()) {
            return {ip_status::ok, std::span<char>{}};
         }

         auto bytes_recv = ::recv(socket->socket_fd, buffer.data(), glz::net::ssize_t(buffer.size()), 0);
         if (bytes_recv > 0) {
            // Ok, we've recieved some data.
            return {ip_status::ok, std::span<char>{buffer.data(), size_t(bytes_recv)}};
         }
         else if (bytes_recv == 0) {
            // On TCP stream sockets 0 indicates the connection has been closed by the peer.
            return {ip_status::closed, std::span<char>{}};
         }
         else {
            // Report the error to the user.
            return {errno_to_ip_status(), std::span<char>{}};
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
      template <const_buffer Buffer>
      std::pair<ip_status, std::string_view> send(const Buffer& buffer)
      {
         // If the user requested zero bytes, just return.
         if (buffer.empty()) {
            return {ip_status::ok, std::string_view{buffer.data(), buffer.size()}};
         }

         auto bytes_sent = ::send(socket->socket_fd, buffer.data(), glz::net::ssize_t(buffer.size()), 0);
         if (bytes_sent >= 0) {
            // Some or all of the bytes were written.
            return {ip_status::ok, std::string_view{buffer.data() + bytes_sent, buffer.size() - bytes_sent}};
         }
         else {
            // Due to the error none of the bytes were written.
            return {errno_to_ip_status(), std::string_view{buffer.data(), buffer.size()}};
         }
      }
   };
}
