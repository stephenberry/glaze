// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#include "glaze/coroutine/poll.hpp"
#include "glaze/network/ip_address.hpp"

namespace glz
{
   struct socket
   {
      enum struct type {
         /// udp datagram socket
         udp,
         /// tcp streaming socket
         tcp
      };

      enum struct blocking {
         /// This socket should block on system calls.
         yes,
         /// This socket should not block on system calls.
         no
      };

      struct options
      {
         /// The domain for the socket.
         ip_version domain;
         /// The type of socket.
         socket::type type;
         /// If the socket should be blocking or non-blocking.
         socket::blocking blocking;
      };

      static int type_to_os(socket::type type)
      {
          switch (type)
          {
              case type::udp:
                  return SOCK_DGRAM;
              case type::tcp:
                  return SOCK_STREAM;
              default:
                  GLZ_THROW_OR_ABORT(std::runtime_error{"Unknown socket::type_t."});
          }
      }

      socket() = default;
      explicit socket(int fd) : m_fd(fd) {}

      socket(const socket& other) : m_fd(dup(other.m_fd)) {}
      socket(socket&& other) : m_fd(std::exchange(other.m_fd, -1)) {}
      
      socket& operator=(const socket& other) noexcept
      {
          this->close();
          this->m_fd = dup(other.m_fd);
          return *this;
      }
      
      socket& operator=(socket&& other) noexcept
      {
          if (std::addressof(other) != this)
          {
              m_fd = std::exchange(other.m_fd, -1);
          }

          return *this;
      }

      ~socket() { close(); }

      /**
       * This function returns true if the socket's file descriptor is a valid number, however it does
       * not imply if the socket is still usable.
       * @return True if the socket file descriptor is > 0.
       */
      bool is_valid() const { return m_fd != -1; }

      /**
       * @param block Sets the socket to the given blocking mode.
       */
      bool blocking(socket::blocking block)
      {
          if (m_fd < 0)
          {
              return false;
          }

          int flags = fcntl(m_fd, F_GETFL, 0);
          if (flags == -1)
          {
              return false;
          }

          // Add or subtract non-blocking flag.
          flags = (block == blocking::yes) ? flags & ~O_NONBLOCK : (flags | O_NONBLOCK);

          return (fcntl(m_fd, F_SETFL, flags) == 0);
      }

      /**
       * @param how Shuts the socket down with the given operations.
       * @param Returns true if the sockets given operations were shutdown.
       */
      bool shutdown(poll_op how = poll_op::read_write)
      {
          if (m_fd != -1)
          {
              int h{0};
              switch (how)
              {
                  case poll_op::read:
                      h = SHUT_RD;
                      break;
                  case poll_op::write:
                      h = SHUT_WR;
                      break;
                  case poll_op::read_write:
                      h = SHUT_RDWR;
                      break;
              }

              return (::shutdown(m_fd, h) == 0);
          }
          return false;
      }

      /**
       * Closes the socket and sets this socket to an invalid state.
       */
      void close()
      {
          if (m_fd != -1)
          {
              ::close(m_fd);
              m_fd = -1;
          }
      }

      /**
       * @return The native handle (file descriptor) for this socket.
       */
      int native_handle() const { return m_fd; }

     private:
      int m_fd{-1};
   };

   /**
    * Creates a socket with the given socket options, this typically is used for creating sockets to
    * use within client objects, e.g. tcp::client and udp::client.
    * @param opts See socket::options for more details.
    */
   socket make_socket(const socket::options& opts)
   {
       socket s{::socket(static_cast<int>(opts.domain), socket::type_to_os(opts.type), 0)};
       if (s.native_handle() < 0)
       {
          GLZ_THROW_OR_ABORT(std::runtime_error{"Failed to create socket."});
       }

       if (opts.blocking == socket::blocking::no)
       {
           if (s.blocking(socket::blocking::no) == false)
           {
              GLZ_THROW_OR_ABORT(std::runtime_error{"Failed to set socket to non-blocking mode."});
           }
       }

       return s;
   }

   /**
    * Creates a socket that can accept connections or packets with the given socket options, address,
    * port and backlog.  This is used for creating sockets to use within server objects, e.g.
    * tcp::server and udp::server.
    * @param opts See socket::options for more details
    * @param address The ip address to bind to.  If the type of socket is tcp then it will also listen.
    * @param port The port to bind to.
    * @param backlog If the type of socket is tcp then the backlog of connections to allow.  Does nothing
    *                for udp types.
    */
   socket make_accept_socket(const socket::options& opts, const std::string& address, uint16_t port,
                           int32_t backlog = 128)
   {
       socket s = make_socket(opts);

       int sock_opt{1};
       if (setsockopt(s.native_handle(), SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &sock_opt, sizeof(sock_opt)) < 0)
       {
          GLZ_THROW_OR_ABORT(std::runtime_error{"Failed to setsockopt(SO_REUSEADDR | SO_REUSEPORT)"});
       }

       sockaddr_in server{};
       server.sin_family = static_cast<int>(opts.domain);
       server.sin_port   = htons(port);
       server.sin_addr   = *reinterpret_cast<const in_addr*>(address.data());

       if (bind(s.native_handle(), (struct sockaddr*)&server, sizeof(server)) < 0)
       {
          GLZ_THROW_OR_ABORT(std::runtime_error{"Failed to bind."});
       }

       if (opts.type == socket::type::tcp)
       {
           if (listen(s.native_handle(), backlog) < 0)
           {
              GLZ_THROW_OR_ABORT(std::runtime_error{"Failed to listen."});
           }
       }

       return s;
   }
}
