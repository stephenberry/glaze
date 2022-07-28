// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/api/std/string.hpp"
#include "glaze/api/meta.hpp"
#include "glaze/format.hpp"

#include <stdexcept>

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
         [[nodiscard]]  T& get(const sv path);
         
         template <class T>
         [[nodiscard]] T* get_if(const sv path) noexcept;

         virtual bool write(const uint32_t /*format*/, const sv /*path*/,
                    const sv /*data*/) noexcept
         { return false; }

          virtual bool read(const uint32_t /*format*/, const sv /*path*/,
                            std::string& /*data*/) noexcept
         {
            return false;
         }
         
         virtual const sv last_error() const noexcept {
            return error;
         }
         
         virtual constexpr const version_type version() const noexcept = 0;
         virtual constexpr const sv version_sv() const noexcept = 0;
         virtual constexpr const sv hash() const noexcept = 0;
         
      protected:
         virtual void* get(const sv path, const sv type_hash) noexcept = 0;

         std::string error{};
      };
      
      [[nodiscard]] inline std::shared_ptr<glaze::api> create_api(const std::string_view name = "") noexcept;
      
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
