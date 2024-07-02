#pragma once

#include "server.hpp"

namespace glz
{

   class client
   {
     public:
      struct options
      {
         /// The  ip address to connect to.  Use a dns_resolver to turn hostnames into ip addresses.
         glz::net::ip_address address{net::ip_address::from_string("127.0.0.1")};
         /// The port to connect to.
         uint16_t port{8080};
      };

      /**
       * Creates a new tcp client that can connect to an ip address + port.  By default the socket
       * created will be in non-blocking mode, meaning that any sending or receiving of data should
       * poll for event readiness prior.
       * @param scheduler The io scheduler to drive the tcp client.
       * @param opts See client::options for more information.
       */
      explicit client(std::shared_ptr<io_scheduler> scheduler,
                      options opts = options{
                         .address = {net::ip_address::from_string("127.0.0.1")},
                         .port = 8080,
                      });
      client(const client& other);
      client(client&& other);
      auto operator=(const client& other) noexcept -> client&;
      auto operator=(client&& other) noexcept -> client&;
      ~client();

      /**
       * @return The tcp socket this client is using.
       * @{
       **/
      auto socket() -> net::socket& { return m_socket; }
      auto socket() const -> const net::socket& { return m_socket; }
      /** @} */

      /**
       * Connects to the address+port with the given timeout.  Once connected calling this function
       * only returns the connected status, it will not reconnect.
       * @param timeout How long to wait for the connection to establish? Timeout of zero is indefinite.
       * @return The result status of trying to connect.
       */
      auto connect(std::chrono::milliseconds timeout = std::chrono::milliseconds{0}) -> coro::task<net::connect_status>;

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

     private:
      /// The tcp::server creates already connected clients and provides a tcp socket pre-built.
      friend server;
      client(std::shared_ptr<io_scheduler> scheduler, net::socket socket, options opts);

      /// The scheduler that will drive this tcp client.
      std::shared_ptr<io_scheduler> m_io_scheduler{nullptr};
      /// Options for what server to connect to.
      options m_options{};
      /// The tcp socket.
      net::socket m_socket{-1};
      /// Cache the status of the connect in the event the user calls connect() again.
      std::optional<net::connect_status> m_connect_status{std::nullopt};
   };

} // namespace glz
