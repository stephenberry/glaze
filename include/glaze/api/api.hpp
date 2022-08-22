// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/api/std/string.hpp"
#include "glaze/api/trait.hpp"
#include "glaze/core/format.hpp"

#include <stdexcept>
#include <mutex>
#include <memory>
#include <functional>
#include <map>

namespace glaze
{
   inline namespace v0_0_1
   {
      struct api
      {
         using sv = std::string_view;
         
         api() noexcept = default;
         api(const api&) noexcept = default;
         api(api&&) noexcept = default;
         api& operator=(const api&) noexcept = default;
         api& operator=(api&&) noexcept = default;
         virtual ~api() noexcept {}
         
         template <class T>
         [[nodiscard]] T& get(const sv path);
         
         template <class T>
         [[nodiscard]] T* get_if(const sv path) noexcept;

         virtual bool read(const uint32_t /*format*/, const sv /*path*/,
                    const sv /*data*/) noexcept
         { return false; }

         virtual bool write(const uint32_t /*format*/, const sv /*path*/, std::string& /*data*/) noexcept {
            return false;
         }
         
         virtual const sv last_error() const noexcept {
            return error;
         }
         
      protected:
         virtual void* get(const sv path, const sv type_hash) noexcept = 0;

         std::string error{};
      };

      using api_map_t =
         std::map<std::string, std::function<std::shared_ptr<api>()>,
                  std::less<>>;
      
      template <class T>
      T& api::get(const std::string_view path) {
         static constexpr auto hash = glaze::hash<T>();
         auto* ptr = get(path, hash);
         if (ptr) {
            return *static_cast<T*>(ptr);
         }
         else {
            error = "glaze::get<" + std::string(glaze::name<T>) + ">(\"" + std::string(path) + "\") | " + error;
#ifdef __cpp_exceptions
            throw std::runtime_error(error);
#else
            static T x{};
            return x;
#endif
         }
      }
      
      template <class T>
      T* api::get_if(const std::string_view path) noexcept {
         static constexpr auto hash = glaze::hash<T>();
         auto* ptr = get(path, hash);
         if (ptr) {
            return static_cast<T*>(ptr);
         }
         return nullptr;
      }
   }
}

#if defined(_WIN32) || defined(__CYGWIN__)
#define DLL_EXPORT __declspec(dllexport)
#else
define DLL_EXPORT
#endif

struct wrapper
{
   glaze::api_map_t map{};
};

extern "C" DLL_EXPORT wrapper create_api() noexcept;
