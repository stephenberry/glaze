// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/coroutine/task.hpp"
#include "glaze/network/client.hpp"
#include "glaze/network/ip.hpp"
#include "glaze/network/socket.hpp"

namespace glz
{
   struct server
   {
      std::shared_ptr<glz::scheduler> scheduler{};
      std::string address{"127.0.0.1"};
      uint16_t port{8080};
      int32_t backlog{128}; // The kernel backlog of connections to buffer.
      /// The socket for accepting new tcp connections on.
      std::shared_ptr<socket> accept_socket = make_accept_socket(address, port);

      /**
       * Polls for new incoming tcp connections.
       * @param timeout How long to wait for a new connection before timing out, zero waits indefinitely.
       * @return The result of the poll, 'event' means the poll was successful and there is at least 1
       *         connection ready to be accepted.
       */
      task<poll_status> poll(std::chrono::milliseconds timeout = std::chrono::milliseconds{0})
      {
         return scheduler->poll(accept_socket->socket_fd, poll_op::read, timeout);
      }

      /**
       * Accepts an incoming tcp client connection.  On failure the tls clients socket will be set to
       * and invalid state, use the socket.is_value() to verify the client was correctly accepted.
       * @return The newly connected tcp client connection.
       */
      client accept()
      {
         sockaddr_in client{};
         constexpr int len = sizeof(struct sockaddr_in);
         socket sock{::accept(accept_socket->socket_fd, (struct sockaddr*)(&client),
                           const_cast<socklen_t*>((const socklen_t*)(&len)))};
         
         std::string_view ip_addr_view{
            (char*)(&client.sin_addr.s_addr),
            sizeof(client.sin_addr.s_addr),
         };
         
         return {scheduler, binary_to_ip_string(ip_addr_view).value(), ntohs(client.sin_port), ip_version(client.sin_family)};
      }
   };
}
