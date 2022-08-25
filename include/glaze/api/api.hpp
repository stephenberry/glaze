// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/api/std/string.hpp"
#include "glaze/api/trait.hpp"
#include "glaze/core/format.hpp"

#include <stdexcept>
#include <memory>
#include <functional>
#include <map>

namespace glaze
{
   inline namespace v0_0_1
   {
      struct api
      {         
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
                           const sv /*data*/) noexcept = 0;

         virtual bool write(const uint32_t /*format*/, const sv /*path*/, std::string& /*data*/) = 0;
         
         virtual const sv last_error() const noexcept {
            return error;
         }
         
      protected:
         /// unchecked void* access
         virtual void* get(const sv path, const sv type_hash) noexcept = 0;

         std::string error{};
      };

      using interface =
         std::map<std::string, std::function<std::shared_ptr<api>()>,
                  std::less<>>;
      
      /// access reference via JSON pointer path
      template <class T>
      T& api::get(const sv path) {
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
      T* api::get_if(const sv path) noexcept {
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
#define DLL_EXPORT
#endif

// IMPORTANT: unmanged memory returned
extern "C" DLL_EXPORT glaze::interface* glaze_interface() noexcept;
