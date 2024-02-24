// Glaze Library
// For the license information refer to glaze.hpp

// originally from: https://github.com/kocienda/Any

#pragma once

#include <concepts>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "glaze/api/trait.hpp"

namespace glz {

template <class T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

static inline void handle_bad_any_cast() { std::abort(); }

template <class T>
struct in_place_t : std::false_type {};
template <>
struct in_place_t<std::in_place_t> : std::true_type {};
template <class T>
struct in_place_t<std::in_place_type_t<T>> : std::true_type {};
template <size_t S>
struct in_place_t<std::in_place_index_t<S>> : std::true_type {};
template <class T>
constexpr bool is_in_place = in_place_t<T>::value;

constexpr size_t storage_size = 3 * sizeof(void *);
using storage_buffer =
    std::aligned_storage_t<storage_size, std::alignment_of_v<void *>>;

template <class T>
concept is_storage_sized = requires {
    sizeof(T) <= sizeof(storage_buffer);
} && (std::alignment_of_v<storage_buffer> % std::alignment_of_v<T> == 0);

union storage {
    constexpr storage() {}
    storage(const storage &) = delete;
    storage(storage &&) = delete;
    storage &operator=(const storage &) = delete;
    storage &operator=(storage &&) = delete;

    storage_buffer buf;
    void *ptr = nullptr;
};

template <class T>
struct fallback_typeinfo {
    static constexpr int id = 0;
};

template <class T>
inline constexpr const void *fallback_typeid() {
    return &fallback_typeinfo<std::decay_t<T>>::id;
}

inline static constexpr void *void_get(storage *, const void *) {
    return nullptr;
}

inline static constexpr void void_copy(storage *, const storage *) {}

inline static constexpr void void_move(storage *, storage *) {}

inline static constexpr void void_drop(storage *) {}

struct any_actions {
    using Get = void *(*)(storage *s, const void *type);
    using Copy = void (*)(storage *dst, const storage *src);
    using Move = void (*)(storage *dst, storage *src);
    using Drop = void (*)(storage *s);

    constexpr any_actions() noexcept {}

    constexpr any_actions(Get g, Copy c, Move m, Drop d, const void *t) noexcept
        : get(g), copy(c), move(m), drop(d), type(t) {}

    Get get = void_get;
    Copy copy = void_copy;
    Move move = void_move;
    Drop drop = void_drop;
    const void *type = fallback_typeid<void>();
};

template <class T>
struct any_traits {
    template <class X = T, class... Args>
        requires(is_storage_sized<X> && std::is_nothrow_move_constructible_v<X>)
    inline static X &make(storage *s, std::in_place_type_t<X>,
                          Args &&...args) {
        return *(::new (static_cast<void *>(&s->buf))
                     X(std::forward<Args>(args)...));
    }

    template <class X = T, class... Args>
        requires(!is_storage_sized<X>)
    inline static X &make(storage *s, std::in_place_type_t<X>,
                          Args &&...args) {
        s->ptr = new X(std::forward<Args>(args)...);
        return *static_cast<X *>(s->ptr);
    }

   private:
    any_traits(const any_traits &) = default;
    any_traits(any_traits &&) = default;
    any_traits &operator=(const any_traits &) = default;
    any_traits &operator=(any_traits &&) = default;

    template <class X = T>
    inline static bool compare_typeid(const void *id) {
        return (id && id == fallback_typeid<X>());
    }

    //
    // get
    //
    template <class X = T>
        requires std::same_as<X, void>
    inline static void *get(storage *, const void *) {
        return nullptr;
    }

    template <class X = T>
        requires is_storage_sized<X>
    inline static void *get(storage *s, const void *type) {
        if (compare_typeid<X>(type)) {
            return static_cast<void *>(&s->buf);
        }
        return nullptr;
    }

    template <class X = T>
        requires(!is_storage_sized<X>)
    inline static void *get(storage *s, const void *type) {
        if (compare_typeid<X>(type)) {
            return s->ptr;
        }
        return nullptr;
    }

    //
    // copy
    //
    template <class X = T>
        requires std::same_as<X, void>
    inline static void copy(storage *, const storage *) {}

