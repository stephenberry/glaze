// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

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
   using file_handle_t = unsigned int;
   constexpr unsigned int invalid_file_handle = 0;
   using ssize_t = int64_t;
#else
   using file_handle_t = int;
   constexpr int invalid_file_handle = -1;
#endif
   
#if defined(__APPLE__)
   using poll_event_t = struct kevent;
#elif defined(__linux__)
   using poll_event_t = struct epoll_event;
#elif defined(_WIN32)
#endif
   
#if defined(__APPLE__)
   constexpr auto poll_in = EVFILT_READ;
   constexpr auto poll_out = EVFILT_WRITE;
#elif defined(__linux__)
   constexpr auto poll_in = EPOLLIN;
   constexpr auto poll_out = EPOLLOUT;
#elif defined(_WIN32)
#endif
   
   inline auto close_socket(file_handle_t fd) {
#ifdef _WIN32
      ::closesocket(fd);
#else
      ::close(fd);
#endif
   }
   
   inline auto event_close(file_handle_t fd) {
#ifdef _WIN32
      WSACloseEvent(fd);
#else
      ::close(fd);
#endif
   }
   
   inline file_handle_t create_event_poll() {
#if defined(__APPLE__)
         return ::kqueue();
#elif defined(__linux__)
         return ::epoll_create1(EPOLL_CLOEXEC);
#elif defined(_WIN32)
         return WSACreateEvent();
#endif
   }
   
#if defined(__APPLE__)
   inline file_handle_t create_user_kqueue_handle() {
      int kq = kqueue();
       if (kq == -1) {
          GLZ_THROW_OR_ABORT(std::runtime_error("Failed to create kqueue"));
       }

       struct kevent kev;
       EV_SET(&kev, 1, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, NULL);

       if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1) {
           close(kq);
          GLZ_THROW_OR_ABORT( std::runtime_error("Failed to add EVFILT_USER event to kqueue"));
       }

       return kq;
   }
   
   inline void trigger_user_kqueue(file_handle_t fd)
   {
      struct kevent kev;
       EV_SET(&kev, 1, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);

       if (kevent(fd, &kev, 1, NULL, 0, NULL) == -1) {
          GLZ_THROW_OR_ABORT(std::runtime_error("Failed to signal shutdown event"));
       }
   }
#endif
   
   inline file_handle_t create_shutdown_handle() {
#if defined(__APPLE__)
      return create_user_kqueue_handle();
#elif defined(__linux__)
      return ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
#elif defined(_WIN32)
      return 0;
#endif
   }
   
   inline file_handle_t create_timer_handle() {
#if defined(__APPLE__)
      return create_user_kqueue_handle();
#elif defined(__linux__)
      return ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
#elif defined(_WIN32)
      return 0;
#endif
   }
   
   inline file_handle_t create_schedule_handle() {
#if defined(__APPLE__)
      return create_user_kqueue_handle();
#elif defined(__linux__)
      return ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
#elif defined(_WIN32)
      return 0;
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
