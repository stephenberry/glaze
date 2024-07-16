// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>

#ifndef GLZ_THROW_OR_ABORT
#if __cpp_exceptions
#define GLZ_THROW_OR_ABORT(EXC) (throw(EXC))
#include <exception>
#else
#define GLZ_THROW_OR_ABORT(EXC) (std::abort())
#endif
#endif

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <ctime>
#include <time.h>
#endif

#if __has_include(<netinet/in.h>)
#include <netinet/in.h>
#endif

#if defined(__APPLE__)
#include <sys/event.h> // for kqueue on macOS
#elif defined(__linux__)
#include <sys/epoll.h> // for epoll on Linux
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#endif

namespace glz::net
{
#ifdef _WIN32
   using event_handle_t = HANDLE;
   using ssize_t = int32_t;
   using asize_t = uint8_t;
#else
   using event_handle_t = int;
   using ssize_t = ::ssize_t;
   using asize_t = int;
#endif
   
#if defined(__APPLE__)
   constexpr auto invalid_socket = -1;
   using poll_event_t = struct kevent;
   constexpr int invalid_event_handle = -1;
   using ident_t = uintptr_t;
   constexpr uintptr_t invalid_ident = ~uintptr_t(0); // set all bits
#elif defined(__linux__)
   constexpr auto invalid_socket = -1;
   using poll_event_t = struct epoll_event;
   constexpr int invalid_event_handle = -1;
   using ident_t = int;
   constexpr int invalid_ident = -1;
#elif defined(_WIN32)
   constexpr auto invalid_socket = INVALID_SOCKET;
   using ident_t = HANDLE;
   using poll_event_t = HANDLE;
   inline const HANDLE invalid_event_handle = INVALID_HANDLE_VALUE;
   inline const HANDLE invalid_ident = INVALID_HANDLE_VALUE;
#endif
   
#if defined(__APPLE__)
   constexpr auto poll_in = EVFILT_READ;
   constexpr auto poll_out = EVFILT_WRITE;
#elif defined(__linux__)
   constexpr auto poll_in = EPOLLIN;
   constexpr auto poll_out = EPOLLOUT;
#elif defined(_WIN32)
   constexpr auto poll_in = 0;
   constexpr auto poll_out = 1;
#endif


   inline auto close_socket(auto& fd) {
      if (fd != invalid_socket) {
#ifdef _WIN32
      ::closesocket(fd);
#else
      ::close(fd);
#endif
      }
      fd = invalid_socket;
   }
   
   inline auto close_event(auto fd) {
#ifdef _WIN32
      ::CloseHandle(fd);
#else
      ::close(fd);
#endif
   }
   
   inline event_handle_t create_event_poll() {
#if defined(__APPLE__)
         return ::kqueue();
#elif defined(__linux__)
         return ::epoll_create1(EPOLL_CLOEXEC);
#elif defined(_WIN32)
      return INVALID_HANDLE_VALUE;
#endif
   }
   
   inline event_handle_t create_shutdown_handle() {
#if defined(__APPLE__)
      return invalid_event_handle;
#elif defined(__linux__)
      return ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
#elif defined(_WIN32)
      return CreateEventA(nullptr, TRUE, FALSE, "create_shutdown_handle");
#endif
   }
   
   inline event_handle_t create_timer_handle() {
#if defined(__APPLE__)
      return invalid_event_handle;
#elif defined(__linux__)
      return ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
#elif defined(_WIN32)
      return CreateWaitableTimerA(nullptr, TRUE, "create_timer_handle");
#endif
   }
   
   inline event_handle_t create_schedule_handle() {
#if defined(__APPLE__)
      return invalid_event_handle;
#elif defined(__linux__)
      return ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
#elif defined(_WIN32)
      return CreateEventA(nullptr, TRUE, FALSE, "create_schedule_handle");
#endif
   }
   
   inline bool poll_error([[maybe_unused]] uint32_t events) {
#if defined(__APPLE__)
      return events & EV_ERROR;
#elif defined(__linux__)
      return events & EPOLLERR;
#elif defined(_WIN32)
      return true;
#endif
   }
   
   inline bool event_closed([[maybe_unused]] uint32_t events) {
#if defined(__APPLE__)
      return events & EV_EOF;
#elif defined(__linux__)
      return events & EPOLLRDHUP || events & EPOLLHUP;
#elif defined(_WIN32)
      return true;
#endif
   }
}