    template <class X = T>
        requires(is_storage_sized<X> && std::is_nothrow_move_constructible_v<X>)
    inline static void copy(storage *dst, const storage *src) {
        any_traits::make(
            dst, std::in_place_type_t<X>(),
            *static_cast<X const *>(static_cast<void const *>(&src->buf)));
    }

    template <class X = T>
        requires(!is_storage_sized<X>)
    inline static void copy(storage *dst, const storage *src) {
        any_traits::make(
            dst, std::in_place_type_t<X>(),
            *static_cast<X const *>(static_cast<void const *>(src->ptr)));
    }

    //
    // move
    //
    template <class X = T>
        requires is_storage_sized<X>
    inline static void move(storage *dst, storage *src) {
        any_traits::make(dst, std::in_place_type_t<X>(),
                         std::move(*static_cast<X const *>(
                             static_cast<void const *>(&src->buf))));
    }

    template <class X = T>
        requires(!is_storage_sized<X>)
    inline static void move(storage *dst, storage *src) {
        dst->ptr = src->ptr;
    }

    //
    // drop
    //
    template <class X = T>
        requires std::same_as<X, void>
    inline static void drop(storage *) {}

    template <class X = T>
        requires std::is_trivially_destructible_v<X>
    inline static void drop(storage *) {}

    template <class X = T>
        requires(is_storage_sized<X> && !std::is_trivially_destructible_v<X>)
    inline static void drop(storage *s) {
        X &t = *static_cast<X *>(
            static_cast<void *>(const_cast<storage_buffer *>(&s->buf)));
        t.~X();
    }

    template <class X = T>
        requires(!is_storage_sized<X>)
    inline static void drop(storage *s) {
        delete static_cast<X *>(s->ptr);
    }

   public:
    static constexpr any_actions actions =
        any_actions(get<T>, copy<T>, move<T>, drop<T>, fallback_typeid<T>());
};

struct any;

template <class V, class T = std::decay_t<V>>
concept is_any_constructible =
    !std::same_as<T, any> && !is_in_place<V> && std::is_copy_constructible_v<T>;

template <class T, class U, class... Args>
concept is_list_constructible =
    std::is_constructible_v<T, std::initializer_list<U> &, Args...> &&
    std::is_copy_constructible_v<T>;

struct any {
    constexpr any() noexcept : actions(void_any_actions) {}

    template <class V, class T = std::decay_t<V>>
        requires is_any_constructible<V>
    any(V &&v) : actions(&any_traits<T>::actions) {
        any_traits<T>::make(&storage, std::in_place_type_t<T>(),
                            std::forward<V>(v));
    }

    template <class V, class... Args, class T = std::decay_t<V>>
        requires is_any_constructible<T>
    explicit any(std::in_place_type_t<V> vtype, Args &&...args)
        : actions(&any_traits<T>::actions) {
        any_traits<T>::make(&storage, vtype, std::forward<Args>(args)...);
    }

    template <class V, class U, class... Args, class T = std::decay_t<V>>
        requires is_list_constructible<T, U, Args...>
    explicit any(std::in_place_type_t<V> vtype, std::initializer_list<U> list,
                 Args &&...args)
        : actions(&any_traits<T>::actions) {
        any_traits<T>::make(&storage, vtype,
                            V{list, std::forward<Args>(args)...});
    }

    any(const any &other) : actions(other.actions) {
        actions->copy(&storage, &other.storage);
    }

    any(any &&other) noexcept : actions(other.actions) {
        actions->move(&storage, &other.storage);
        other.actions = void_any_actions;
    }

    any &operator=(const any &other) {
        if (this != &other) {
            actions->drop(&storage);
            actions = other.actions;
            actions->copy(&storage, &other.storage);
        }
        return *this;
    }

    any &operator=(any &&other) noexcept {
        if (this != &other) {
            actions->drop(&storage);
            actions = other.actions;
            actions->move(&storage, &other.storage);
            other.actions = void_any_actions;
        }
        return *this;
    }

    template <class V, class T = std::decay_t<V>>
        requires is_any_constructible<T>
    any &operator=(V &&v) {
        *this = any(std::forward<V>(v));
        return *this;
    }

    ~any() { actions->drop(&storage); }

