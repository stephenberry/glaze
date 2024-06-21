// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/network/server.hpp"
#include "glaze/rpc/repe.hpp"

namespace glz
{
   template <opts Opts = opts{}>
   struct repe_server
   {
      uint16_t port{};
      glz::server server{};

      struct glaze
      {
         using T = repe_server;
         static constexpr auto value = glz::object(&T::port);
      };

      repe::registry<Opts> registry{};

      void clear_registry() { registry.clear(); }

      template <const std::string_view& Root = repe::detail::empty_path, class T>
         requires(glz::detail::glaze_object_t<T> || glz::detail::reflectable<T>)
      void on(T& value)
      {
         registry.template on<Root>(value);
      }

      void run()
      {
         server.port = port;
         
         auto ec = server.accept([this](socket&& socket, auto& active) {
            // TODO: handle potential error
            std::ignore = socket.no_delay();
            
            std::string buffer{};

            try {
               while (active) {
                  if (auto ec = receive(socket, buffer)) {
                     std::fprintf(stderr, "%s\n", ec.message().c_str());
                     if (ec.value() == ip_error::client_disconnected) {
                        return;
                     }
                  }
                  else {
                     auto response = registry.call(buffer);
                     if (response) {
                        if (auto ec = send(socket, response->value())) {
                           std::fprintf(stderr, "%s\n", ec.message().c_str());
                        }
                     }
                  }
               }
            }
            catch (const std::exception& e) {
               std::fprintf(stderr, "%s\n", e.what());
            }
         });
         
         if (ec) {
            std::fprintf(stderr, "%s\n", ec.message().c_str());
         }
      }

      // stop the server
      void stop()
      {
         server.active = false;
      }
   };
}
