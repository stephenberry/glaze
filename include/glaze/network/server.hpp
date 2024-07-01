// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/network/socket.hpp"

#ifdef _WIN32
#define GLZ_CLOSE_SOCKET closesocket
#define GLZ_EVENT_CLOSE WSACloseEvent
#define GLZ_EWOULDBLOCK WSAEWOULDBLOCK
#define GLZ_INVALID_EVENT WSA_INVALID_EVENT
#define GLZ_INVALID_SOCKET INVALID_SOCKET
#define GLZ_SOCKET SOCKET
#define GLZ_SOCKET_ERROR SOCKET_ERROR
#define GLZ_SOCKET_ERROR_CODE WSAGetLastError()
#define GLZ_WAIT_FAILED WSA_WAIT_FAILED
#define GLZ_WAIT_RESULT_TYPE DWORD
#else
#include <sys/socket.h>
#include <sys/types.h>
#if __has_include(<netinet/in.h>)
#include <netinet/in.h>
#endif
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <unistd.h>

#include <cerrno>
#define GLZ_CLOSE_SOCKET ::close
#define GLZ_EVENT_CLOSE ::close
#define GLZ_EWOULDBLOCK EWOULDBLOCK
#define GLZ_INVALID_EVENT (-1)
#define GLZ_INVALID_SOCKET (-1)
#define GLZ_SOCKET int
#define GLZ_SOCKET_ERROR (-1)
#define GLZ_SOCKET_ERROR_CODE errno
#define GLZ_WAIT_FAILED (-1)
#define GLZ_WAIT_RESULT_TYPE int
#endif

#if defined(__APPLE__)
#include <sys/event.h> // for kqueue on macOS
#elif defined(__linux__)
#include <sys/epoll.h> // for epoll on Linux
#endif

namespace glz
{
   namespace detail
   {
      inline void server_thread_cleanup(auto& threads)
      {
         threads.erase(std::partition(threads.begin(), threads.end(),
                                      [](auto& future) {
                                         if (auto status = future.wait_for(std::chrono::milliseconds(0));
                                             status == std::future_status::ready) {
                                            return false;
                                         }
                                         return true;
                                      }),
                       threads.end());
      }
   }

   struct server final
   {
      int port{};
      std::atomic<bool> active = true;
      std::shared_future<std::error_code> async_accept_thread{};
      std::vector<std::future<void>> threads{};

      ~server() { active = false; }

      template <class AcceptCallback>
      std::shared_future<std::error_code> async_accept(AcceptCallback&& callback)
      {
         async_accept_thread = {
            std::async([this, callback = std::forward<AcceptCallback>(callback)] { return accept(callback); })};
         return async_accept_thread;
      }

      template <class AcceptCallback>
      [[nodiscard]] std::error_code accept(AcceptCallback&& callback)
      {
         glz::socket accept_socket{};

         const auto ec = bind_and_listen(accept_socket, port);
         if (ec) {
            return {int(ip_error::socket_bind_failed), ip_error_category::instance()};
         }

#if defined(__APPLE__)
         int event_fd = ::kqueue();
#elif defined(__linux__)
         int event_fd = ::epoll_create1(0);
#elif defined(_WIN32)
         HANDLE event_fd = WSACreateEvent();
#endif

         if (event_fd == GLZ_INVALID_EVENT) {
            return {int(ip_error::queue_create_failed), ip_error_category::instance()};
         }

         bool event_setup_failed = false;
#if defined(__APPLE__)
         struct kevent change;
         EV_SET(&change, accept_socket.socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
         event_setup_failed = ::kevent(event_fd, &change, 1, nullptr, 0, nullptr) == -1;
#elif defined(__linux__)
         struct epoll_event ev;
         ev.events = EPOLLIN;
         ev.data.fd = accept_socket.socket_fd;
         event_setup_failed = epoll_ctl(event_fd, EPOLL_CTL_ADD, accept_socket.socket_fd, &ev) == -1;
#elif defined(_WIN32)
         event_setup_failed = WSAEventSelect(accept_socket.socket_fd, event_fd, FD_ACCEPT) == GLZ_SOCKET_ERROR;
#endif

         if (event_setup_failed) {
            GLZ_EVENT_CLOSE(event_fd);
            return {int(ip_error::event_ctl_failed), ip_error_category::instance()};
         }

#if defined(__APPLE__)
         std::vector<struct kevent> events(16);
#elif defined(__linux__)
         std::vector<struct epoll_event> epoll_events(16);
#endif

         while (active) {
            GLZ_WAIT_RESULT_TYPE n{};

#if defined(__APPLE__)
            struct timespec timeout
            {
               0, 10000000
            }; // 10ms
            n = ::kevent(event_fd, nullptr, 0, events.data(), static_cast<int>(events.size()), &timeout);
#elif defined(__linux__)
            n = ::epoll_wait(event_fd, epoll_events.data(), static_cast<int>(epoll_events.size()), 10);
#elif defined(_WIN32)
            n = WSAWaitForMultipleEvents(1, &event_fd, FALSE, 10, FALSE);
#endif

            if (n == GLZ_WAIT_FAILED) {
#if defined(__APPLE__) || defined(__linux__)
               if (errno == EINTR) continue;
#else
               if (n == WSA_WAIT_TIMEOUT) continue;
#endif
               GLZ_EVENT_CLOSE(event_fd);
               return {int(ip_error::event_wait_failed), ip_error_category::instance()};
            }

            auto spawn_socket = [&] {
               sockaddr_in client_addr;
               socklen_t client_len = sizeof(client_addr);
               auto client_fd = ::accept(accept_socket.socket_fd, (sockaddr*)&client_addr, &client_len);
               if (client_fd != GLZ_INVALID_SOCKET) {
                  threads.emplace_back(
                     std::async([this, callback, client_fd] { callback(socket{client_fd}, active); }));
               }
            };

#if defined(__APPLE__) || defined(__linux__)
            for (int i = 0; i < n; ++i) {
#if defined(__APPLE__)
               if (events[i].ident == uintptr_t(accept_socket.socket_fd) && events[i].filter == EVFILT_READ) {
#elif defined(__linux__)
               if (epoll_events[i].data.fd == accept_socket.socket_fd && epoll_events[i].events & EPOLLIN) {
#endif
                  spawn_socket();
               }
            }

#else // Windows
            WSANETWORKEVENTS events;
            if (WSAEnumNetworkEvents(accept_socket.socket_fd, event_fd, &events) == GLZ_SOCKET_ERROR) {

               WSACloseEvent(event_fd);

               // requires explicit 'std::error_code'...otherwise the following error with msvc...
               // 
               // error C2440: 'return': cannot convert from 'initializer list' to 'std::error_code'
               // 
               return {int(ip_error::event_enum_failed), ip_error_category::instance()};
            }

            if (events.lNetworkEvents & FD_ACCEPT) {
               if (events.iErrorCode[FD_ACCEPT_BIT] == 0) {
                  spawn_socket();
               }
            }
#endif

            detail::server_thread_cleanup(threads);
         }

         GLZ_EVENT_CLOSE(event_fd);
         return {};
      }
   };
}

#undef GLZ_CLOSE_SOCKET
#undef GLZ_EVENT_CLOSE
#undef GLZ_EWOULDBLOCK
#undef GLZ_INVALID_EVENT
#undef GLZ_INVALID_SOCKET
#undef GLZ_SOCKET
#undef GLZ_SOCKET_ERROR
#undef GLZ_SOCKET_ERROR_CODE
#undef GLZ_WAIT_FAILED
#undef GLZ_WAIT_RESULT_TYPE
