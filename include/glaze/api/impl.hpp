// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/api/std/functional.hpp"
#include "glaze/api/std/string.hpp"
#include "glaze/api/std/unordered_map.hpp"
#include "glaze/api/std/vector.hpp"
#include "glaze/api/type_support.hpp"
#include "glaze/api/api.hpp"

namespace glaze
{
   namespace helper
   {
      template <class T>
      inline std::pair<std::string_view, void*> hide(T* x) {
         return { glaze::hash<T*>(), x };
      }
   }
   
   template <class Interface>
   struct impl : api
   {
      Interface interface{};
      
      template <class T>
      void set(const std::string_view sv, T& x) {
         using V = std::add_pointer_t<std::decay_t<T>>;
         const auto p = helper::hide(&x);
         inter.emplace(sv, p);
         if (!hash_names.contains(p.first)) {
            hash_names.emplace(p.first, glaze::name<T>);
         }
      }
      
      void* get(const std::string_view path, const std::string_view type_hash) noexcept override
      {
         // compare input hash with hash associated with path
         if (auto it = inter.find(path); it != inter.end()) [[likely]] {
            const auto[h, ptr] = it->second;
            if (h == type_hash) [[likely]] {
               return ptr;
            }
            else [[unlikely]] {
               error = "mismatching types";
               if (auto it = hash_names.find(h); it != hash_names.end()) {
                  error += ", expected: " + std::string(it->second);
               }
               else {
                  error += ", nonexistant type";
               }
            }
         }
         else [[unlikely]] {
            error = "identifier not found '" + std::string(path) + "'";
         }
         return nullptr;
      }
      
      bool to(const uint32_t format, const sv path, sv& /*out*/) noexcept override
      {
         error = "glaze::to | no formats supported";
         return false;
      }
      
      bool from(const uint32_t format, const sv path, const sv /*in*/) noexcept override
      {
         error = "glaze::from | no formats supported";
         return false;
      }
      
      virtual constexpr const version_type version() const noexcept override {
         return glaze::meta<Interface>::version;
      }
      
      constexpr const std::string_view version_sv() const noexcept override {
         return glaze::meta<Interface>::version_sv;
      }
      
      constexpr const std::string_view hash() const noexcept override {
         return glaze::hash<Interface>();
      }
      
   private:
      std::unordered_map<std::string_view, std::pair<std::string_view, void*>> inter;
      std::unordered_map<std::string_view, std::string_view> hash_names;
   };
   
   template <class T>
   inline constexpr std::pair<T&, std::shared_ptr<impl<T>>> make_impl() {
      auto io = std::make_shared<impl<T>>();
      return { io->interface, io };
   }
}
