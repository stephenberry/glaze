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
#include <span>

namespace glz
{
   inline namespace v0_0_2
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

         // Get a std::function from a member function or std::function across the API
         template <class T>
         [[nodiscard]] T get_fn(const sv path);

         template <class Ret, class... Args>
         [[nodiscard]] Ret call(const sv path, Args&&... args);
         
         [[nodiscard]] virtual bool contains(const sv path) noexcept = 0;

         virtual bool read(const uint32_t /*format*/, const sv /*path*/,
                           const sv /*data*/) noexcept = 0;

         virtual bool write(const uint32_t /*format*/, const sv /*path*/, std::string& /*data*/) = 0;

         [[nodiscard]] virtual const sv last_error() const noexcept {
            return error;
         }

         /// unchecked `void*` access for low level programming (prefer templated get)
         [[nodiscard]] virtual void* get(const sv path, const sv type_hash) noexcept = 0;
      protected:

         virtual bool caller(const sv path, const sv type_hash, void*& ret, std::span<void*> args) noexcept = 0;

         virtual std::unique_ptr<void, void(*)(void*)> get_fn(const sv path, const sv type_hash) noexcept = 0;

         std::string error{};
      };

      using iface =
         std::map<std::string, std::function<std::shared_ptr<api>()>,
                  std::less<>>;

      /// access reference via JSON pointer path
      template <class T>
      T& api::get(const sv path) {
         static constexpr auto hash = glz::hash<T>();
         auto* ptr = get(path, hash);
         if (ptr) {
            return *static_cast<T*>(ptr);
         }
         else {
            error = "\n api: glaze::get<" + std::string(glz::name_v<T>) + ">(\"" + std::string(path) + "\") | " + error;
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
         static constexpr auto hash = glz::hash<T>();
         auto* ptr = get(path, hash);
         if (ptr) {
            return static_cast<T*>(ptr);
         }
         return nullptr;
      }

      template <class T>
      T api::get_fn(const sv path) {
         static constexpr auto hash = glz::hash<T>();
         auto d = get_fn(path, hash);
         if (d) {
            T copy = *static_cast<T*>(d.get());
            return copy;
         }
         else {
            error = "\n api: glaze::get_fn<" + std::string(glz::name_v<T>) + ">(\"" + std::string(path) + "\") | " + error;
   #ifdef __cpp_exceptions
            throw std::runtime_error(error);
   #else
            static T x{};
            return x;
   #endif
         }
      }

      template <class Ret, class... Args>
      Ret api::call(const sv path, Args&&... args)
      {
         using F = std::function<Ret(Args...)>;
         static constexpr auto hash = glz::hash<F>();

         static constexpr auto N = sizeof...(Args);
         std::array<void*, N> arguments;

         //auto tuple = std::make_tuple(std::forward<Args>(args)...);
         auto tuple = std::forward_as_tuple(std::forward<Args>(args)...);

         for_each<N>([&](auto I) {
            std::get<I>(arguments) = &std::get<I>(tuple);
         });

         if constexpr (std::is_pointer_v<Ret>) {
            void* ptr{};
            const auto success = caller(path, hash, ptr, arguments);

            if (success) {
               return static_cast<Ret>(ptr);
            }
            else {
               error = "\n api: glaze::call<" + std::string(glz::name_v<Ret>) + ">(\"" + std::string(path) + "\") | " + error;
      #ifdef __cpp_exceptions
               throw std::runtime_error(error);
      #else
               static T x{};
               return x;
      #endif
            }
         }
         else if constexpr (std::is_void_v<Ret>) {
            void* ptr = nullptr;
            const auto success = caller(path, hash, ptr, arguments);

            if (success) {
               return;
            }
            else {
               error = "\n api: glaze::call<" + std::string(glz::name_v<Ret>) + ">(\"" + std::string(path) + "\") | " +
                       error;
#ifdef __cpp_exceptions
               throw std::runtime_error(error);
#else
               return;
#endif
            }
         }
         else if constexpr (std::is_lvalue_reference_v<Ret>) {
            void* ptr{};
            const auto success = caller(path, hash, ptr, arguments);

            if (success) {
               return *static_cast<std::decay_t<Ret>*>(ptr);
            }
            else {
               error = "\n api: glaze::call<" + std::string(glz::name_v<Ret>) + ">(\"" + std::string(path) + "\") | " +
                       error;
#ifdef __cpp_exceptions
               throw std::runtime_error(error);
#else
               static T x{};
               return x;
#endif
            }
         }
         else {
            Ret value{};
            void* ptr = &value;
            const auto success = caller(path, hash, ptr, arguments);

            if (success) {
               return value;
            }
            else {
               error = "\n api: glaze::call<" + std::string(glz::name_v<Ret>) + ">(\"" + std::string(path) + "\") | " + error;
      #ifdef __cpp_exceptions
               throw std::runtime_error(error);
      #else
               static T x{};
               return x;
      #endif
            }
         }
      }

      using iface_fn = std::shared_ptr<glz::iface>(*)();
   }
}

#if defined(_WIN32) || defined(__CYGWIN__)
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

extern "C" DLL_EXPORT glz::iface_fn glz_iface() noexcept;