    template <class V, class... Args, class T = std::decay_t<V>>
        requires(std::is_constructible_v<T, Args...> &&
                 std::is_copy_constructible_v<T>)
    T &emplace(Args &&...args) {
        actions->drop(&storage);
        actions = &any_traits<T>::actions;
        return any_traits<T>::make(&storage, std::in_place_type_t<T>(),
                                   std::forward<Args>(args)...);
    }

    template <class V, class U, class... Args, class T = std::decay_t<V>>
        requires is_list_constructible<T, U, Args...>
    T &emplace(std::initializer_list<U> list, Args &&...args) {
        reset();
        actions = &any_traits<T>::actions;
        return any_traits<T>::make(&storage, std::in_place_type_t<T>(),
                                   V{list, std::forward<Args>(args)...});
    }

    inline void reset() {
        actions->drop(&storage);
        actions = void_any_actions;
    }
   
   template <class V>
   inline void* data() {
      using U = std::decay_t<V>;
      return actions->get(&storage, fallback_typeid<U>()); }
   
   template <class V>
   inline const void* data() const {
      using U = std::decay_t<V>;
      return actions->get(&storage, fallback_typeid<U>()); }

    inline void swap(any &rhs) noexcept {
        if (this == &rhs) {
            return;
        }

        any tmp;

        // swap storage
        rhs.actions->move(&tmp.storage, &rhs.storage);
        actions->move(&rhs.storage, &storage);
        rhs.actions->move(&storage, &tmp.storage);

        // swap actions
        tmp.actions = rhs.actions;
        rhs.actions = actions;
        actions = tmp.actions;
    }

    template <bool B>
    inline bool has_value() const noexcept {
        return (actions != void_any_actions) == B;
    }

    inline bool has_value() const noexcept { return has_value<true>(); }

    template <class V>
    friend remove_cvref_t<V> *any_cast(any *a) noexcept;

   private:
    static constexpr any_actions void_any_actions_value = any_actions();
    static constexpr const any_actions *const void_any_actions =
        &void_any_actions_value;

    const any_actions *actions;
    glz::storage storage;
};

inline void swap(any &lhs, any &rhs) noexcept { lhs.swap(rhs); }

template <class T, class... Args>
inline any make_any(Args &&...args) {
    return any(std::in_place_type<T>, std::forward<Args>(args)...);
}

template <class T, class U, class... Args>
inline any make_any(std::initializer_list<U> il, Args &&...args) {
    return any(std::in_place_type<T>, il, std::forward<Args>(args)...);
}

template <class V, class T = remove_cvref_t<V>>
    requires(std::constructible_from<T, const remove_cvref_t<T> &>)
V any_cast(const any &a) {
    auto tmp = any_cast<std::add_const_t<T>>(&a);
    if (tmp == nullptr) {
        handle_bad_any_cast();
    }
    return static_cast<V>(*tmp);
}

template <class V, class T = remove_cvref_t<V>>
    requires(std::constructible_from<T, remove_cvref_t<T> &>)
V any_cast(any &a) {
    auto tmp = any_cast<T>(&a);
    if (tmp == nullptr) {
        handle_bad_any_cast();
    }
    return static_cast<V>(*tmp);
}

template <class V, class T = remove_cvref_t<V>>
    requires(std::constructible_from<T, remove_cvref_t<T> &>)
V any_cast(any &&a) {
    auto tmp = any_cast<T>(&a);
    if (tmp == nullptr) {
        handle_bad_any_cast();
    }
    return static_cast<V>(std::move(*tmp));
}

template <class V, class T = remove_cvref_t<V>>
const T *any_cast(const any *a) noexcept {
    return any_cast<V>(const_cast<any *>(a));
}

template <class V>
remove_cvref_t<V> *any_cast(any *a) noexcept {
    using T = remove_cvref_t<V>;
    using U = std::decay_t<V>;
    if (a && a->has_value()) {
        void *p = a->actions->get(&a->storage, fallback_typeid<U>());
        return (std::is_function<V>{}) ? nullptr : static_cast<T *>(p);
    }
    return nullptr;
}

}

// originally from: https://codereview.stackexchange.com/questions/219075/implementation-of-stdany

/*namespace glz
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
}*/
