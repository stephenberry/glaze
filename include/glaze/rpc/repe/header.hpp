// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace glz::repe
{
   // read/write are from the client's perspective
   enum struct action : uint32_t {
      notify = 1 << 0, // If this message does not require a response
      read = 1 << 1, // Read a value or return the result of invoking a function
      write = 1 << 2 // Write a value or invoke a function
   };

   inline constexpr auto no_length_provided = (std::numeric_limits<uint64_t>::max)();

   struct header
   {
      uint64_t length{no_length_provided}; // Total length of [header, query, body]
      //
      uint16_t spec{0x1507}; // (5383) Magic two bytes to denote the REPE specification
      uint8_t version = 1; // REPE version
      bool error{}; // Whether an error has occurred
      repe::action action{}; // Action to take, multiple actions may be bit-packed together
      //
      uint64_t id{}; // Identifier
      //
      uint64_t query_length{no_length_provided}; // The total length of the query (-1 denotes no size given)
      //
      uint64_t body_length{no_length_provided}; // The total length of the body (-1 denotes no size given)
      //
      uint16_t query_format{};
      uint16_t body_format{};
      uint32_t reserved{}; // Must be set to zero

      bool notify() const { return bool(uint32_t(action) & uint32_t(repe::action::notify)); }

      bool read() const { return bool(uint32_t(action) & uint32_t(repe::action::read)); }

      bool write() const { return bool(uint32_t(action) & uint32_t(repe::action::write)); }

      void notify(bool enable)
      {
         if (enable) {
            action = static_cast<repe::action>(uint32_t(action) | uint32_t(repe::action::notify));
         }
         else {
            action = static_cast<repe::action>(uint32_t(action) & ~uint32_t(repe::action::notify));
         }
      }

      void read(bool enable)
      {
         if (enable) {
            action = static_cast<repe::action>(uint32_t(action) | uint32_t(repe::action::read));
         }
         else {
            action = static_cast<repe::action>(uint32_t(action) & ~uint32_t(repe::action::read));
         }
      }

      void write(bool enable)
      {
         if (enable) {
            action = static_cast<repe::action>(uint32_t(action) | uint32_t(repe::action::write));
         }
         else {
            action = static_cast<repe::action>(uint32_t(action) & ~uint32_t(repe::action::write));
         }
      }
   };

   static_assert(sizeof(header) == 48);

   // query and body are heap allocated and we want to be able to reuse memory
   struct message final
   {
      repe::header header{};
      std::string query{};
      std::string body{};
   };

   // User interface that will be encoded into a REPE header
   struct user_header final
   {
      std::string_view query = ""; // The JSON pointer path to call or member to access/assign
      uint64_t id{}; // Identifier
      bool error{}; // Whether an error has occurred

      repe::action action{}; // Action to take, multiple actions may be bit-packed together

      bool notify() const { return bool(uint32_t(action) & uint32_t(repe::action::notify)); }

      bool read() const { return bool(uint32_t(action) & uint32_t(repe::action::read)); }

      bool write() const { return bool(uint32_t(action) & uint32_t(repe::action::write)); }

      void notify(bool enable)
      {
         if (enable) {
            action = static_cast<repe::action>(uint32_t(action) | uint32_t(action::notify));
         }
         else {
            action = static_cast<repe::action>(uint32_t(action) & ~uint32_t(action::notify));
         }
      }

      void read(bool enable)
      {
         if (enable) {
            action = static_cast<repe::action>(uint32_t(action) | uint32_t(action::read));
         }
         else {
            action = static_cast<repe::action>(uint32_t(action) & ~uint32_t(action::read));
         }
      }

      void write(bool enable)
      {
         if (enable) {
            action = static_cast<repe::action>(uint32_t(action) | uint32_t(action::write));
         }
         else {
            action = static_cast<repe::action>(uint32_t(action) & ~uint32_t(action::write));
         }
      }
   };

   inline repe::header encode(const user_header& h) noexcept
   {
      repe::header ret{
         .error = h.error, //
         .action = h.action, //
         .id = h.id, //
         .query_length = h.query.size() //
      };
      return ret;
   }
}
