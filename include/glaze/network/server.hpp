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
         sockaddr_in client_addr{};
         client_addr.sin_family = AF_INET; // Use AF_INET for IPv4
         client_addr.sin_port = htons(port);

         if (::inet_pton(AF_INET, address.c_str(), &client_addr.sin_addr) <= 0) {
            std::cerr << "Invalid address/ Address not supported: " << inet_ntoa(client_addr.sin_addr) << ":"
                      << ntohs(client_addr.sin_port) << '\n';
            return {};
         }

         std::ostringstream sockaddr;
         sockaddr << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << '\n';
         std::cout << "Accepting incoming client connection to: " << sockaddr.str();

         constexpr int len = sizeof(struct sockaddr_in);

         auto new_client_id = ::accept(accept_socket->socket_fd, (struct sockaddr*)(&client_addr),
                                       const_cast<socklen_t*>((const socklen_t*)(&len)));

         if (new_client_id < 0) {
            std::cerr << "Unable to accept client on socket address " << sockaddr.str() << '\n';
            //
            // TODO: Handle Error
            //
            return {};
         }
         socket sock{new_client_id};

         std::cout << "New Client Id, " << new_client_id << ", " << "Accepted On " << sockaddr.str();

         std::string_view ip_addr_view{sockaddr.str()};

         // clang-format off
         return { .scheduler = scheduler, 
                  .address = to_ip_string(ip_addr_view).value(), 
                  .port = ntohs(client_addr.sin_port),
                  .ipv = ip_version(client_addr.sin_family)};
         // clang-format on
      }
   };
}
