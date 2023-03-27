// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// originally from: https://codereview.stackexchange.com/questions/219075/implementation-of-stdany

#include <concepts>
#include <memory>
#include <utility>

#include "glaze/api/trait.hpp"

namespace glz
{
   template <class T>
   struct is_in_place_type_t : std::false_type
   {};

   template <class T>
   struct is_in_place_type_t<std::in_place_type_t<T>> : std::true_type
   {};

   template <class T>
   concept is_in_place_type = is_in_place_type_t<T>::value;

   template <class T, class List, class... Args>
   concept init_list_constructible = std::constructible_from<std::decay_t<T>, std::initializer_list<List>&, Args...>;

   template <class T>
   concept copy_constructible = std::copy_constructible<std::decay_t<T>>;

   struct any
   {
      constexpr any() noexcept = default;
      ~any() = default;

      any(const any& other)
      {
         if (other.instance) {
            instance = other.instance->clone();
         }
      }

      any(any&& other) noexcept : instance(std::move(other.instance)) {}

      template <class T>
         requires(!std::same_as<std::decay_t<T>, any> && !is_in_place_type<T> && copy_constructible<T>)
      any(T&& value)
      {
         emplace<std::decay_t<T>>(std::forward<T>(value));
      }

      template <class T, class... Args>
         requires(std::constructible_from<std::decay_t<T>, Args...> && copy_constructible<T>)
      explicit any(std::in_place_type_t<T>, Args&&... args)
      {
         emplace<std::decay_t<T>>(std::forward<Args>(args)...);
      }

      template <class T, class List, class... Args>
         requires(init_list_constructible<T, List, Args...> && copy_constructible<T>)
      explicit any(std::in_place_type_t<T>, std::initializer_list<List> list, Args&&... args)
      {
         emplace<std::decay_t<T>>(list, std::forward<Args>(args)...);
      }

      any& operator=(const any& rhs)
      {
         any(rhs).swap(*this);
         return *this;
      }

      any& operator=(any&& rhs) noexcept
      {
         std::move(rhs).swap(*this);
         return *this;
      }

      template <class T>
         requires(!std::is_same_v<std::decay_t<T>, any> && copy_constructible<T>)
      any& operator=(T&& rhs)
      {
         any(std::forward<T>(rhs)).swap(*this);
         return *this;
      }

      template <class T, class... Args>
         requires(std::constructible_from<std::decay_t<T>, Args...> && copy_constructible<T>)
      std::decay_t<T>& emplace(Args&&... args)
      {
         using D = std::decay_t<T>;
         auto new_inst = std::make_unique<storage_impl<D>>(std::forward<Args>(args)...);
         D& value = new_inst->value;
         instance = std::move(new_inst);
         return value;
      }

      template <class T, class List, class... Args>
         requires(init_list_constructible<T, List, Args...> && copy_constructible<T>)
      std::decay_t<T>& emplace(std::initializer_list<List> list, Args&&... args)
      {
         using D = std::decay_t<T>;
         auto new_inst = std::make_unique<storage_impl<D>>(list, std::forward<Args>(args)...);
         D& value = new_inst->value;
         instance = std::move(new_inst);
         return value;
      }

      void reset() noexcept { instance.reset(); }

      void* data() { return instance->data(); }

      void swap(any& other) noexcept { std::swap(instance, other.instance); }

      bool has_value() const noexcept { return bool(instance); }

      friend void swap(any& lhs, any& rhs) { lhs.swap(rhs); }

     private:
      template <class T>
      friend const T* any_cast(const any*) noexcept;

      template <class T>
      friend T* any_cast(any*) noexcept;

      struct storage_base
      {
         virtual ~storage_base() = default;

         virtual std::unique_ptr<storage_base> clone() const = 0;
         virtual void* data() noexcept = 0;
         glz::hash_t type_hash;
      };

      std::unique_ptr<storage_base> instance;

      template <class T>
      struct storage_impl final : storage_base
      {
         template <class... Args>
         storage_impl(Args&&... args) : value(std::forward<Args>(args)...)
         {
            type_hash = hash<T>();
         }

         std::unique_ptr<storage_base> clone() const override { return std::make_unique<storage_impl<T>>(value); }

         void* data() noexcept override
         {
            if constexpr (std::is_pointer_v<T>) {
               return value;
            }
            else {
               return &value;
            }
         }

         T value;
      };
   };

   // C++20
   template <class T>
   using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

   template <class T>
      requires(std::constructible_from<T, const remove_cvref_t<T>&>)
   T any_cast(const any& a)
   {
      using V = remove_cvref_t<T>;
      if (auto* value = any_cast<V>(&a)) {
         return static_cast<T>(*value);
      }
      else {
         std::abort();
      }
   }

   template <class T>
      requires(std::constructible_from<T, remove_cvref_t<T>&>)
   T any_cast(any& a)
   {
      using V = remove_cvref_t<T>;
      if (auto* value = any_cast<V>(&a)) {
         return static_cast<T>(*value);
      }
      else {
         std::abort();
      }
   }

   template <class T>
      requires(std::constructible_from<T, remove_cvref_t<T>>)
   T any_cast(any&& a)
   {
      using V = remove_cvref_t<T>;
      if (auto* value = any_cast<V>(&a)) {
         return static_cast<T>(std::move(*value));
      }
      else {
         std::abort();
      }
   }

   template <class T>
   const T* any_cast(const any* a) noexcept
   {
      if (!a) {
         return nullptr;
      }
      static constexpr auto h = hash<T>();

      if (h == a->instance->type_hash) [[likely]] {
         return &(reinterpret_cast<any::storage_impl<T>*>(a->instance.get())->value);
      }
      return nullptr;
   }

   template <class T>
   T* any_cast(any* a) noexcept
   {
      return const_cast<T*>(any_cast<T>(static_cast<const any*>(a)));
   }

   template <class T, class... Args>
   any make_any(Args&&... args)
   {
      return any(std::in_place_type<T>, std::forward<Args>(args)...);
   }

   template <class T, class List, class... Args>
   any make_any(std::initializer_list<List> list, Args&&... args)
   {
      return any(std::in_place_type<T>, list, std::forward<Args>(args)...);
   }
}
