// nanorange.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_HPP_INCLUDED
#define NANORANGE_HPP_INCLUDED

// nanorange/algorithm.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_HPP_INCLUDED
#define NANORANGE_ALGORITHM_HPP_INCLUDED

// Algorithms reimplemented in Nanorange
// nanorange/algorithm/adjacent_find.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_ADJACENT_FIND_HPP_INCLUDED
#define NANORANGE_ALGORITHM_ADJACENT_FIND_HPP_INCLUDED

// nanorange/range.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_RANGES_HPP_INCLUDED
#define NANORANGE_RANGES_HPP_INCLUDED

// nanorange/detail/iterator/algorithm_requirements.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_ITERATOR_ALGORITHM_REQUIREMENTS_HPP_INCLUDED
#define NANORANGE_DETAIL_ITERATOR_ALGORITHM_REQUIREMENTS_HPP_INCLUDED

// nanorange/detail/functional/comparisons.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_FUNCTIONAL_COMPARISONS_HPP_INCLUDED
#define NANORANGE_DETAIL_FUNCTIONAL_COMPARISONS_HPP_INCLUDED

// nanorange/detail/concepts/comparison.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_CONCEPTS_COMPARISON_HPP_INCLUDED
#define NANORANGE_DETAIL_CONCEPTS_COMPARISON_HPP_INCLUDED

// nanorange/detail/concepts/core.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_CONCEPTS_CORE_HPP_INCLUDED
#define NANORANGE_DETAIL_CONCEPTS_CORE_HPP_INCLUDED

// nanorange/detail/macros.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_MACROS_HPP_INCLUDED
#define NANORANGE_DETAIL_MACROS_HPP_INCLUDED

#include <ciso646>

#ifdef NANORANGE_NO_DEPRECATION_WARNINGS
#define NANO_DEPRECATED
#define NANO_DEPRECATED_FOR(x)
#else
#define NANO_DEPRECATED [[deprecated]]
#define NANO_DEPRECATED_FOR(x) [[deprecated(x)]]
#endif

#ifdef __has_cpp_attribute
#if __has_cpp_attribute(no_unique_address) >= 201803L
#define NANO_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define NANO_NO_UNIQUE_ADDRESS
#endif // __has_cpp_attribute(no_unique_address)
#else
#define NANO_NO_UNIQUE_ADDRESS
#endif // defined(__has_cpp_attribute)

#define NANO_CONCEPT inline constexpr bool

#define NANO_BEGIN_NAMESPACE                                                   \
    \
namespace nano                                                                 \
    {                                                                          \
        \
inline namespace ranges                                                        \
        {

#define NANO_END_NAMESPACE                                                     \
    }                                                                          \
    }

#define NANO_INLINE_VAR(type, name)                                            \
    inline namespace function_objects {                                        \
    inline constexpr type name{};                                              \
    }

#if defined(_LIBCPP_VERSION)
#define NANO_BEGIN_NAMESPACE_STD _LIBCPP_BEGIN_NAMESPACE_STD
#define NANO_END_NAMESPACE_STD _LIBCPP_END_NAMESPACE_STD
#elif defined(_MSVC_STL_VERSION)
#define NANO_BEGIN_NAMESPACE_STD _STD_BEGIN
#define NANO_END_NAMESPACE_STD _STD_END
#elif defined(_GLIBCXX_DEBUG)
#ifndef NANORANGE_NO_STD_FORWARD_DECLARATIONS
#define NANORANGE_NO_STD_FORWARD_DECLARATIONS
#endif
#else
#define NANO_BEGIN_NAMESPACE_STD namespace std {
#define NANO_END_NAMESPACE_STD }
#endif

#if defined(_MSC_VER)
#define NANO_MSVC_LAMBDA_PIPE_WORKAROUND 1
#endif

#endif

// nanorange/type_traits.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_TYPE_TRAITS_HPP_INCLUDED
#define NANORANGE_TYPE_TRAITS_HPP_INCLUDED

// nanorange/detail/common_reference.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_COMMON_REFERENCE_HPP_INCLUDED
#define NANORANGE_DETAIL_COMMON_REFERENCE_HPP_INCLUDED


// nanorange/detail/type_traits.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_TYPE_TRAITS_HPP_INCLUDED
#define NANORANGE_DETAIL_TYPE_TRAITS_HPP_INCLUDED



#include <type_traits>

NANO_BEGIN_NAMESPACE

template <typename T>
struct remove_cvref {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
};

template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

template <typename T>
struct type_identity {
    using type = T;
};

template <typename T>
using type_identity_t = typename type_identity<T>::type;

namespace detail {

template <bool>
struct conditional {
    template <typename T, typename>
    using type = T;
};

template <>
struct conditional<false> {
    template <typename, typename U>
    using type = U;
};

template <bool B, typename T, typename U>
using conditional_t = typename conditional<B>::template type<T, U>;

template <template <class...> class AliasT, typename... Args>
auto exists_helper(long) -> std::false_type;

template <template <class...> class AliasT, typename... Args,
          typename = AliasT<Args...>>
auto exists_helper(int) -> std::true_type;

template <template <class...> class AliasT, typename... Args>
inline constexpr bool exists_v = decltype(exists_helper<AliasT, Args...>(0))::value;

template <typename, typename...>
auto test_requires_fn(long) -> std::false_type;

template <typename R, typename... Args,
          typename = decltype(&R::template requires_<Args...>)>
auto test_requires_fn(int) -> std::true_type;

template <typename R, typename... Args>
inline constexpr bool requires_ = decltype(test_requires_fn<R, Args...>(0))::value;

template <bool Expr>
using requires_expr = std::enable_if_t<Expr, int>;

template <std::size_t I>
struct priority_tag : priority_tag<I - 1> {
};

template <>
struct priority_tag<0> {
};

} // namespace detail

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

template <typename...>
struct common_type;

namespace detail {

template <typename T, typename U>
struct copy_cv {
    using type = U;
};

template <typename T, typename U>
struct copy_cv<const T, U> {
    using type = std::add_const_t<U>;
};

template <typename T, typename U>
struct copy_cv<volatile T, U> {
    using type = std::add_volatile_t<U>;
};

template <typename T, typename U>
struct copy_cv<const volatile T, U> {
    using type = std::add_cv_t<U>;
};

template <typename T, typename U>
using copy_cv_t = typename copy_cv<T, U>::type;

template <typename T>
using cref_t = std::add_lvalue_reference_t<const std::remove_reference_t<T>>;

// Workaround for "term does not evaluate to a function taking 0 arguments"
// error in MSVC 19.22 (issue #75)
#if defined(_MSC_VER) && _MSC_VER >= 1922
template <typename, typename, typename = void>
struct cond_res {};

template <typename T, typename U>
struct cond_res<T, U, std::void_t<decltype(false ? std::declval<T (&)()>()()
                                                 : std::declval<U (&)()>()())>>
{
    using type = decltype(false ? std::declval<T (&)()>()()
                                : std::declval<U (&)()>()());
};

template <typename T, typename U>
using cond_res_t = typename cond_res<T, U>::type;
#else
template <typename T, typename U>
using cond_res_t = decltype(false ? std::declval<T (&)()>()()
                                  : std::declval<U (&)()>()());
#endif


// For some value of "simple"
template <typename A, typename B,
          typename X = std::remove_reference_t<A>,
          typename Y = std::remove_reference_t<B>,
          typename = void>
struct common_ref {};

template <typename A, typename B>
using common_ref_t = typename common_ref<A, B>::type;

template <typename A, typename B,
          typename X = std::remove_reference_t<A>,
          typename Y = std::remove_reference_t<B>,
          typename = void>
struct lval_common_ref {};

template <typename A, typename B, typename X, typename Y>
struct lval_common_ref<A, B, X, Y, std::enable_if_t<
    std::is_reference_v<cond_res_t<copy_cv_t<X, Y>&, copy_cv_t<Y, X>&>>>>
{
    using type = cond_res_t<copy_cv_t<X, Y>&, copy_cv_t<Y, X>&>;
};

template <typename A, typename B>
using lval_common_ref_t = typename lval_common_ref<A, B>::type;

template <typename A, typename B, typename X, typename Y>
struct common_ref<A&, B&, X, Y> : lval_common_ref<A&, B&> {};

template <typename X, typename Y>
using rref_cr_helper_t = std::remove_reference_t<lval_common_ref_t<X&, Y&>>&&;

template <typename A, typename B, typename X, typename Y>
struct common_ref<A&&, B&&, X, Y, std::enable_if_t<
    std::is_convertible_v<A&&, rref_cr_helper_t<X, Y>> &&
    std::is_convertible_v<B&&, rref_cr_helper_t<X, Y>>>>
{
    using type = rref_cr_helper_t<X, Y>;
};

template <typename A, typename B, typename X, typename Y>
struct common_ref<A&&, B&, X, Y, std::enable_if_t<
    std::is_convertible_v<A&&, lval_common_ref_t<const X&, Y&>>>>
{
    using type = lval_common_ref_t<const X&, Y&>;
};

template <typename A, typename B, typename X, typename Y>
struct common_ref<A&, B&&, X, Y>
    : common_ref<B&&, A&>
{};

template <typename>
struct xref { template <typename U> using type = U; };

template <typename A>
struct xref<A&> {
    template <typename U>
    using type = std::add_lvalue_reference_t<typename xref<A>::template type<U>>;
};

template <typename A>
struct xref<A&&> {
    template <typename U>
    using type = std::add_rvalue_reference_t<typename xref<A>::template type<U>>;
};

template <typename A>
struct xref<const A> {
    template <typename U>
    using type = std::add_const_t<typename xref<A>::template type<U>>;
};

template <typename A>
struct xref<volatile A> {
    template <typename U>
    using type = std::add_volatile_t<typename xref<A>::template type<U>>;
};

template <typename A>
struct xref<const volatile A> {
    template <typename U>
    using type = std::add_cv_t<typename xref<A>::template type<U>>;
};

} // namespace detail

template <class T, class U, template <class> class TQual,
          template <class> class UQual>
struct basic_common_reference {
};

template <typename...>
struct common_reference;

template <typename... Ts>
using common_reference_t = typename common_reference<Ts...>::type;

template <>
struct common_reference<> {
};

template <typename T0>
struct common_reference<T0> {
    using type = T0;
};

namespace detail {

template <typename T, typename U>
inline constexpr bool has_common_ref_v =
    exists_v<common_ref_t, T, U>;

template <typename T, typename U>
using basic_common_ref_t =
    typename basic_common_reference<remove_cvref_t<T>, remove_cvref_t<U>,
                                    detail::xref<T>::template type, detail::xref<U>::template type>::type;

template <typename T, typename U>
inline constexpr bool has_basic_common_ref_v =
    exists_v<basic_common_ref_t, T, U>;

template <typename T, typename U>
inline constexpr bool has_cond_res_v = exists_v<cond_res_t, T, U>;

template <typename T, typename U, typename = void>
struct binary_common_ref : common_type<T, U> {
};

template <typename T, typename U>
struct binary_common_ref<T, U, std::enable_if_t<has_common_ref_v<T, U>>>
    : common_ref<T, U> {};

template <typename T, typename U>
struct binary_common_ref<T, U,
                         std::enable_if_t<has_basic_common_ref_v<T, U> &&
                                          !has_common_ref_v<T, U>>>
{
    using type = basic_common_ref_t<T, U>;
};

template <typename T, typename U>
struct binary_common_ref<T, U,
                         std::enable_if_t<has_cond_res_v<T, U> &&
                                          !has_basic_common_ref_v<T, U> &&
                                          !has_common_ref_v<T, U>>>
{
    using type = cond_res_t<T, U>;
};

} // namespace detail

template <typename T1, typename T2>
struct common_reference<T1, T2> : detail::binary_common_ref<T1, T2> {
};

namespace detail {

template <typename Void, typename T1, typename T2, typename... Rest>
struct multiple_common_reference {
};

template <typename T1, typename T2, typename... Rest>
struct multiple_common_reference<std::void_t<common_reference_t<T1, T2>>, T1, T2,
                                 Rest...>
    : common_reference<common_reference_t<T1, T2>, Rest...> {
};

} // namespace detail

template <typename T1, typename T2, typename... Rest>
struct common_reference<T1, T2, Rest...>
    : detail::multiple_common_reference<void, T1, T2, Rest...> {
};

NANO_END_NAMESPACE

#endif

// nanorange/detail/common_reference.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_COMMON_TYPE_HPP_INCLUDED
#define NANORANGE_DETAIL_COMMON_TYPE_HPP_INCLUDED





NANO_BEGIN_NAMESPACE

template <typename...>
struct common_type;

template <typename... Ts>
using common_type_t = typename common_type<Ts...>::type;

namespace detail {

template <typename T, typename U>
constexpr bool same_decayed_v =
        std::is_same<T, std::decay_t<T>>::value &&
        std::is_same<U, std::decay_t<U>>::value;

template <typename T, typename U>
using ternary_return_t =
    std::decay_t<decltype(false ? std::declval<T>() : std::declval<U>())>;

template <typename, typename, typename = void>
struct binary_common_type {};

template <typename T, typename U>
struct binary_common_type<T, U,
        std::enable_if_t<!same_decayed_v<T, U>>>
    : nano::common_type<std::decay_t<T>, std::decay_t<U>> {};

template <typename T, typename U>
struct binary_common_type<T, U,
        std::enable_if_t<same_decayed_v<T, U> &&
                         exists_v<ternary_return_t, T, U>>> {
    using type = ternary_return_t<T, U>;
};

template <typename T, typename U>
struct binary_common_type<T, U,
        std::enable_if_t<same_decayed_v<T, U> &&
                         !exists_v<ternary_return_t, T, U> &&
                          exists_v<cond_res_t, cref_t<T>, cref_t<U>>>> {
    using type = std::decay_t<cond_res_t<cref_t<T>, cref_t<U>>>;
};

}

template <>
struct common_type<> {};

template <typename T>
struct common_type<T> : common_type<T, T> {};

template <typename T, typename U>
struct common_type<T, U>
    : detail::binary_common_type<T, U> {};

namespace detail {

template <typename Void, typename...>
struct multiple_common_type {};

template <typename T1, typename T2, typename... R>
struct multiple_common_type<std::void_t<common_type_t<T1, T2>>, T1, T2, R...>
    : common_type<common_type_t<T1, T2>, R...> {};

}


template <typename T1, typename T2, typename... R>
struct common_type<T1, T2, R...>
    : detail::multiple_common_type<void, T1, T2, R...> {};

NANO_END_NAMESPACE

#endif


#endif


#include <utility>

NANO_BEGIN_NAMESPACE

// [concept.same]
template <typename T, typename U>
NANO_CONCEPT same_as = std::is_same_v<T, U>;

// [concept.derived]
namespace detail {

struct derived_from_concept {
    template <typename, typename>
    static auto test(long) -> std::false_type;

    template <typename Derived, typename Base>
    static auto test(int) -> std::enable_if_t<
        std::is_base_of_v<Base, Derived> &&
        std::is_convertible_v<const volatile Derived*, const volatile Base*>,
        std::true_type>;

};

}

template <typename Derived, typename Base>
NANO_CONCEPT derived_from =
    decltype(detail::derived_from_concept::test<Derived, Base>(0))::value;

// [concept.convertible]
namespace detail {

struct convertible_to_concept {
    template <typename From, typename To>
    auto requires_(std::add_rvalue_reference_t<From> (&f)())
        -> decltype(static_cast<To>(f()));
};

} // namespace detail

template <typename From, typename To>
NANO_CONCEPT convertible_to =
    std::is_convertible_v<From, To> &&
    detail::requires_<detail::convertible_to_concept, From, To>;

// [concept.commonref]
namespace detail {

struct common_reference_with_concept {
    template <typename T, typename U>
    static auto test(long) -> std::false_type;

    template <typename T, typename U>
    static auto test(int) -> std::enable_if_t<
        same_as<common_reference_t<T, U>, common_reference_t<U, T>> &&
            convertible_to<T, common_reference_t<T, U>> &&
            convertible_to<U, common_reference_t<T, U>>,
        std::true_type>;
};

}

template <typename T, typename U>
NANO_CONCEPT common_reference_with =
    decltype(detail::common_reference_with_concept::test<T, U>(0))::value;

// [concepts.common]
namespace detail {

struct common_with_concept {
    template <typename T, typename U>
    auto requires_() -> decltype(
        static_cast<common_type_t<T, U>>(std::declval<T>()),
        static_cast<common_type_t<T, U>>(std::declval<U>())
    );

    template <typename, typename>
    static auto test(long) -> std::false_type;

    template <typename T, typename U>
    static auto test(int) -> std::enable_if_t<
        same_as<common_type_t<T, U>, common_type_t<U, T>> &&
        detail::requires_<common_with_concept, T, U> &&
        common_reference_with<
            std::add_lvalue_reference_t<const T>,
            std::add_lvalue_reference_t<const U>> &&
        common_reference_with<
            std::add_lvalue_reference_t<common_type_t<T, U>>,
            common_reference_t<
                std::add_lvalue_reference_t<const T>,
                std::add_lvalue_reference_t<const U>>>,
        std::true_type>;

};

}

template <typename T, typename U>
NANO_CONCEPT common_with =
    decltype(detail::common_with_concept::test<T, U>(0))::value;

// [concept.arithmetic]
template <typename T>
NANO_CONCEPT integral = std::is_integral_v<T>;

template <typename T>
NANO_CONCEPT signed_integral = integral<T> && std::is_signed_v<T>;

template <typename T>
NANO_CONCEPT unsigned_integral = integral<T> && !signed_integral<T>;

template <typename T>
NANO_CONCEPT floating_point = std::is_floating_point_v<T>;

// [concept.assignable]

namespace detail {

struct assignable_from_concept {
    template <typename LHS, typename RHS>
    auto requires_(LHS lhs, RHS&& rhs) -> decltype(
        requires_expr<same_as<decltype(lhs = std::forward<RHS>(rhs)), LHS>>{});

    template <typename, typename>
    static auto test(long) -> std::false_type;

    template <typename LHS, typename RHS>
    static auto test(int) -> std::enable_if_t<
        std::is_lvalue_reference_v<LHS> &&
        common_reference_with<
            const std::remove_reference_t<LHS>&,
            const std::remove_reference_t<RHS>&> &&
        detail::requires_<assignable_from_concept, LHS, RHS>,
        std::true_type>;
};

} // namespace detail

template <typename LHS, typename RHS>
NANO_CONCEPT assignable_from =
    decltype(detail::assignable_from_concept::test<LHS, RHS>(0))::value;

// [concept.destructible]
template <typename T>
NANO_CONCEPT destructible = std::is_nothrow_destructible_v<T>;

// [concept.constructible]
template <typename T, typename... Args>
NANO_CONCEPT constructible_from =
    destructible<T> && std::is_constructible_v<T, Args...>;

// [concept.default.init]
namespace detail {

template <typename, typename = void>
inline constexpr bool is_default_initializable = false;

// Thanks to Damian Jarek on Slack for this formulation
template <typename T>
inline constexpr bool
is_default_initializable<T, std::void_t<decltype(::new T)>> = true;

struct default_initializable_concept {
    template <typename T, typename = decltype(T{})>
    auto requires_() -> void;
};

}

template <typename T>
NANO_CONCEPT default_initializable =
    constructible_from<T> &&
    detail::requires_<detail::default_initializable_concept, T> &&
    detail::is_default_initializable<T>;

// [concept.moveconstructible]
template <typename T>
NANO_CONCEPT move_constructible =
    constructible_from<T, T> && convertible_to<T, T>;

// [concept.copyconstructible]
namespace detail {

struct copy_constructible_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename T>
    static auto test(int) -> std::enable_if_t<
        move_constructible<T> &&
        constructible_from<T, T&> && convertible_to<T&, T> &&
        constructible_from<T, const T&> && convertible_to<const T&, T> &&
        constructible_from<T, const T> && convertible_to<const T, T>,
        std::true_type>;
};

}

template <typename T>
NANO_CONCEPT copy_constructible =
    decltype(detail::copy_constructible_concept::test<T>(0))::value;

NANO_END_NAMESPACE

#endif

// nanorange/detail/concepts/swappable.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_CONCEPTS_SWAPPABLE_HPP_INCLUDED
#define NANORANGE_DETAIL_CONCEPTS_SWAPPABLE_HPP_INCLUDED


// nanorange/detail/swap.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_SWAP_HPP_INCLUDED
#define NANORANGE_DETAIL_SWAP_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

// [range.swap]

namespace detail {
namespace swap_ {

template <typename T>
void swap(T&, T&) = delete;

template <typename T, std::size_t N>
void swap(T (&)[N], T (&)[N]) = delete;

struct fn {
private:
    template <typename T, typename U>
    static constexpr auto impl(T&& t, U&& u, priority_tag<2>) noexcept(
        noexcept(swap(std::forward<T>(t), std::forward<U>(u))))
        -> decltype(static_cast<void>(swap(std::forward<T>(t),
                                           std::forward<U>(u))))
    {
        (void) swap(std::forward<T>(t), std::forward<U>(u));
    }

    template <typename T, typename U, std::size_t N, typename F = fn>
    static constexpr auto
        impl(T (&t)[N], U (&u)[N],
             priority_tag<1>) noexcept(noexcept(std::declval<F&>()(*t, *u)))
            -> decltype(std::declval<F&>()(*t, *u))
    {
        for (std::size_t i = 0; i < N; ++i) {
            fn::impl(t[i], u[i], priority_tag<2>{});
        }
    }

    template <typename T>
    static constexpr auto impl(T& a, T& b, priority_tag<0>) noexcept(
        std::is_nothrow_move_constructible<T>::value&&
            std::is_nothrow_assignable<T&, T>::value)
        -> std::enable_if_t<move_constructible<T> && assignable_from<T&, T>>
    {
        T temp = std::move(a);
        a = std::move(b);
        b = std::move(temp);
    }

public:
    template <typename T, typename U>
    constexpr auto operator()(T&& t, U&& u) const
        noexcept(noexcept(fn::impl(std::forward<T>(t), std::forward<U>(u),
                                   priority_tag<2>{})))
            -> decltype(fn::impl(std::forward<T>(t), std::forward<U>(u),
                                 priority_tag<2>{}))
    {
        return fn::impl(std::forward<T>(t), std::forward<U>(u),
                        priority_tag<2>{});
    }
};

} // end namespace swap_
} // end namespace detail

NANO_INLINE_VAR(detail::swap_::fn, swap)

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

// [concept.swappable]
namespace detail {

struct swappable_concept {
    template <typename T>
    auto requires_(T& a, T& b) -> decltype(ranges::swap(a, b));
};

}

template <typename T>
NANO_CONCEPT swappable = detail::requires_<detail::swappable_concept, T>;

namespace detail {

struct swappable_with_concept {
    template <typename T, typename U>
    auto requires_(T&& t, U&& u) -> decltype(
        ranges::swap(std::forward<T>(t), std::forward<T>(t)),
        ranges::swap(std::forward<U>(u), std::forward<U>(u)),
        ranges::swap(std::forward<T>(t), std::forward<U>(u)),
        ranges::swap(std::forward<U>(u), std::forward<T>(t))
    );

    template <typename, typename>
    static auto test(long) -> std::false_type;

    template <typename T, typename U>
    static auto test(int) -> std::enable_if_t<
        common_reference_with<
            const std::remove_reference_t<T>&,
            const std::remove_reference_t<U>&> &&
        detail::requires_<swappable_with_concept, T, U>,
        std::true_type>;

};

} // namespace detail

template <typename T, typename U>
NANO_CONCEPT swappable_with =
    decltype(detail::swappable_with_concept::test<T, U>(0))::value;

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

// [concept.boolean_testable]
namespace detail {

template <typename T>
NANO_CONCEPT boolean_testable_impl = convertible_to<T, bool>;

struct boolean_testable_concept {
    template <typename T>
    auto requires_(T&& t) ->
        requires_expr<boolean_testable_impl<decltype(!std::forward<T>(t))>>;
};

template <typename T>
NANO_CONCEPT boolean_testable =
    boolean_testable_impl<T> &&
    detail::requires_<boolean_testable_concept, T>;

} // namespace detail

// [concept.equalitycomparable]
namespace detail {

struct weakly_equality_comparable_with_concept {
    template <typename T, typename U>
    auto requires_(const std::remove_reference_t<T>& t,
                   const std::remove_reference_t<U>& u)
        -> decltype(
            requires_expr<boolean_testable<decltype(t == u)>>{},
            requires_expr<boolean_testable<decltype(t != u)>>{},
            requires_expr<boolean_testable<decltype(u == t)>>{},
            requires_expr<boolean_testable<decltype(u != t)>>{});
};

template <typename T, typename U>
NANO_CONCEPT weakly_equality_comparable_with =
    requires_<weakly_equality_comparable_with_concept, T, U>;

} // namespace detail

template <typename T>
NANO_CONCEPT equality_comparable = detail::weakly_equality_comparable_with<T, T>;

namespace detail {

struct equality_comparable_with_concept {
    template <typename, typename>
    static auto test(long) -> std::false_type;

    template <typename T, typename U>
    static auto test(int) -> std::enable_if_t<
        equality_comparable<T> && equality_comparable<U> &&
        common_reference_with<const std::remove_reference_t<T>&,
                              const std::remove_reference_t<U>&> &&
        equality_comparable<
            common_reference_t<
                const std::remove_reference_t<T>&,
                const std::remove_reference_t<U>&>> &&
        weakly_equality_comparable_with<T, U>,
        std::true_type>;
};

}

template <typename T, typename U>
NANO_CONCEPT equality_comparable_with =
    decltype(detail::equality_comparable_with_concept::test<T, U>(0))::value;

// [concepts.totallyordered]
namespace detail {

struct partially_ordered_with_concept {
    template <typename T, typename U>
    auto requires_(const std::remove_reference_t<T>& t,
                   const std::remove_reference_t<U>& u)
    -> decltype(requires_expr<boolean_testable<decltype(t < u)>>{},
        requires_expr<boolean_testable<decltype(t > u)>>{},
        requires_expr<boolean_testable<decltype(t <= u)>>{},
        requires_expr<boolean_testable<decltype(t >= u)>>{},
        requires_expr<boolean_testable<decltype(u < t)>>{},
        requires_expr<boolean_testable<decltype(u > t)>>{},
        requires_expr<boolean_testable<decltype(u <= t)>>{},
        requires_expr<boolean_testable<decltype(u >= t)>>{});
};

template <typename T, typename U>
NANO_CONCEPT partially_ordered_with =
    detail::requires_<detail::partially_ordered_with_concept, T, U>;

}

template <typename T>
NANO_CONCEPT totally_ordered =
    equality_comparable<T> && detail::partially_ordered_with<T, T>;

namespace detail {

struct totally_ordered_with_concept {
    template <typename, typename>
    static auto test(long) -> std::false_type;

    template <typename T, typename U>
    static auto test(int) -> std::enable_if_t<
        totally_ordered<T> && totally_ordered<U> &&
        equality_comparable_with<T, U> &&
        totally_ordered<
            common_reference_t<
                const std::remove_reference_t<T>&,
                const std::remove_reference_t<U>&>> &&
        partially_ordered_with<T, U>,
        std::true_type>;
};

} // namespace detail

template <typename T, typename U>
NANO_CONCEPT totally_ordered_with =
    decltype(detail::totally_ordered_with_concept::test<T, U>(0))::value;

NANO_END_NAMESPACE

#endif



#include <functional>
#include <utility>

NANO_BEGIN_NAMESPACE

// [range.comparisons]

struct equal_to {
    template <typename T, typename U>
    constexpr auto operator()(T&& t, U&& u) const
        -> std::enable_if_t<equality_comparable_with<T, U>, bool>
    {
        return std::equal_to<>{}(std::forward<T>(t), std::forward<U>(u));
    }

    using is_transparent = std::true_type;
};

struct not_equal_to {
    template <typename T, typename U>
    constexpr auto operator()(T&& t, U&& u) const
        -> std::enable_if_t<equality_comparable_with<T, U>, bool>
    {
        return !ranges::equal_to{}(std::forward<T>(t), std::forward<U>(u));
    }

    using is_transparent = std::true_type;
};

struct less {
    template <typename T, typename U>
    constexpr auto operator()(T&& t, U&& u) const
        -> std::enable_if_t<totally_ordered_with<T, U>, bool>
    {
        return std::less<>{}(std::forward<T>(t), std::forward<U>(u));
    }

    using is_transparent = std::true_type;
};

struct greater {
    template <typename T, typename U>
    constexpr auto operator()(T&& t, U&& u) const
        -> std::enable_if_t<totally_ordered_with<T, U>, bool>
    {
        return ranges::less{}(std::forward<U>(u), std::forward<T>(t));
    }

    using is_transparent = std::true_type;
};

struct greater_equal {
    template <typename T, typename U>
    constexpr auto operator()(T&& t, U&& u) const
    -> std::enable_if_t<totally_ordered_with<T, U>, bool>
    {
        return !ranges::less{}(std::forward<T>(t), std::forward<U>(u));
    }

    using is_transparent = std::true_type;
};

struct less_equal {
    template <typename T, typename U>
    constexpr auto operator()(T&& t, U&& u) const
        -> std::enable_if_t<totally_ordered_with<T, U>, bool>
    {
        return !ranges::less{}(std::forward<U>(u), std::forward<T>(t));
    }

    using is_transparent = std::true_type;
};

NANO_END_NAMESPACE

#endif
// nanorange/detail/functional/identity.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_FUNCTIONAL_IDENTITY_HPP_INCLUDED
#define NANORANGE_DETAIL_FUNCTIONAL_IDENTITY_HPP_INCLUDED



#include <type_traits>
#include <utility>

NANO_BEGIN_NAMESPACE

struct identity {
    template <typename T>
    constexpr T&& operator()(T&& t) const noexcept
    {
        return std::forward<T>(t);
    }

    using is_transparent = std::true_type;
};

NANO_END_NAMESPACE

#endif

// nanorange/detail/iterator/indirect_callable_concepts.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_ITERATOR_INDIRECT_CALLABLE_CONCEPTS_HPP_INCLUDED
#define NANORANGE_DETAIL_ITERATOR_INDIRECT_CALLABLE_CONCEPTS_HPP_INCLUDED

// nanorange/detail/iterator/concepts.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_ITERATOR_CONCEPTS_HPP_INCLUDED
#define NANORANGE_DETAIL_ITERATOR_CONCEPTS_HPP_INCLUDED

// nanorange/detail/concepts/object.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_CONCEPTS_OBJECT_HPP_INCLUDED
#define NANORANGE_DETAIL_CONCEPTS_OBJECT_HPP_INCLUDED



// nanorange/detail/functional/invoke.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_FUNCTIONAL_INVOKE_HPP_INCLUDED
#define NANORANGE_DETAIL_FUNCTIONAL_INVOKE_HPP_INCLUDED




#include <functional>

NANO_BEGIN_NAMESPACE

namespace detail {

// This is a reimplementation of std::invoke, which for some stupid
// reason is not constexpr in C++17
inline namespace invoke_ {

template <typename>
constexpr bool is_reference_wrapper_v = false;

template <typename T>
constexpr bool is_reference_wrapper_v<std::reference_wrapper<T>> = true;

struct fn {
private:
    template <class Base, class T, class Derived, class... Args>
    static constexpr auto
    impl(T Base::*pmf, Derived&& ref,
         Args&&... args) noexcept(noexcept((std::forward<Derived>(ref).*
                                            pmf)(std::forward<Args>(args)...)))
        -> std::enable_if_t<
            std::is_function<T>::value &&
                std::is_base_of<Base, std::decay_t<Derived>>::value,
            decltype((std::forward<Derived>(ref).*
                      pmf)(std::forward<Args>(args)...))>
    {
        return (std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...);
    }

    template <class Base, class T, class RefWrap, class... Args>
    static constexpr auto
    impl(T Base::*pmf, RefWrap&& ref, Args&&... args) noexcept(
        noexcept((ref.get().*pmf)(std::forward<Args>(args)...)))
        -> std::enable_if_t<std::is_function<T>::value &&
                                is_reference_wrapper_v<std::decay_t<RefWrap>>,
                            decltype((ref.get().*
                                      pmf)(std::forward<Args>(args)...))>
    {
        return (ref.get().*pmf)(std::forward<Args>(args)...);
    }

    template <class Base, class T, class Pointer, class... Args>
    static constexpr auto
    impl(T Base::*pmf, Pointer&& ptr,
         Args&&... args) noexcept(noexcept(((*std::forward<Pointer>(ptr)).*
                                            pmf)(std::forward<Args>(args)...)))
        -> std::enable_if_t<
            std::is_function<T>::value &&
                !is_reference_wrapper_v<std::decay_t<Pointer>> &&
                !std::is_base_of<Base, std::decay_t<Pointer>>::value,
            decltype(((*std::forward<Pointer>(ptr)).*
                      pmf)(std::forward<Args>(args)...))>
    {
        return ((*std::forward<Pointer>(ptr)).*
                pmf)(std::forward<Args>(args)...);
    }

    template <class Base, class T, class Derived>
    static constexpr auto
    impl(T Base::*pmd,
         Derived&& ref) noexcept(noexcept(std::forward<Derived>(ref).*pmd))
        -> std::enable_if_t<
            !std::is_function<T>::value &&
                std::is_base_of<Base, std::decay_t<Derived>>::value,
            decltype(std::forward<Derived>(ref).*pmd)>
    {
        return std::forward<Derived>(ref).*pmd;
    }

    template <class Base, class T, class RefWrap>
    static constexpr auto impl(T Base::*pmd,
                               RefWrap&& ref) noexcept(noexcept(ref.get().*pmd))
        -> std::enable_if_t<!std::is_function<T>::value &&
                                is_reference_wrapper_v<std::decay_t<RefWrap>>,
                            decltype(ref.get().*pmd)>
    {
        return ref.get().*pmd;
    }

    template <class Base, class T, class Pointer>
    static constexpr auto
    impl(T Base::*pmd,
         Pointer&& ptr) noexcept(noexcept((*std::forward<Pointer>(ptr)).*pmd))
        -> std::enable_if_t<
            !std::is_function<T>::value &&
                !is_reference_wrapper_v<std::decay_t<Pointer>> &&
                !std::is_base_of<Base, std::decay_t<Pointer>>::value,
            decltype((*std::forward<Pointer>(ptr)).*pmd)>
    {
        return (*std::forward<Pointer>(ptr)).*pmd;
    }

    template <class F, class... Args>
    static constexpr auto impl(F&& f, Args&&... args) noexcept(
        noexcept(std::forward<F>(f)(std::forward<Args>(args)...)))
        -> std::enable_if_t<
            !std::is_member_pointer<std::decay_t<F>>::value,
            decltype(std::forward<F>(f)(std::forward<Args>(args)...))>
    {
        return std::forward<F>(f)(std::forward<Args>(args)...);
    }

public:
    template <typename F, typename... Args>
    constexpr auto operator()(F&& f, Args&&... args) const noexcept(
        noexcept(fn::impl(std::forward<F>(f), std::forward<Args>(args)...)))
        -> decltype(fn::impl(std::forward<F>(f), std::forward<Args>(args)...))
    {
        return fn::impl(std::forward<F>(f), std::forward<Args>(args)...);
    }
};

} // namespace invoke_
} // namespace detail

NANO_INLINE_VAR(nano::detail::invoke_::fn, invoke)

namespace detail {

template <typename Void, typename, typename...>
struct invoke_result_helper {
};

template <typename F, typename... Args>
struct invoke_result_helper<
    std::void_t<decltype(nano::invoke(std::declval<F>(), std::declval<Args>()...))>,
    F, Args...> {
    using type =
        decltype(nano::invoke(std::declval<F>(), std::declval<Args>()...));
};

} // namespace detail

template <typename F, typename... Args>
struct invoke_result : detail::invoke_result_helper<void, F, Args...> {
};

template <typename F, typename... Args>
using invoke_result_t = typename invoke_result<F, Args...>::type;

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

// [concept.movable]
namespace detail {

struct movable_concept {
    template <typename T>
    static auto test(long) -> std::false_type;

    template <typename T>
    static auto test(int) -> std::enable_if_t<
        std::is_object_v<T> && move_constructible<T> &&
        assignable_from<T&, T> && swappable<T>,
        std::true_type>;
};
}

template <typename T>
NANO_CONCEPT movable = decltype(detail::movable_concept::test<T>(0))::value;

// [concept.copyable]
namespace detail {

struct copyable_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename T>
    static auto test(int) -> std::enable_if_t<
        copy_constructible<T> && movable<T> && assignable_from<T&, T&> &&
        assignable_from<T&, const T&> && assignable_from<T&, const T>,
        std::true_type>;

};

}

template <typename T>
NANO_CONCEPT copyable = decltype(detail::copyable_concept::test<T>(0))::value;

// [concept.semiregular]
template <typename T>
NANO_CONCEPT semiregular = copyable<T> && default_initializable<T>;

// [concept.regular]
template <typename T>
NANO_CONCEPT regular = semiregular<T> && equality_comparable<T>;

// [concept.invocable]
namespace detail {

struct invocable_concept {
    // FIXME (Clang): https://bugs.llvm.org/show_bug.cgi?id=21446
#if (defined(__clang_major__) && (defined(__apple_build_version__) ||__clang_major__ < 7))
    template <typename F, typename... Args>
    auto requires_(F&& f, Args&&... args) -> invoke_result_t<F, Args...>;
#else
    template <typename F, typename... Args>
    auto requires_(F&& f, Args&&... args) -> decltype(
        nano::invoke(std::forward<F>(f), std::forward<Args>(args)...)
    );
#endif
};

} // namespace detail

template <typename F, typename... Args>
NANO_CONCEPT invocable = detail::requires_<detail::invocable_concept, F, Args...>;

// [concept.regularinvocable]
template <typename F, typename... Args>
NANO_CONCEPT regular_invocable = invocable<F, Args...>;

// [concept.predicate]
namespace detail {

struct predicate_concept {
    template <typename, typename...>
    static auto test(long) -> std::false_type;

    template <typename F, typename... Args>
    static auto test(int) -> std::enable_if_t<
        regular_invocable<F, Args...> &&
        boolean_testable<invoke_result_t<F, Args...>>,
        std::true_type>;

};

}

template <typename F, typename... Args>
NANO_CONCEPT predicate = decltype(detail::predicate_concept::test<F, Args...>(0))::value;

// [concept.relation]
template <typename R, typename T, typename U>
NANO_CONCEPT relation =
    predicate<R, T, T> && predicate<R, U, U> &&
    predicate<R, T, U> && predicate<R, U, T>;

// [concept.equiv]
template <typename R, typename T, typename U>
NANO_CONCEPT equivalence_relation = relation<R, T, U>;

// [concept.strictweakorder]
template <typename R, typename T, typename U>
NANO_CONCEPT strict_weak_order = relation<R, T, U>;

NANO_END_NAMESPACE

#endif

// nanorange/detail/iterator/associated_types.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_ITERATOR_ASSOCIATED_TYPES_HPP_INCLUDED
#define NANORANGE_DETAIL_ITERATOR_ASSOCIATED_TYPES_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename>
struct incrementable_traits;

namespace detail {

struct empty {};

template <typename T>
struct with_difference_type {
    using difference_type = T;
};

template <typename, typename = void>
struct incrementable_traits_helper {
};

// Workaround for GCC silliness: void* has no difference_type
// FIXME: This is required to stop WeaklyIncrementable<void*> being a hard error
// Can we formulate the concept differently to avoid the need for this hack?
template <>
struct incrementable_traits_helper<void*> {};

template <typename T>
struct incrementable_traits_helper<T*>
    : detail::conditional_t<std::is_object<T>::value,
            with_difference_type<std::ptrdiff_t>, empty> {
};

template <class I>
struct incrementable_traits_helper<const I> : incrementable_traits<std::decay_t<I>> {
};

template <typename, typename = void>
struct has_member_difference_type : std::false_type {};

template <typename T>
struct has_member_difference_type<T, std::void_t<typename T::difference_type>>
    : std::true_type{};

template <typename T>
constexpr bool has_member_difference_type_v =
        has_member_difference_type<T>::value;

template <typename T>
struct incrementable_traits_helper<T, std::enable_if_t<has_member_difference_type_v<T>>> {
    using difference_type = typename T::difference_type;
};

template <typename T>
struct incrementable_traits_helper<
    T, std::enable_if_t<!std::is_pointer<T>::value &&
                        !has_member_difference_type_v<T> &&
                        integral<decltype(std::declval<const T&>() -
                                          std::declval<const T&>())>>>
    : with_difference_type<std::make_signed_t<decltype(std::declval<T>() - std::declval<T>())>> {
};

} // namespace detail

template <typename T>
struct incrementable_traits : detail::incrementable_traits_helper<T> {
};

template <typename T>
using iter_difference_t = typename incrementable_traits<T>::difference_type;

// [range.iterator.assoc.types.value_type]

template <typename>
struct readable_traits;

namespace detail {

template <typename T>
struct with_value_type {
    using value_type = T;
};

template <typename, typename = void>
struct readable_traits_helper {};

template <typename T>
struct readable_traits_helper<T*>
    : detail::conditional_t<std::is_object<T>::value,
            with_value_type<std::remove_cv_t<T>>,
            empty> {};

template <typename I>
struct readable_traits_helper<I, std::enable_if_t<std::is_array<I>::value>>
    : readable_traits<std::decay_t<I>> {};

template <typename I>
struct readable_traits_helper<const I, std::enable_if_t<!std::is_array<I>::value>>
    : readable_traits<std::decay_t<I>> {};

template <typename T, typename V = typename T::value_type>
struct member_value_type
    : detail::conditional_t<std::is_object<V>::value,
            with_value_type<V>, empty> {};

template <typename T, typename E = typename T::element_type>
struct member_element_type
    : detail::conditional_t<std::is_object<E>::value,
            with_value_type<std::remove_cv_t<E>>, empty> {};

template <typename T>
using member_value_type_t = typename T::value_type;

template <typename T>
constexpr bool has_member_value_type_v = exists_v<member_value_type_t, T>;

template <typename T>
using member_element_type_t = typename T::element_type;

template <typename T>
constexpr bool has_member_element_type_v = exists_v<member_element_type_t, T>;

template <typename T>
struct readable_traits_helper<T, std::enable_if_t<
    has_member_value_type_v<T> &&
    !has_member_element_type_v<T>>>
    : member_value_type<T> {};

template <typename T>
struct readable_traits_helper<T, std::enable_if_t<
    has_member_element_type_v<T> &&
    !has_member_value_type_v<T>>>
    : member_element_type<T> {};

// A type which has both value_type and element_type members must specialise
// readable_traits to tell us which one to prefer -- see
// https://github.com/ericniebler/stl2/issues/562
template <typename T>
struct readable_traits_helper<T, std::enable_if_t<
    has_member_element_type_v<T> &&
    has_member_value_type_v<T>>>
{};

} // namespace detail

template <typename T>
struct readable_traits : detail::readable_traits_helper<T> {};

template <typename T>
using iter_value_t = typename readable_traits<T>::value_type;

NANO_END_NAMESPACE

#endif

// nanorange/detail/iterator/traits.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_ITERATOR_TRAITS_HPP_INCLUDED
#define NANORANGE_DETAIL_ITERATOR_TRAITS_HPP_INCLUDED

// nanorange/detail/iterator/dereferenceable.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_ITERATOR_DEREFERENCABLE_HPP_INCLUDED
#define NANORANGE_DETAIL_ITERATOR_DEREFERENCABLE_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

template <typename T>
using with_reference = T&;

struct can_reference_concept {
    template <typename T>
    auto requires_() -> with_reference<T>;
};

template <typename T>
NANO_CONCEPT can_reference = detail::requires_<can_reference_concept, T>;

struct dereferencable_concept {
    template <typename T>
    auto requires_(T& t)
        -> decltype(requires_expr<can_reference<decltype(*t)>>{});
};

template <typename T>
NANO_CONCEPT dereferenceable = requires_<dereferencable_concept, T>;

// GCC and Clang allow dereferencing void* as an extension.
// Let's kill that off now.

template <>
NANO_CONCEPT dereferenceable<void*> = false;

} // namespace detail

NANO_END_NAMESPACE

#endif

// nanorange/detail/iterator/iter_move.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_ITERATOR_ITER_MOVE_HPP_INCLUDED
#define NANORANGE_DETAIL_ITERATOR_ITER_MOVE_HPP_INCLUDED



#include <utility>

NANO_BEGIN_NAMESPACE

namespace detail {
namespace iter_move_ {

void iter_move();

struct fn {
private:
    template <typename T>
    static constexpr auto impl(T&& t, priority_tag<2>)
        noexcept(noexcept(iter_move(t)))
        -> decltype(iter_move(t))
    {
        return iter_move(t);
    }

    template <typename T>
    static constexpr auto impl(T&& t, priority_tag<1>) noexcept(
        noexcept(std::move(*std::declval<T&&>())))
       -> std::enable_if_t<std::is_lvalue_reference<decltype(*std::forward<T>(t))>::value,
                           decltype(std::move(*std::forward<T>(t)))>
    {
        return std::move(*std::forward<T>(t));
    }

    template <typename T>
    static constexpr auto
    impl(T&& t, priority_tag<0>)
        noexcept(noexcept(*std::forward<T>(t)))
        -> decltype(*std::forward<T>(t))
    {
        return *std::forward<T>(t);
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(fn::impl(std::forward<T>(t), priority_tag<2>{})))
            -> decltype(fn::impl(std::forward<T>(t), priority_tag<2>{}))
    {
        return fn::impl(std::forward<T>(t), priority_tag<2>{});
    }
};

} // namespace iter_move_
} // namespace detail

NANO_INLINE_VAR(detail::iter_move_::fn, iter_move)

NANO_END_NAMESPACE

#endif


#include <iterator>

NANO_BEGIN_NAMESPACE

// [range.iterator.assoc.types.iterator_category]
using std::bidirectional_iterator_tag;
using std::forward_iterator_tag;
using std::input_iterator_tag;
using std::output_iterator_tag;
using std::random_access_iterator_tag;

struct contiguous_iterator_tag : random_access_iterator_tag {};

template <typename T>
struct iterator_category;

namespace detail {

template <typename T, typename = void>
struct iterator_category_ {
};

template <typename T>
struct iterator_category_<T*>
    : std::enable_if<std::is_object<T>::value, contiguous_iterator_tag> {
};

template <typename T>
struct iterator_category_<const T> : iterator_category<T> {
};

template <typename T>
struct iterator_category_<T, std::void_t<typename T::iterator_category>> {
    using type = typename T::iterator_category;
};

} // namespace detail

template <typename T>
struct iterator_category : detail::iterator_category_<T> {
};

template <typename T>
using iterator_category_t = typename iterator_category<T>::type;

namespace detail {

template <typename T, typename = void>
struct legacy_iterator_category
    : iterator_category<T> {};

template <typename T>
struct legacy_iterator_category<T,
        std::enable_if_t<std::is_same<iterator_category_t<T>, contiguous_iterator_tag>::value>>
{
    using type = random_access_iterator_tag;
};

template <typename T>
using legacy_iterator_category_t = typename legacy_iterator_category<T>::type;

}

template <typename T>
using iter_reference_t = std::enable_if_t<detail::dereferenceable<T>,
                                          decltype(*std::declval<T&>())>;

namespace detail {

struct iter_rvalue_reference_req {
    template <typename T>
    auto requires_(T& t) -> decltype(
        ranges::iter_move(t),
        requires_expr<can_reference<decltype(ranges::iter_move(t))>>{});
};

}

template <typename T>
using iter_rvalue_reference_t = std::enable_if_t<
        detail::dereferenceable<T> &&
        detail::requires_<detail::iter_rvalue_reference_req, T>,
        decltype(ranges::iter_move(std::declval<T&>()))>;

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

// [iterators.concept.readable]
namespace detail {

struct readable_concept {
    template <typename In>
    auto requires_() -> decltype(
        std::declval<iter_value_t<In>>(),
        std::declval<iter_reference_t<In>>(),
        std::declval<iter_rvalue_reference_t<In>>()
    );

    template <typename>
    static auto test(long) -> std::false_type;

    template <typename In>
    static auto test(int) -> std::enable_if_t<
        detail::requires_<readable_concept, In> &&
        common_reference_with<iter_reference_t<In>&&, iter_value_t<In>&> &&
        common_reference_with<iter_reference_t<In>&&, iter_rvalue_reference_t<In>&&> &&
        common_reference_with<iter_rvalue_reference_t<In>&&, const iter_value_t<In>&>,
        std::true_type>;
};

} // namespace detail

template <typename In>
NANO_CONCEPT readable = decltype(detail::readable_concept::test<In>(0))::value;

// [iterator.concept.writable]
namespace detail {

struct writable_concept {
    template <typename Out, typename T>
    auto requires_(Out&& o, T&& t) -> decltype(
        *o = std::forward<T>(t),
        *std::forward<Out>(o) = std::forward<T>(t),
        const_cast<const iter_reference_t<Out>&&>(*o) = std::forward<T>(t),
        const_cast<const iter_reference_t<Out>&&>(*std::forward<Out>(o)) =
            std::forward<T>(t));
};

} // namespace detail

template <typename Out, typename T>
NANO_CONCEPT writable = detail::requires_<detail::writable_concept, Out, T>;

// [iterator.concept.weaklyincrementable]

namespace detail {

template <typename T>
inline constexpr bool is_integer_like = integral<T>;

template <typename T>
inline constexpr bool is_signed_integer_like = signed_integral<T>;

struct weakly_incrementable_concept {
    template <typename I>
    auto requires_(I i) -> decltype(
        std::declval<iter_difference_t<I>>(),
        requires_expr<is_signed_integer_like<iter_difference_t<I>>>{},
        requires_expr<same_as<decltype(++i), I&>>{},
        i++
    );

};

} // namespace detail

template <typename I>
NANO_CONCEPT weakly_incrementable = default_initializable<I> && movable<I> &&
    detail::requires_<detail::weakly_incrementable_concept, I>;

// [iterator.concept.incrementable]
namespace detail {

struct incrementable_concept {
    template <typename I>
    auto requires_(I i) -> decltype(
        requires_expr<same_as<decltype(i++), I>>{}
    );
};

} // namespace detail

template <typename I>
NANO_CONCEPT incrementable = regular<I> && weakly_incrementable<I> &&
    detail::requires_<detail::incrementable_concept, I>;

// [iterator.concept.iterator]

namespace detail {

struct input_or_output_iterator_concept {
    template <typename I>
    auto requires_(I i) -> decltype(
            requires_expr<can_reference<decltype(*i)>>{});
};

} // namespace detail

template <typename I>
NANO_CONCEPT input_or_output_iterator =
    detail::requires_<detail::input_or_output_iterator_concept, I> &&
    weakly_incrementable<I>;

// [iterator.concept.sentinel]

template <typename S, typename I>
NANO_CONCEPT sentinel_for =
    semiregular<S> && input_or_output_iterator<I> && 
    detail::weakly_equality_comparable_with<S, I>;

// [iterator.concept.sizedsentinel]

template <typename S, typename I>
inline constexpr bool disable_sized_sentinel = false;

namespace detail {

struct sized_sentinel_for_concept {
    template <typename S, typename I>
    auto requires_(const S& s, const I& i)
        -> decltype(requires_expr<same_as<decltype(s - i), iter_difference_t<I>>>{},
                    requires_expr<same_as<decltype(i - s), iter_difference_t<I>>>{});
};

} // namespace detail

template <typename S, typename I>
NANO_CONCEPT sized_sentinel_for =
    sentinel_for<S, I> &&
    !disable_sized_sentinel<std::remove_cv_t<S>, std::remove_cv_t<I>> &&
    detail::requires_<detail::sized_sentinel_for_concept, S, I>;

// This is a hack, but I'm fed up with my tests breaking because GCC
// has a silly extension
template <typename S>
NANO_CONCEPT sized_sentinel_for<S, void*> = false;

template <typename I>
NANO_CONCEPT sized_sentinel_for<void*, I> = false;

template <>
NANO_CONCEPT sized_sentinel_for<void*, void*> = false;

// [iterator.concept.input]

namespace detail {

// FIXME: Use ITER_CONCEPT, not iterator_category_t
struct input_iterator_concept {
    template <typename I>
    auto requires_() -> iterator_category_t<I>;

    template <typename>
    static auto test(long) -> std::false_type;

    template <typename I>
    static auto test(int) -> std::enable_if_t<
        input_or_output_iterator<I> &&
        readable<I> &&
        detail::requires_<input_iterator_concept, I> &&
        derived_from<iterator_category_t<I>, input_iterator_tag>,
        std::true_type>;
};

}

template <typename I>
NANO_CONCEPT input_iterator =
    decltype(detail::input_iterator_concept::test<I>(0))::value;

// [iterator.concept.output]

namespace detail {

struct output_iterator_concept {
    template <typename I, typename T>
    auto requires_(I i, T&& t) -> decltype(
        *i++ = std::forward<T>(t)
    );
};

} // namespace detail

template <typename I, typename T>
NANO_CONCEPT output_iterator =
    input_or_output_iterator<I> &&
    writable<I, T> &&
    detail::requires_<detail::output_iterator_concept, I, T>;

// [ranges.iterators.forward]

namespace detail {

struct forward_iterator_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename I>
    static auto test(int) -> std::enable_if_t<
        input_iterator<I> &&
        derived_from<iterator_category_t<I>, forward_iterator_tag> &&
        incrementable<I> &&
        sentinel_for<I, I>,
        std::true_type>;


};

}

template <typename I>
NANO_CONCEPT forward_iterator =
    decltype(detail::forward_iterator_concept::test<I>(0))::value;

// [iterator.concept.bidir]
namespace detail {

struct bidirectional_iterator_concept {
    template <typename I>
    auto requires_(I i) -> decltype(
        requires_expr<same_as<decltype(--i), I&>>{},
        requires_expr<same_as<decltype(i--), I>>{}
    );

    template <typename>
    static auto test(long) -> std::false_type;

    template <typename I>
    static auto test(int) -> std::enable_if_t<
        forward_iterator<I> &&
        derived_from<iterator_category_t<I>, bidirectional_iterator_tag> &&
        detail::requires_<bidirectional_iterator_concept, I>,
        std::true_type>;
};

} // namespace detail

template <typename I>
NANO_CONCEPT bidirectional_iterator =
    decltype(detail::bidirectional_iterator_concept::test<I>(0))::value;

// [iterator.concept.random.access]

namespace detail {

struct random_access_iterator_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename I>
    static auto test(int) -> std::enable_if_t<
        bidirectional_iterator<I> &&
        derived_from<iterator_category_t<I>, random_access_iterator_tag> &&
        totally_ordered<I> &&
        sized_sentinel_for<I, I> &&
        detail::requires_<random_access_iterator_concept, I>,
        std::true_type>;

    template <typename I>
    auto requires_(I i, const I j, const iter_difference_t<I> n) -> decltype(
        requires_expr<same_as<decltype(i += n), I&>>{},
        requires_expr<same_as<decltype(j +  n), I>>{},
#ifndef _MSC_VER
        requires_expr<same_as<decltype(n +  j), I>>{}, // FIXME: MSVC doesn't like this when I = int*
#endif
        requires_expr<same_as<decltype(i -= n), I&>>{},
        requires_expr<same_as<decltype(j -  n), I>>{},
        requires_expr<same_as<decltype(j[n]), iter_reference_t<I>>>{}
    );
};

} // namespace detail

template <typename I>
NANO_CONCEPT random_access_iterator =
        decltype(detail::random_access_iterator_concept::test<I>(0))::value;

namespace detail {

struct contiguous_iterator_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename I>
    static auto test(int) -> std::enable_if_t<
        random_access_iterator<I> &&
        derived_from<iterator_category_t<I>, contiguous_iterator_tag> &&
        std::is_lvalue_reference_v<iter_reference_t<I>> &&
        same_as<iter_value_t<I>, remove_cvref_t<iter_reference_t<I>>>,
        std::true_type>;
};

}

template <typename I>
NANO_CONCEPT contiguous_iterator =
    decltype(detail::contiguous_iterator_concept::test<I>(0))::value;

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

template <typename T>
using iter_common_reference_t = std::enable_if_t<readable<T>,
        common_reference_t<iter_reference_t<T>, iter_value_t<T>&>>;

// [iterator.concept.indirectinvocable]
namespace detail {

struct indirect_unary_invocable_concept {
    template <typename, typename>
    static auto test(long) -> std::false_type;

    template <typename F, typename I>
    static auto test(int) -> std::enable_if_t<
        readable<I> &&
        copy_constructible<F> &&
        invocable<F&, iter_value_t<I>&> &&
        invocable<F&, iter_reference_t<I>> &&
        invocable<F&, iter_common_reference_t<I>> &&
        common_reference_with<
            invoke_result_t<F&, iter_value_t<I>&>,
            invoke_result_t<F&, iter_reference_t<I>&>>,
        std::true_type>;
};

}

template <typename F, typename I>
NANO_CONCEPT indirect_unary_invocable =
        decltype(detail::indirect_unary_invocable_concept::test<F, I>(0))::value;

namespace detail {

struct indirect_regular_unary_invocable_concept {
    template <typename, typename>
    static auto test(long) -> std::false_type;

    template <typename F, typename I>
    static auto test(int) -> std::enable_if_t<
        readable<I> &&
        copy_constructible<F> &&
        regular_invocable<F&, iter_value_t<I>&> &&
        regular_invocable<F&, iter_reference_t<I>> &&
        regular_invocable<F&, iter_common_reference_t<I>> &&
        common_reference_with<
            invoke_result_t<F&, iter_value_t<I>&>,
            invoke_result_t<F&, iter_reference_t<I>&>>,
        std::true_type>;
};

}

template <typename F, typename I>
NANO_CONCEPT indirect_regular_unary_invocable =
        decltype(detail::indirect_regular_unary_invocable_concept::test<F, I>(0))::value;

namespace detail {

struct indirect_unary_predicate_concept {
    template <typename, typename>
    static auto test(long) -> std::false_type;

    template <typename F, typename I>
    static auto test(int) -> std::enable_if_t<
        readable<I> &&
        copy_constructible<F> &&
        predicate<F&, iter_value_t<I>&> &&
        predicate<F&, iter_reference_t<I>> &&
        predicate<F&, iter_common_reference_t<I>>,
        std::true_type>;
};

}

template <typename F, typename I>
NANO_CONCEPT indirect_unary_predicate =
        decltype(detail::indirect_unary_predicate_concept::test<F, I>(0))::value;

namespace detail {

struct indirect_relation_concept {
    template <typename F, typename I1, typename I2>
    static auto test(long) -> std::false_type;

    template <typename F, typename I1, typename I2>
    static auto test(int) -> std::enable_if_t<
        readable<I1> && readable<I2> &&
        copy_constructible<F> &&
        relation<F&, iter_value_t<I1>&, iter_value_t<I2>&> &&
        relation<F&, iter_value_t<I1>&, iter_reference_t<I2>> &&
        relation<F&, iter_reference_t<I1>, iter_value_t<I2>&> &&
        relation<F&, iter_reference_t<I1>, iter_reference_t<I2>> &&
        relation<F&, iter_common_reference_t<I1>, iter_common_reference_t<I2>>,
        std::true_type>;
};

}

template <typename F, typename I1, typename I2 = I1>
NANO_CONCEPT indirect_relation =
    decltype(detail::indirect_relation_concept::test<F, I1, I2>(0))::value;

namespace detail {

struct indirect_strict_weak_order_concept {
    template <typename, typename, typename>
    static auto test(long) -> std::false_type;

    template <typename F, typename I1, typename I2>
    static auto test(int) -> std::enable_if_t<
        readable<I1> && readable<I2> &&
        copy_constructible<F> &&
        strict_weak_order<F&, iter_value_t<I1>&, iter_value_t<I2>&> &&
        strict_weak_order<F&, iter_value_t<I1>&, iter_reference_t<I2>> &&
        strict_weak_order<F&, iter_reference_t<I1>, iter_value_t<I2>&> &&
        strict_weak_order<F&, iter_reference_t<I1>, iter_reference_t<I2>> &&
        strict_weak_order<F&, iter_common_reference_t<I1>, iter_common_reference_t<I2>>,
        std::true_type>;
};

}

template <typename F, typename I1, typename I2 = I1>
NANO_CONCEPT indirect_strict_weak_order =
    decltype(detail::indirect_strict_weak_order_concept::test<F, I1, I2>(0))::value;

template <typename F, typename... Is>
using indirect_result_t = std::enable_if_t<
    (readable<Is> && ... ) && invocable<F, iter_reference_t<Is>...>,
    invoke_result_t<F, iter_reference_t<Is>...>>;

// [alg.req.ind.move]

namespace detail {

struct indirectly_movable_concept {
    template <typename, typename>
    static auto test(long) -> std::false_type;

    template <typename In, typename Out>
    static auto test(int) -> std::enable_if_t<
        readable<In> &&
        writable<Out, iter_rvalue_reference_t<In>>,
        std::true_type>;
};

}

template <typename In, typename Out>
NANO_CONCEPT indirectly_movable =
    decltype(detail::indirectly_movable_concept::test<In, Out>(0))::value;

namespace detail {

struct indirectly_movable_storable_concept {
    template <typename In, typename Out>
    static auto test(long) -> std::false_type;

    template <typename In, typename Out>
    static auto test(int) -> std::enable_if_t<
        indirectly_movable<In, Out> &&
        writable<Out, iter_value_t<In>> &&
        movable<iter_value_t<In>> &&
        constructible_from<iter_value_t<In>, iter_rvalue_reference_t<In>> &&
        assignable_from<iter_value_t<In>&, iter_rvalue_reference_t<In>>,
        std::true_type>;
};

}

template <typename In, typename Out>
NANO_CONCEPT indirectly_movable_storable =
    decltype(detail::indirectly_movable_storable_concept::test<In, Out>(0))::value;

// [alg.req.ind.copy]
namespace detail {

struct indirectly_copyable_concept {
    template <typename, typename>
    static auto test(long) -> std::false_type;

    template <typename In, typename Out>
    static auto test(int) -> std::enable_if_t<
        readable<In> &&
        writable<Out, iter_reference_t<In>>,
        std::true_type>;
};

}

template <typename In, typename Out>
NANO_CONCEPT indirectly_copyable =
    decltype(detail::indirectly_copyable_concept::test<In, Out>(0))::value;

namespace detail {

struct indirectly_copyable_storable_concept {
    template <typename, typename>
    static auto test(long) -> std::false_type;

    template <typename In, typename Out>
    static auto test(int) -> std::enable_if_t<
        indirectly_copyable<In, Out> &&
        writable<Out, const iter_value_t<In>&> &&
        copyable<iter_value_t<In>> &&
        constructible_from<iter_value_t<In>, iter_reference_t<In>> &&
        assignable_from<iter_value_t<In>&, iter_reference_t<In>>,
        std::true_type>;
};

}

template <typename In, typename Out>
NANO_CONCEPT indirectly_copyable_storable =
    decltype(detail::indirectly_copyable_storable_concept::test<In, Out>(0))::value;

NANO_END_NAMESPACE

#endif

// nanorange/detail/iterator/iter_swap.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_ITERATOR_ITER_SWAP_HPP_INCLUDED
#define NANORANGE_DETAIL_ITERATOR_ITER_SWAP_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

namespace detail {
namespace iter_swap_ {

// ADL "poison pill"
template <typename I1, typename I2>
void iter_swap(I1, I2) = delete;

// FIXME MSVC: add a second (redundant) poison pill
template <typename I>
void iter_swap(I, I) = delete;

struct fn {
private:
    template <typename X, typename Y>
    static constexpr iter_value_t<std::remove_reference_t<X>>
    iter_exchange_move(X&& x, Y&& y) noexcept(
        noexcept(iter_value_t<std::remove_reference_t<X>>(ranges::iter_move(x))) &&
        noexcept(*x = ranges::iter_move(y)))
    {
        iter_value_t<std::remove_reference_t<X>> old_value(ranges::iter_move(x));
        *x = ranges::iter_move(y);
        return old_value;
    }

    template <typename T, typename U>
    static constexpr auto impl(T&& t, U&& u, priority_tag<2>) noexcept(
        noexcept((void) (iter_swap(std::forward<T>(t), std::forward<U>(u)))))
        -> decltype((void) (iter_swap(std::forward<T>(t), std::forward<U>(u))))
    {
        (void) iter_swap(std::forward<T>(t), std::forward<U>(u));
    }

    template <typename T, typename U>
    static constexpr auto impl(T&& t, U&& u, priority_tag<1>) noexcept(
        noexcept(ranges::swap(*std::forward<T>(t), *std::forward<U>(u))))
        -> std::enable_if_t<
                readable<std::remove_reference_t<T>> &&
                readable<std::remove_reference_t<U>> &&
                swappable_with<iter_reference_t<T>, iter_reference_t<U>>>
    {
        ranges::swap(*std::forward<T>(t), *std::forward<U>(u));
    }

    template <typename T, typename U>
    static constexpr auto impl(T&& t, U&& u, priority_tag<0>) noexcept(noexcept(
        *t = fn::iter_exchange_move(std::forward<U>(u), std::forward<T>(t))))
        -> std::enable_if_t<indirectly_movable_storable<T, U> &&
                            indirectly_movable_storable<U, T>>
    {
        return *t = fn::iter_exchange_move(std::forward<U>(u),
                                           std::forward<T>(t));
    }

public:
    template <typename T, typename U>
    constexpr auto operator()(T&& t, U&& u) const
        noexcept(noexcept(fn::impl(std::forward<T>(t), std::forward<U>(u),
                                   priority_tag<2>{})))
            -> decltype(fn::impl(std::forward<T>(t), std::forward<U>(u),
                                 priority_tag<2>{}))
    {
        return fn::impl(std::forward<T>(t), std::forward<U>(u),
                        priority_tag<2>{});
    }
};
}
} // namespace detail

NANO_INLINE_VAR(detail::iter_swap_::fn, iter_swap)

NANO_END_NAMESPACE

#endif

// nanorange/detail/iterator/projected.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_ITERATOR_PROJECTED_HPP_INCLUDED
#define NANORANGE_DETAIL_ITERATOR_PROJECTED_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

// [range.projected]

//template <typename I, typename Proj>
//struct projected;

namespace detail {

template <typename, typename, typename = void>
struct projected_helper {
};

template <typename I, typename Proj>
struct projected_helper<
    I, Proj,
    std::enable_if_t<readable<I> &&
                     indirect_regular_unary_invocable<Proj, I>>> {
    using value_type = remove_cvref_t<indirect_result_t<Proj&, I>>;

    indirect_result_t<Proj&, I> operator*() const;
};

template <typename, typename, typename = void>
struct projected_difference_t_helper {};

template <typename I, typename Proj>
struct projected_difference_t_helper<I, Proj, std::enable_if_t<
    weakly_incrementable<I>>> {
    using difference_type = iter_difference_t<I>;
};

} // namespace detail

template <typename I, typename Proj>
using projected = detail::conditional_t<
    same_as<Proj, identity>, I, detail::projected_helper<I, Proj>>;

template <typename I, typename Proj>
struct incrementable_traits<detail::projected_helper<I, Proj>>
    : detail::projected_difference_t_helper<I, Proj> {};

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE


namespace detail {

struct indirectly_swappable_concept {
    template <typename I1, typename I2>
    auto requires_(I1& i1, I2& i2) -> decltype(
        ranges::iter_swap(i1, i1),
        ranges::iter_swap(i2, i2),
        ranges::iter_swap(i1, i2),
        ranges::iter_swap(i2, i1));
};

} // namespace detail

template <typename I1, typename I2 = I1>
NANO_CONCEPT indirectly_swappable =
    readable<I1> && readable<I2> &&
    detail::requires_<detail::indirectly_swappable_concept, I1, I2>;

// [alg.req.ind.cmp]

template <typename I1, typename I2, typename R,
          typename P1 = identity, typename P2 = identity>
NANO_CONCEPT indirectly_comparable =
    indirect_relation<R, projected<I1, P1>, projected<I2, P2>>;

// [alg.req.permutable]

template <typename I>
NANO_CONCEPT permutable =
    forward_iterator<I> &&
    indirectly_movable_storable<I, I> &&
    indirectly_swappable<I, I>;

// [alg.req.mergeable]

template <typename I1, typename I2, typename Out, typename R = ranges::less,
          typename P1 = identity, typename P2 = identity>
NANO_CONCEPT mergeable =
    input_iterator<I1> &&
    input_iterator<I2> &&
    weakly_incrementable<Out> &&
    indirectly_copyable<I1, Out> &&
    indirectly_copyable<I2, Out> &&
    indirect_strict_weak_order<R, projected<I1, P1>, projected<I2, P2>>;

// [alg.req.sortable]

template <typename I, typename R = ranges::less, typename P = identity>
NANO_CONCEPT sortable =
    permutable<I> &&
    indirect_strict_weak_order<R, projected<I, P>>;

NANO_END_NAMESPACE

#endif







// nanorange/detail/ranges/access.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_RANGES_ACCESS_HPP_INCLUDED
#define NANORANGE_DETAIL_RANGES_ACCESS_HPP_INCLUDED

// nanorange/detail/ranges/begin_end.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_RANGES_BEGIN_END_HPP_INCLUDED
#define NANORANGE_DETAIL_RANGES_BEGIN_END_HPP_INCLUDED

// nanorange/detail/functional/decay_copy.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_FUNCTIONAL_DECAY_COPY_HPP_INCLUDED
#define NANORANGE_DETAIL_FUNCTIONAL_DECAY_COPY_HPP_INCLUDED



#include <type_traits>
#include <utility>

NANO_BEGIN_NAMESPACE

namespace detail {

template <typename T>
constexpr std::decay_t<T> decay_copy(T &&t) noexcept(
    noexcept(static_cast<std::decay_t<T>>(std::forward<T>(t))))
{
    return std::forward<T>(t);
}

} // namespace detail

NANO_END_NAMESPACE

#endif


// nanorange/detail/ranges/borrowed_range.hpp
//
// Copyright (c) 2020 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_RANGES_BORROWED_RANGE_HPP_INCLUDED
#define NANORANGE_DETAIL_RANGES_BORROWED_RANGE_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

template <typename>
inline constexpr bool enable_borrowed_range = false;

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

// [range.access.begin]

namespace detail {
namespace begin_ {

template <typename T>
void begin(T&) = delete;

template <typename T>
void begin(const T&) = delete;

struct fn {
private:
    template <typename T,
              std::enable_if_t<
                  !std::is_lvalue_reference_v<T> &&
                  !enable_borrowed_range<std::remove_cv_t<T>>, int> = 0>
    static constexpr void impl(T&&, priority_tag<3>) = delete;

    template <typename T,
              std::enable_if_t<std::is_array_v<remove_cvref_t<T>>, int> = 0>
    static constexpr auto impl(T&& t, priority_tag<2>) noexcept
        -> decltype(t + 0)
    {
        return t + 0;
    }

    template <typename T>
    static constexpr auto
    impl(T&& t, priority_tag<1>)
        noexcept(noexcept(decay_copy(std::forward<T>(t).begin())))
        -> std::enable_if_t<
            input_or_output_iterator<
                decltype(decay_copy(std::forward<T>(t).begin()))>,
                decltype(decay_copy(std::forward<T>(t).begin()))>
    {
        return decay_copy(t.begin());
    }

    template <typename T>
    static constexpr auto impl(T&& t, priority_tag<0>) noexcept(
        noexcept(decay_copy(begin(std::forward<T>(t)))))
        -> std::enable_if_t<input_or_output_iterator<decltype(decay_copy(begin(std::forward<T>(t))))>,
            decltype(decay_copy(begin(std::forward<T>(t))))>
    {
        return decay_copy(begin(std::forward<T>(t)));
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(fn::impl(std::forward<T>(t), priority_tag<3>{})))
            -> decltype(fn::impl(std::forward<T>(t), priority_tag<3>{}))
    {
        return fn::impl(std::forward<T>(t), priority_tag<3>{});
    }
};

} // namespace begin_
} // namespace detail

NANO_INLINE_VAR(detail::begin_::fn, begin)

namespace detail {
namespace end_ {

template <typename T>
void end(T&) = delete;

template <typename T>
void end(const T&) = delete;

struct fn {
private:
    template <typename T,
              std::enable_if_t<
                  !std::is_lvalue_reference_v<T> &&
                  !enable_borrowed_range<std::remove_cv_t<T>>, int> = 0>
    static constexpr void impl(T&&, priority_tag<3>) = delete;

    template <typename T,
              std::enable_if_t<std::is_array_v<remove_cvref_t<T>>, int> = 0>
    static constexpr auto impl(T&& t, priority_tag<2>) noexcept
        -> decltype(t + std::extent_v<remove_cvref_t<T>>)
    {
        return t + std::extent_v<remove_cvref_t<T>>;
    }

    template <typename T,
              typename S = decltype(decay_copy(std::declval<T>().end())),
              typename I = decltype(ranges::begin(std::declval<T>()))>
    static constexpr auto
    impl(T&& t, priority_tag<1>)
        noexcept(noexcept(decay_copy(std::forward<T>(t).end())))
        -> std::enable_if_t<sentinel_for<S, I>,
                            decltype(decay_copy(std::forward<T>(t).end()))>
    {
        return decay_copy(std::forward<T>(t).end());
    }

    template <typename T,
              typename S = decltype(decay_copy(end(std::declval<T>()))),
              typename I = decltype(ranges::begin(std::declval<T>()))>
    static constexpr auto impl(T&& t, priority_tag<0>) noexcept(
        noexcept(decay_copy(end(std::forward<T>(t)))))
        -> std::enable_if_t<sentinel_for<S, I>, S>
    {
        return decay_copy(end(std::forward<T>(t)));
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(fn::impl(std::forward<T>(t), priority_tag<3>{})))
            -> decltype(fn::impl(std::forward<T>(t), priority_tag<3>{}))
    {
        return fn::impl(std::forward<T>(t), priority_tag<3>{});
    }
};

} // namespace end_
} // namespace detail

NANO_INLINE_VAR(detail::end_::fn, end)

// [range.access.cbegin]

namespace detail {
namespace cbegin_ {

struct fn {
private:
    template <typename T, typename U = std::remove_reference_t<T>,
              std::enable_if_t<std::is_lvalue_reference_v<T>, int> = 0>
    static constexpr auto impl(T&& t)
        noexcept(noexcept(ranges::begin(static_cast<const U&>(t))))
        -> decltype(ranges::begin(static_cast<const U&>(t)))
    {
        return ranges::begin(static_cast<const U&>(t));
    }

    template <typename T,
              std::enable_if_t<!std::is_lvalue_reference_v<T>, int> = 0>
    static constexpr auto impl(T&& t)
        noexcept(noexcept(ranges::begin(static_cast<const T&&>(std::forward<T>(t)))))
        -> decltype(ranges::begin(static_cast<const T&&>(std::forward<T>(t))))
    {
        return ranges::begin(static_cast<const T&&>(std::forward<T>(t)));
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(fn::impl(std::forward<T>(t))))
        -> decltype(fn::impl(std::forward<T>(t)))
    {
        return fn::impl(std::forward<T>(t));
    }
};

} // namespace cbegin_
} // namespace detail

NANO_INLINE_VAR(detail::cbegin_::fn, cbegin)

// [ranges.access.cend]

namespace detail {
namespace cend_ {

struct fn {
private:
    template <typename T, typename U = std::remove_reference_t<T>,
              std::enable_if_t<std::is_lvalue_reference_v<T>, int> = 0>
    static constexpr auto impl(T&& t)
        noexcept(noexcept(ranges::end(static_cast<const U&>(t))))
        -> decltype(ranges::end(static_cast<const U&>(t)))
    {
        return ranges::end(static_cast<const U&>(t));
    }

    template <typename T,
              std::enable_if_t<!std::is_lvalue_reference_v<T>, int> = 0>
    static constexpr auto impl(T&& t)
        noexcept(noexcept(ranges::end(static_cast<const T&&>(std::forward<T>(t)))))
        -> decltype(ranges::end(static_cast<const T&&>(std::forward<T>(t))))
    {
        return ranges::end(static_cast<const T&&>(std::forward<T>(t)));
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(fn::impl(std::forward<T>(t))))
        -> decltype(fn::impl(std::forward<T>(t)))
    {
        return fn::impl(std::forward<T>(t));
    }

};

} // namespace cend_
} // namespace detail

NANO_INLINE_VAR(detail::cend_::fn, cend)

NANO_END_NAMESPACE

#endif

// nanorange/iterator/reverse_iterator.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_REVERSE_ITERATOR_HPP_INCLUDED
#define NANORANGE_ITERATOR_REVERSE_ITERATOR_HPP_INCLUDED





// nanorange/iterator/operations.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_OPERATIONS_HPP_INCLUDED
#define NANORANGE_ITERATOR_OPERATIONS_HPP_INCLUDED



// nanorange/detail/ranges/concepts.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_RANGES_CONCEPTS_HPP_INCLUDED
#define NANORANGE_DETAIL_RANGES_CONCEPTS_HPP_INCLUDED

// nanorange/detail/ranges/basic_range_types.hpp
//
// Copyright (c) 2020 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_RANGES_BASIC_RANGE_TYPES_HPP_INCLUDED
#define NANORANGE_DETAIL_RANGES_BASIC_RANGE_TYPES_HPP_INCLUDED


// nanorange/detail/ranges/range_concept.hpp
//
// Copyright (c) 2020 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_RANGES_RANGE_CONCEPT_HPP_INCLUDED
#define NANORANGE_DETAIL_RANGES_RANGE_CONCEPT_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct range_concept {
    template <typename T>
    auto requires_(T& t) -> decltype(ranges::begin(t), ranges::end(t));
};

} // namespace detail

template <typename T>
NANO_CONCEPT range = detail::requires_<detail::range_concept, T>;

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

template <typename T>
using iterator_t = decltype(ranges::begin(std::declval<T&>()));

template <typename R>
using sentinel_t =
    std::enable_if_t<range<R>, decltype(ranges::end(std::declval<R&>()))>;

template <typename R>
using range_difference_t =
    std::enable_if_t<range<R>, iter_difference_t<iterator_t<R>>>;

template <typename R>
using range_value_t = std::enable_if_t<range<R>, iter_value_t<iterator_t<R>>>;

template <typename R>
using range_reference_t =
    std::enable_if_t<range<R>, iter_reference_t<iterator_t<R>>>;

template <typename R>
using range_rvalue_reference_t =
    std::enable_if_t<range<R>, iter_rvalue_reference_t<iterator_t<R>>>;

NANO_END_NAMESPACE

#endif



// nanorange/detail/ranges/primitives.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_RANGES_PRIMITIVES_HPP_INCLUDED
#define NANORANGE_DETAIL_RANGES_PRIMITIVES_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

// [range.prim.size]

template <typename>
inline constexpr bool disable_sized_range = false;

namespace detail {
namespace size_ {

template <typename T>
void size(T&&) = delete;

// For some reason MSVC doesn't mind poison pills,
// as long as there are two
template <typename T>
void size(T&) = delete;

struct fn {
private:
    template <typename T, std::size_t N>
    static constexpr std::size_t impl(const T(&&)[N], priority_tag<3>) noexcept
    {
        return N;
    }

    template <typename T, std::size_t N>
    static constexpr std::size_t impl(const T (&)[N], priority_tag<3>) noexcept
    {
        return N;
    }

    template <typename T,
              typename I = decltype(decay_copy(std::declval<T>().size()))>
    static constexpr auto impl(T&& t, priority_tag<2>) noexcept(
        noexcept(decay_copy(std::forward<T>(t).size())))
        -> std::enable_if_t<
            integral<I> && !disable_sized_range<remove_cvref_t<T>>, I>
    {
        return decay_copy(std::forward<T>(t).size());
    }

    template <typename T,
              typename I = decltype(decay_copy(size(std::declval<T>())))>
    static constexpr auto impl(T&& t, priority_tag<1>) noexcept(
        noexcept(decay_copy(size(std::forward<T>(t)))))
        -> std::enable_if_t<
            integral<I> && !disable_sized_range<remove_cvref_t<T>>, I>
    {
        return decay_copy(size(std::forward<T>(t)));
    }

    template <typename T,
              typename I = decltype(ranges::begin(std::declval<T>())),
              typename S = decltype(ranges::end(std::declval<T>())),
              typename D = decltype(decay_copy(std::declval<S>() -
                                               std::declval<I>()))>
    static constexpr auto impl(T&& t, priority_tag<0>) noexcept(
        noexcept(decay_copy(ranges::end(t) - ranges::begin(t))))
        -> std::enable_if_t<
            !std::is_array<remove_cvref_t<T>>::value && // MSVC sillyness?
                sized_sentinel_for<S, I> && forward_iterator<I>,
            D>
    {
        return decay_copy(ranges::end(t) - ranges::begin(t));
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(fn::impl(std::forward<T>(t), priority_tag<3>{})))
            -> decltype(fn::impl(std::forward<T>(t), priority_tag<3>{}))
    {
        return fn::impl(std::forward<T>(t), priority_tag<3>{});
    }
};

} // namespace size_
} // namespace detail

NANO_INLINE_VAR(detail::size_::fn, size)

// [range.prim.ssize]

namespace detail {
namespace ssize_ {

struct fn {
private:
    template <typename T>
    using ssize_return_t =
        std::conditional_t<sizeof(range_difference_t<T>) <
                               sizeof(std::ptrdiff_t),
                           std::ptrdiff_t, range_difference_t<T>>;

    template <typename T>
    static constexpr auto
    impl(T&& t) noexcept(noexcept(ranges::size(std::forward<T>(t))))
        -> decltype(ranges::size(std::forward<T>(t)), ssize_return_t<T>())
    {
        return static_cast<ssize_return_t<T>>(ranges::size(std::forward<T>(t)));
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(fn::impl(std::forward<T>(t))))
            -> decltype(fn::impl(std::forward<T>(t)))
    {
        return fn::impl(std::forward<T>(t));
    }
};

} // namespace ssize_
} // namespace detail

NANO_INLINE_VAR(detail::ssize_::fn, ssize)

// [range.prim.empty]

namespace detail {
namespace empty_ {

struct fn {
private:
    template <typename T>
    static constexpr auto
    impl(T&& t,
         priority_tag<2>) noexcept(noexcept((bool(std::forward<T>(t).empty()))))
        -> decltype((bool(std::forward<T>(t).empty())))
    {
        return bool((std::forward<T>(t).empty()));
    }

    template <typename T>
    static constexpr auto impl(T&& t, priority_tag<1>) noexcept(
        noexcept(ranges::size(std::forward<T>(t)) == 0))
        -> decltype(ranges::size(std::forward<T>(t)) == 0)
    {
        return ranges::size(std::forward<T>(t)) == 0;
    }

    template <typename T,
              typename I = decltype(ranges::begin(std::declval<T>()))>
    static constexpr auto
    impl(T&& t,
         priority_tag<0>) noexcept(noexcept(ranges::begin(t) == ranges::end(t)))
        -> std::enable_if_t<forward_iterator<I>,
                            decltype(ranges::begin(t) == ranges::end(t))>
    {
        return ranges::begin(t) == ranges::end(t);
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(fn::impl(std::forward<T>(t), priority_tag<2>{})))
            -> decltype(fn::impl(std::forward<T>(t), priority_tag<2>{}))
    {
        return fn::impl(std::forward<T>(t), priority_tag<2>{});
    }
};

} // namespace empty_
} // namespace detail

NANO_INLINE_VAR(detail::empty_::fn, empty)

namespace detail {

template <typename, typename = void>
inline constexpr bool is_object_pointer_v = false;

template <typename P>
inline constexpr bool is_object_pointer_v<P,
    std::enable_if_t<std::is_pointer_v<P> &&
                     std::is_object_v<iter_value_t<P>>>> = true;

namespace data_ {

struct fn {
private:
    template <typename T, typename D = decltype(decay_copy(std::declval<T&>().data()))>
    static constexpr auto
    impl(T& t, priority_tag<1>) noexcept(noexcept(decay_copy(t.data())))
        -> std::enable_if_t<is_object_pointer_v<D>, D>
    {
        return t.data();
    }

    template <typename T>
    static constexpr auto
    impl(T&& t,
         priority_tag<0>) noexcept(noexcept(ranges::begin(std::forward<T>(t))))
        -> std::enable_if_t<
            is_object_pointer_v<decltype(ranges::begin(std::forward<T>(t)))>,
            decltype(ranges::begin(std::forward<T>(t)))>
    {
        return ranges::begin(std::forward<T>(t));
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(fn::impl(std::forward<T>(t), priority_tag<1>{})))
            -> decltype(fn::impl(std::forward<T>(t), priority_tag<1>{}))
    {
        return fn::impl(std::forward<T>(t), priority_tag<1>{});
    }
};

} // namespace data_
} // namespace detail

NANO_INLINE_VAR(detail::data_::fn, data)

namespace detail {
namespace cdata_ {

struct fn {
private:
    template <typename T, typename U = std::remove_reference_t<T>,
	      std::enable_if_t<std::is_lvalue_reference_v<T>, int> = 0>
    static constexpr auto impl(T&& t)
        noexcept(noexcept(ranges::data(static_cast<const U&>(t))))
	-> decltype(ranges::data(static_cast<const U&>(t)))
    {
	return ranges::data(static_cast<const U&>(t));
    }

    template <typename T,
	      std::enable_if_t<!std::is_lvalue_reference_v<T>, int> = 0>
    static constexpr auto impl(T&& t)
        noexcept(noexcept(ranges::data(static_cast<const T&&>(t))))
	-> decltype(ranges::data(static_cast<const T&&>(t)))
    {
	return ranges::data(static_cast<const T&&>(t));
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(fn::impl(std::forward<T>(t))))
	-> decltype(fn::impl(std::forward<T>(t)))
    {
	return fn::impl(std::forward<T>(t));
    }
};

} // namespace cdata_
} // namespace detail

NANO_INLINE_VAR(detail::cdata_::fn, cdata)

NANO_END_NAMESPACE

#endif



#include <initializer_list>

// Avoid dragging in the large <set> and <unordered_set> headers
// This is technically undefined behaviour: define the symbol
// NANORANGE_NO_STD_FORWARD_DECLARATIONS
// to enforce standard-compliant mode
#ifndef NANORANGE_NO_STD_FORWARD_DECLARATIONS
NANO_BEGIN_NAMESPACE_STD
template <typename, typename> class basic_string_view;
template <typename, typename, typename> class set;
template <typename, typename, typename> class multiset;
template <typename, typename, typename, typename> class unordered_set;
template <typename, typename, typename, typename> class unordered_multiset;
template <typename, typename> class match_results;
NANO_END_NAMESPACE_STD
#else
#include <string_view>
#include <regex>
#include <set>
#include <unordered_set>
#endif

NANO_BEGIN_NAMESPACE

template <typename T>
NANO_CONCEPT borrowed_range = range<T> &&
    (std::is_lvalue_reference_v<T> || enable_borrowed_range<remove_cvref_t<T>>);

// Special-case std::string_view
template <typename CharT, typename Traits>
inline constexpr bool
    enable_borrowed_range<std::basic_string_view<CharT, Traits>> = true;


// [range.sized]
namespace detail {

struct sized_range_concept {
    template <typename T>
    auto requires_(T& t) -> decltype(ranges::size(t));
};

} // namespace detail

template <typename T>
NANO_CONCEPT sized_range =
    range<T> &&
    !disable_sized_range<remove_cvref_t<T>> &&
    detail::requires_<detail::sized_range_concept, T>;


// [range.views]
struct view_base { };

namespace detail {

template <typename>
inline constexpr bool is_std_non_view = false;

template <typename T>
inline constexpr bool is_std_non_view<std::initializer_list<T>> = true;

template <typename K, typename C, typename A>
inline constexpr bool is_std_non_view<std::set<K, C, A>> = true;

template <typename K, typename C, typename A>
inline constexpr bool is_std_non_view<std::multiset<K, C, A>> = true;

template <typename K, typename H, typename E, typename A>
inline constexpr bool is_std_non_view<std::unordered_set<K, H, E, A>> = true;

template <typename K, typename H, typename E, typename A>
inline constexpr bool is_std_non_view<std::unordered_multiset<K, H, E, A>> = true;

template <typename B, typename A>
inline constexpr bool is_std_non_view<std::match_results<B, A>> = true;

template <typename T>
constexpr bool enable_view_helper()
{
    if constexpr (derived_from<T, view_base>) {
        return true;
    } else if constexpr (is_std_non_view<T>) {
        return false;
    } else if constexpr (range<T> && range<const T>) {
        return same_as<range_reference_t<T>, range_reference_t<const T>>;
    } else {
        return true;
    }
}

}

template <typename T>
inline constexpr bool enable_view = detail::enable_view_helper<T>();

template <typename T>
NANO_CONCEPT view = range<T> && semiregular<T> && enable_view<T>;

// [range.refinements]
namespace detail {

struct output_range_concept {
    template <typename, typename>
    static auto test(long) -> std::false_type;

    template <typename R, typename T>
    static auto test(int) -> std::enable_if_t<
        range<R> && output_iterator<iterator_t<R>, T>,
        std::true_type>;
};

}

template <typename R, typename T>
NANO_CONCEPT output_range =
    decltype(detail::output_range_concept::test<R, T>(0))::value;

namespace detail {

struct input_range_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename T>
    static auto test(int) -> std::enable_if_t<
        range<T> && input_iterator<iterator_t<T>>,
        std::true_type>;
};

}

template <typename T>
NANO_CONCEPT input_range =
    decltype(detail::input_range_concept::test<T>(0))::value;

namespace detail {

struct forward_range_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename T>
    static auto test(int) -> std::enable_if_t<
        input_range<T> && forward_iterator<iterator_t<T>>,
        std::true_type>;
};

}

template <typename T>
NANO_CONCEPT forward_range =
    decltype(detail::forward_range_concept::test<T>(0))::value;

namespace detail {

struct bidirectional_range_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename T>
    static auto test(int) -> std::enable_if_t<
        forward_range<T> && bidirectional_iterator<iterator_t<T>>,
        std::true_type>;
};

}

template <typename T>
NANO_CONCEPT bidirectional_range =
    decltype(detail::bidirectional_range_concept::test<T>(0))::value;

namespace detail {

struct random_access_range_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename T>
    static auto test(int) -> std::enable_if_t<
        bidirectional_range<T> && random_access_iterator<iterator_t<T>>,
        std::true_type>;
};

}

template <typename T>
NANO_CONCEPT random_access_range =
    decltype(detail::random_access_range_concept::test<T>(0))::value;

namespace detail {

// FIXME: Not to spec
// We only require random_access_iterator, not contiguous_iterator
// This is so that vector::iterator, string::iterator etc can model
// contiguous_range.
// If we do range-v3-style deep integration with iterator_traits then
// this could be fixed
struct contiguous_range_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename T>
    static auto test(int) -> std::enable_if_t<
        random_access_range<T> && /* contiguous_iterator<iterator_t<T>> && */
        detail::requires_<contiguous_range_concept, T>,
        std::true_type>;

    template <typename T>
    auto requires_(T& t) -> decltype(
        requires_expr<same_as<decltype(ranges::data(t)),
                      std::add_pointer_t<range_reference_t<T>>>>{}
    );
};

}

template <typename R>
NANO_CONCEPT contiguous_range =
    decltype(detail::contiguous_range_concept::test<R>(0))::value;

namespace detail {

struct common_range_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename T>
    static auto test(int) -> std::enable_if_t<
        range<T> && same_as<iterator_t<T>, sentinel_t<T>>,
        std::true_type>;
};

}

template <typename T>
NANO_CONCEPT common_range =
    decltype(detail::common_range_concept::test<T>(0))::value;

template <typename T>
NANO_CONCEPT viewable_range =
    range<T> && (borrowed_range<T> || view<remove_cvref_t<T>>);


// [range.dangling]

struct dangling {
    constexpr dangling() noexcept = default;

    template <typename... Args>
    constexpr dangling(Args&&...) noexcept {}
};

template <typename R>
using borrowed_iterator_t = detail::conditional_t<
    borrowed_range<R>, iterator_t<R>, dangling>;

// Helper concepts

namespace detail {

struct simple_view_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename R>
    static auto test(int) -> std::enable_if_t<
        view<R> && range<const R> &&
        same_as<iterator_t<R>, iterator_t<const R>> &&
        same_as<sentinel_t<R>, sentinel_t<const R>>,
        std::true_type>;

};

template <typename R>
NANO_CONCEPT simple_view = decltype(simple_view_concept::test<R>(0))::value;

struct has_arrow_concept {
    template <typename I>
    auto requires_(I i) -> decltype(i.operator->());
};

template <typename I>
NANO_CONCEPT has_arrow = input_iterator<I> &&
    (std::is_pointer_v<I> || detail::requires_<has_arrow_concept, I>);


template <typename T, typename U>
NANO_CONCEPT not_same_as = !same_as<remove_cvref_t<T>, remove_cvref_t<U>>;

}

NANO_END_NAMESPACE

#endif



NANO_BEGIN_NAMESPACE

namespace detail {
namespace advance_ {

struct fn {
private:
    template <typename T>
    static constexpr T abs(T t)
    {
        if (t < T{0}) {
            return -t;
        }
        return t;
    }

    template <typename R>
    static constexpr auto impl(R& r, iter_difference_t<R> n)
        -> std::enable_if_t<random_access_iterator<R>>
    {
        r += n;
    }

    template <typename I>
    static constexpr auto impl(I& i, iter_difference_t<I> n)
        -> std::enable_if_t<bidirectional_iterator<I> &&
                            !random_access_iterator<I>>
    {
        constexpr auto zero = iter_difference_t<I>{0};

        if (n > zero) {
            while (n-- > zero) {
                ++i;
            }
        } else {
            while (n++ < zero) {
                --i;
            }
        }
    }

    template <typename I>
    static constexpr auto impl(I& i, iter_difference_t<I> n)
        -> std::enable_if_t<!bidirectional_iterator<I>>
    {
        while (n-- > iter_difference_t<I>{0}) {
            ++i;
        }
    }

    template <typename I, typename S>
    static constexpr auto impl(I& i, S bound, priority_tag<2>)
        -> std::enable_if_t<assignable_from<I&, S>>
    {
        i = std::move(bound);
    }

    template <typename I, typename S>
    static constexpr auto impl(I& i, S bound, priority_tag<1>)
        -> std::enable_if_t<sized_sentinel_for<S, I>>
    {
        fn::impl(i, bound - i);
    }

    template <typename I, typename S>
    static constexpr void impl(I& i, S bound, priority_tag<0>)
    {
        while (i != bound) {
            ++i;
        }
    }

    template <typename I, typename S>
    static constexpr auto impl(I& i, iter_difference_t<I> n, S bound)
        -> std::enable_if_t<sized_sentinel_for<S, I>, iter_difference_t<I>>
    {
        if (fn::abs(n) >= fn::abs(bound - i)) {
            auto dist = bound - i;
            fn::impl(i, bound, priority_tag<2>{});
            return dist;
        } else {
            fn::impl(i, n);
            return n;
        }
    }

    template <typename I, typename S>
    static constexpr auto impl(I& i, iter_difference_t<I> n, S bound)
        -> std::enable_if_t<bidirectional_iterator<I> && !sized_sentinel_for<S, I>,
                            iter_difference_t<I>>
    {
        constexpr iter_difference_t<I> zero{0};
        iter_difference_t<I> counter{0};

        if (n < zero) {
            do {
                --i;
                --counter; // Yes, really
            } while (++n < zero && i != bound);
        } else {
            while (n-- > zero && i != bound) {
                ++i;
                ++counter;
            }
        }

        return counter;
    }

    template <typename I, typename S>
    static constexpr auto impl(I& i, iter_difference_t<I> n, S bound)
        -> std::enable_if_t<!bidirectional_iterator<I> && !sized_sentinel_for<S, I>,
                            iter_difference_t<I>>
    {
        constexpr iter_difference_t<I> zero{0};
        iter_difference_t<I> counter{0};

        while (n-- > zero && i != bound) {
            ++i;
            ++counter;
        }

        return counter;
    }

public:
    template <typename I>
    constexpr auto operator()(I& i, iter_difference_t<I> n) const
        -> std::enable_if_t<input_or_output_iterator<I>>
    {
        fn::impl(i, n);
    }

    template <typename I, typename S>
    constexpr auto operator()(I& i, S bound) const
        -> std::enable_if_t<input_or_output_iterator<I> && sentinel_for<S, I>>
    {
        fn::impl(i, bound, priority_tag<2>{});
    }

    template <typename I, typename S>
    constexpr auto operator()(I& i, iter_difference_t<I> n, S bound) const
        -> std::enable_if_t<input_or_output_iterator<I> && sentinel_for<S, I>, iter_difference_t<I>>
    {
        return n - fn::impl(i, n, bound);
    }
};

} // namespace advance_
} // namespace detail

NANO_INLINE_VAR(detail::advance_::fn, advance)

namespace detail {
namespace distance_ {

struct fn {
private:
    template <typename I, typename S>
    static constexpr auto impl(I i, S s)
        -> std::enable_if_t<sized_sentinel_for<S, I>, iter_difference_t<I>>
    {
        return s - i;
    }

    template <typename I, typename S>
    static constexpr auto impl(I i, S s)
        -> std::enable_if_t<!sized_sentinel_for<S, I>, iter_difference_t<I>>
    {
        iter_difference_t<I> counter{0};
        while (i != s) {
            ++i;
            ++counter;
        }
        return counter;
    }

    template <typename R>
    static constexpr auto impl(R&& r)
        -> std::enable_if_t<sized_range<R>, iter_difference_t<iterator_t<R>>>
    {
        return static_cast<iter_difference_t<iterator_t<R>>>(ranges::size(r));
    }

    template <typename R>
    static constexpr auto impl(R&& r)
        -> std::enable_if_t<!sized_range<R>, iter_difference_t<iterator_t<R>>>
    {
        return fn::impl(ranges::begin(r), ranges::end(r));
    }

public:
    template <typename I, typename S>
    constexpr auto operator()(I first, S last) const
        -> std::enable_if_t<input_or_output_iterator<I> && sentinel_for<S, I>, iter_difference_t<I>>
    {
        return fn::impl(std::move(first), std::move(last));
    }

    template <typename R>
    constexpr auto operator()(R&& r) const
        -> std::enable_if_t<range<R>, iter_difference_t<iterator_t<R>>>
    {
        return fn::impl(std::forward<R>(r));
    }
};

} // namespace distance_
} // namespace detail

NANO_INLINE_VAR(detail::distance_::fn, distance)

namespace detail {
namespace next_ {

struct fn {
    template <typename I>
    constexpr auto operator()(I x) const -> std::enable_if_t<input_or_output_iterator<I>, I>
    {
        ++x;
        return x;
    }

    template <typename I>
    constexpr auto operator()(I x, iter_difference_t<I> n) const
        -> std::enable_if_t<input_or_output_iterator<I>, I>
    {
        ranges::advance(x, n);
        return x;
    }

    template <typename I, typename S>
    constexpr auto operator()(I x, S bound) const
        -> std::enable_if_t<input_or_output_iterator<I> && sentinel_for<S, I>, I>
    {
        ranges::advance(x, bound);
        return x;
    }

    template <typename I, typename S>
    constexpr auto operator()(I x, iter_difference_t<I> n, S bound) const
        -> std::enable_if_t<input_or_output_iterator<I> && sentinel_for<S, I>, I>
    {
        ranges::advance(x, n, bound);
        return x;
    }
};

} // namespace next_
} // namespace detail

NANO_INLINE_VAR(detail::next_::fn, next)

namespace detail {
namespace prev_ {

struct fn {
    template <typename I>
    constexpr auto operator()(I x) const
        -> std::enable_if_t<bidirectional_iterator<I>, I>
    {
        --x;
        return x;
    }

    template <typename I>
    constexpr auto operator()(I x, iter_difference_t<I> n) const
        -> std::enable_if_t<bidirectional_iterator<I>, I>
    {
        ranges::advance(x, -n);
        return x;
    }

    template <typename I, typename S>
    constexpr auto operator()(I x, iter_difference_t<I> n, S bound) const
        -> std::enable_if_t<bidirectional_iterator<I> && sentinel_for<S, I>, I>
    {
        ranges::advance(x, -n, bound);
        return x;
    }
};

} // namespace prev_
} // namespace detail

NANO_INLINE_VAR(detail::prev_::fn, prev)

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

namespace reverse_iterator_ {

template <typename I>
class reverse_iterator {

    static_assert(bidirectional_iterator<I>,
                  "Template argument to reverse_iterator must model "
                  "BidirectionalIterator");

public:
    using iterator_type = I;
    using difference_type = iter_difference_t<I>;
    using value_type = iter_value_t<I>;
    using iterator_category = detail::legacy_iterator_category_t<I>;
    using reference = iter_reference_t<I>;
    using pointer = I;

    constexpr reverse_iterator() = default;

    explicit constexpr reverse_iterator(I x) : current_(std::move(x)) {}

    template <typename U, std::enable_if_t<convertible_to<U, I>, int> = 0>

    constexpr reverse_iterator(const reverse_iterator<U>& i)
        : current_(i.base())
    {}

    template <typename U, std::enable_if_t<convertible_to<U, I>, int> = 0>

    constexpr reverse_iterator& operator=(const reverse_iterator<U>& i)
    {
        current_ = i.base();
        return *this;
    }

    constexpr I base() const { return current_; }

    constexpr reference operator*() const { return *prev(current_); }

    constexpr pointer operator->() const { return prev(current_); }

    constexpr reverse_iterator& operator++()
    {
        --current_;
        return *this;
    }

    constexpr reverse_iterator operator++(int)
    {
        reverse_iterator tmp = *this;
        --current_;
        return tmp;
    }

    constexpr reverse_iterator& operator--()
    {
        ++current_;
        return *this;
    }

    constexpr reverse_iterator operator--(int)
    {
        reverse_iterator tmp = *this;
        ++current_;
        return tmp;
    }

    template <typename II = I>
    constexpr std::enable_if_t<random_access_iterator<II>, reverse_iterator>
    operator+(difference_type n) const
    {
        return reverse_iterator(current_ - n);
    }

    template <typename II = I>
    constexpr std::enable_if_t<random_access_iterator<II>, reverse_iterator&>
    operator+=(difference_type n)
    {
        current_ -= n;
        return *this;
    }

    template <typename II = I>
    constexpr std::enable_if_t<random_access_iterator<II>, reverse_iterator>
    operator-(difference_type n) const
    {
        return reverse_iterator(current_ + n);
    }

    template <typename II = I>
    constexpr std::enable_if_t<random_access_iterator<II>, reverse_iterator&>
    operator-=(difference_type n)
    {
        current_ += n;
        return *this;
    }

    template <typename II = I>
    constexpr std::enable_if_t<random_access_iterator<II>, reference>
    operator[](difference_type n) const
    {
        return current_[-n - 1];
    }

    friend constexpr iter_rvalue_reference_t<I>
    iter_move(const reverse_iterator& i) noexcept(
        noexcept(ranges::iter_move(std::declval<I&>())) &&
        noexcept(--std::declval<I&>()) &&
        std::is_nothrow_copy_constructible<I>::value)
    {
        return ranges::iter_move(prev(i.current_));
    }

    template <typename I2>
    friend constexpr auto
    iter_swap(const reverse_iterator& x,
              const reverse_iterator<I2>&
                  y) noexcept(noexcept(ranges::iter_swap(std::declval<I>(),
                                                         std::declval<I>())) &&
                              noexcept(--std::declval<I&>()))
        -> std::enable_if_t<indirectly_swappable<I2, I>>

    {
        ranges::iter_swap(prev(x.current_), prev(y.base()));
    }

private:
    I current_{};
};

template <typename I1, typename I2>
constexpr std::enable_if_t<equality_comparable_with<I1, I2>, bool>
operator==(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y)
{
    return x.base() == y.base();
}

template <typename I1, typename I2>
constexpr std::enable_if_t<equality_comparable_with<I1, I2>, bool>
operator!=(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y)
{
    return x.base() != y.base();
}

template <typename I1, typename I2>
constexpr std::enable_if_t<totally_ordered_with<I1, I2>, bool>
operator<(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y)
{
    return x.base() > y.base();
}

template <typename I1, typename I2>
constexpr std::enable_if_t<totally_ordered_with<I1, I2>, bool>
operator>(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y)
{
    return x.base() < y.base();
}

template <typename I1, typename I2>
constexpr std::enable_if_t<totally_ordered_with<I1, I2>, bool>
operator>=(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y)
{
    return x.base() <= y.base();
}

template <typename I1, typename I2>
constexpr std::enable_if_t<totally_ordered_with<I1, I2>, bool>
operator<=(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y)
{
    return x.base() >= y.base();
}

template <typename I1, typename I2>
constexpr std::enable_if_t<sized_sentinel_for<I1, I2>, iter_difference_t<I2>>
operator-(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y)
{
    return y.base() - x.base();
}

template <typename I>
constexpr std::enable_if_t<random_access_iterator<I>, reverse_iterator<I>>
operator+(iter_difference_t<I> n, const reverse_iterator<I>& x)
{
    return reverse_iterator<I>(x.base() - n);
}

} // namespace reverse_iterator_

using reverse_iterator_::reverse_iterator;

template <typename I>
constexpr std::enable_if_t<bidirectional_iterator<I>, reverse_iterator<I>>
make_reverse_iterator(I i)
{
    return reverse_iterator<I>(std::move(i));
}

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

namespace detail {
namespace rbegin_ {

template <typename T>
void rbegin(T&) = delete;

template <typename T>
void rbegin(const T&) = delete;

struct fn {
private:
    template <typename T,
              std::enable_if_t<
                  !std::is_lvalue_reference_v<T> &&
                  !enable_borrowed_range<std::remove_cv_t<T>>, int> = 0>
    static constexpr void impl(T&&, priority_tag<3>) = delete;

    template <typename T,
              typename I = decltype(decay_copy(std::declval<T>().rbegin()))>
    static constexpr auto
    impl(T&& t, priority_tag<2>) noexcept(noexcept(decay_copy(std::forward<T>(t).rbegin())))
        -> std::enable_if_t<input_or_output_iterator<I>, I>
    {
        return std::forward<T>(t).rbegin();
    }

    template <typename T,
              typename I = decltype(decay_copy(rbegin(std::declval<T>())))>
    static constexpr auto impl(T&& t, priority_tag<1>) noexcept(
        noexcept(decay_copy(rbegin(std::forward<T>(t)))))
        -> std::enable_if_t<input_or_output_iterator<I>, I>
    {
        return rbegin(std::forward<T>(t));
    }

    template <typename T,
              typename I = decltype(ranges::begin(std::declval<T>())),
              typename S = decltype(ranges::end(std::declval<T>()))>
    static constexpr auto impl(T&& t, priority_tag<0>) noexcept(
        noexcept(ranges::make_reverse_iterator(ranges::end(std::forward<T>(t)))))
        -> std::enable_if_t<same_as<I, S> && bidirectional_iterator<I>,
                            decltype(ranges::make_reverse_iterator(
                                ranges::end(std::forward<T>(t))))>
    {
        return ranges::make_reverse_iterator(ranges::end(std::forward<T>(t)));
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(fn::impl(std::forward<T>(t), priority_tag<3>{})))
            -> decltype(fn::impl(std::forward<T>(t), priority_tag<3>{}))
    {
        return fn::impl(std::forward<T>(t), priority_tag<3>{});
    }
};

} // namespace rbegin_
} // namespace detail

NANO_INLINE_VAR(detail::rbegin_::fn, rbegin)

namespace detail {
namespace rend_ {

template <typename T>
void rend(T&) = delete;

template <typename T>
void rend(const T&) = delete;

struct fn {
private:
    template <typename T,
              std::enable_if_t<
                  !std::is_lvalue_reference_v<T> &&
                  !enable_borrowed_range<std::remove_cv_t<T>>, int> = 0>
    static constexpr void impl(T&&, priority_tag<3>) = delete;

    template <typename T,
              typename I = decltype(ranges::rbegin(std::declval<T>())),
              typename S = decltype(decay_copy(std::declval<T>().rend()))>
    static constexpr auto
    impl(T&& t, priority_tag<2>) noexcept(noexcept(decay_copy(std::forward<T>(t).rend())))
        -> std::enable_if_t<sentinel_for<S, I>, S>
    {
        return std::forward<T>(t).rend();
    }

    template <typename T,
              typename I = decltype(ranges::rbegin(std::declval<T>())),
              typename S = decltype(decay_copy(rend(std::declval<T>())))>
    static constexpr auto impl(T&& t, priority_tag<1>) noexcept(
        noexcept(decay_copy(rend(std::forward<T>(t)))))
        -> std::enable_if_t<sentinel_for<S, I>, S>
    {
        return rend(std::forward<T>(t));
    }

    template <typename T,
              typename I = decltype(ranges::begin(std::declval<T>())),
              typename S = decltype(ranges::end(std::declval<T>()))>
    static constexpr auto impl(T&& t, priority_tag<0>) noexcept(
        noexcept(ranges::make_reverse_iterator(ranges::begin(std::forward<T>(t)))))
        -> std::enable_if_t<same_as<I, S> && bidirectional_iterator<I>,
                            decltype(ranges::make_reverse_iterator(
                                ranges::begin(std::forward<T>(t))))>
    {
        return ranges::make_reverse_iterator(ranges::begin(std::forward<T>(t)));
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(fn::impl(std::forward<T>(t), priority_tag<3>{})))
            -> decltype(fn::impl(std::forward<T>(t), priority_tag<3>{}))
    {
        return fn::impl(std::forward<T>(t), priority_tag<3>{});
    }
};

} // namespace rend_
} // namespace detail

NANO_INLINE_VAR(detail::rend_::fn, rend)

namespace detail {
namespace crbegin_ {

struct fn {
private:
    template <typename T, typename U = std::remove_reference_t<T>,
              std::enable_if_t<std::is_lvalue_reference_v<T>, int> = 0>
    static constexpr auto impl(T&& t)
        noexcept(noexcept(ranges::rbegin(static_cast<const U&>(t))))
        -> decltype(ranges::rbegin(static_cast<const U&>(t)))
    {
        return ranges::rbegin(static_cast<const U&>(t));
    }

    template <typename T,
              std::enable_if_t<!std::is_lvalue_reference_v<T>, int> = 0>
    static constexpr auto impl(T&& t)
        noexcept(noexcept(ranges::rbegin(static_cast<const T&&>(t))))
            -> decltype(ranges::rbegin(static_cast<const T&&>(t)))
    {
        return ranges::rbegin(static_cast<const T&&>(t));
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(fn::impl(std::forward<T>(t))))
        -> decltype(fn::impl(std::forward<T>(t)))
    {
        return fn::impl(std::forward<T>(t));
    }
};

} // namespace crbegin_
} // namespace detail

NANO_INLINE_VAR(detail::crbegin_::fn, crbegin)

namespace detail {
namespace crend_ {

struct fn {
private:
    template <typename T, typename U = std::remove_reference_t<T>,
              std::enable_if_t<std::is_lvalue_reference_v<T>, int> = 0>
    static constexpr auto impl(T&& t)
        noexcept(noexcept(ranges::rend(static_cast<const U&>(t))))
        -> decltype(ranges::rend(static_cast<const U&>(t)))
    {
        return ranges::rend(static_cast<const U&>(t));
    }

    template <typename T,
              std::enable_if_t<!std::is_lvalue_reference_v<T>, int> = 0>
    static constexpr auto impl(T&& t)
        noexcept(noexcept(ranges::rend(static_cast<const T&&>(t))))
            -> decltype(ranges::rend(static_cast<const T&&>(t)))
    {
        return ranges::rend(static_cast<const T&&>(t));
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(fn::impl(std::forward<T>(t))))
        -> decltype(fn::impl(std::forward<T>(t)))
    {
        return fn::impl(std::forward<T>(t));
    }
};

} // namespace crend_
} // namespace detail

NANO_INLINE_VAR(detail::crend_::fn, crend)

NANO_END_NAMESPACE

#endif




// nanorange/functional.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_FUNCTIONAL_HPP_INCLUDED
#define NANORANGE_FUNCTIONAL_HPP_INCLUDED





#endif


#endif

NANO_BEGIN_NAMESPACE

// [range.alg.adjacent.find]

namespace detail {

struct adjacent_find_fn {
private:
    friend struct unique_fn;

    template <typename I, typename S, typename Proj, typename Pred>
    static constexpr I impl(I first, S last, Pred& pred, Proj& proj)
    {
        if (first == last) {
            return first;
        }

        I next = first;
        ++next;

        while (next != last) {
            if (nano::invoke(pred, nano::invoke(proj, *first),
                             nano::invoke(proj, *next))) {
                return first;
            }
            ++first;
            ++next;
        }

        return next;
    }

public:
    template <typename I, typename S, typename Proj = identity,
              typename Pred = ranges::equal_to>
    constexpr std::enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                                   indirect_relation<Pred, projected<I, Proj>>,
                               I>
    operator()(I first, S last, Pred pred = Pred{}, Proj proj = Proj{}) const
    {
        return adjacent_find_fn::impl(std::move(first), std::move(last),
                                      pred, proj);
    }

    template <typename Rng, typename Proj = identity,
              typename Pred = ranges::equal_to>
    constexpr std::enable_if_t<
        forward_range<Rng> &&
            indirect_relation<Pred, projected<iterator_t<Rng>, Proj>>,
        borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Pred pred = Pred{}, Proj proj = Proj{}) const
    {
        return adjacent_find_fn::impl(nano::begin(rng), nano::end(rng),
                                      pred, proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::adjacent_find_fn, adjacent_find)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/all_of.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_ALL_OF_HPP_INCLUDED
#define NANORANGE_ALGORITHM_ALL_OF_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

// [range.alg.all_of]

namespace detail {

struct all_of_fn {
private:
    template <typename I, typename S, typename Proj, typename Pred>
    static constexpr bool impl(I first, S last, Pred& pred, Proj& proj)
    {
        while (first != last) {
            if (!nano::invoke(pred, nano::invoke(proj, *first))) {
                return false;
            }
            ++first;
        }
        return true;
    }

public:
    template <typename I, typename S, typename Proj = identity, typename Pred>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            indirect_unary_predicate<Pred, projected<I, Proj>>,
        bool>
    operator()(I first, S last, Pred pred, Proj proj = Proj{}) const
    {
        return all_of_fn::impl(std::move(first), std::move(last),
                               pred, proj);
    }

    template <typename Rng, typename Proj = identity, typename Pred>
    constexpr std::enable_if_t<
        input_range<Rng> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>>,
        bool>
    operator()(Rng&& rng, Pred pred, Proj proj = Proj{}) const
    {
        return all_of_fn::impl(nano::begin(rng), nano::end(rng),
                               pred, proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::all_of_fn, all_of)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/any_of.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_ANY_OF_HPP_INCLUDED
#define NANORANGE_ALGORITHM_ANY_OF_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

// [ranges.alg.any_of]

namespace detail {

struct any_of_fn {
private:
    friend struct is_permutation_fn;
    friend struct none_of_fn;

    template <typename I, typename S, typename Proj, typename Pred>
    static constexpr bool impl(I first, S last, Pred& pred, Proj& proj)
    {
        while (first != last) {
            if (nano::invoke(pred, nano::invoke(proj, *first)) == true) {
                return true;
            }
            ++first;
        }
        return false;
    }

public:
    template <typename I, typename S, typename Proj = identity, typename Pred>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            indirect_unary_predicate<Pred, projected<I, Proj>>,
        bool>
    operator()(I first, S last, Pred pred, Proj proj = Proj{}) const
    {
        return any_of_fn::impl(std::move(first), std::move(last),
                               pred, proj);
    }

    template <typename Rng, typename Proj = identity, typename Pred>
    constexpr std::enable_if_t<
        input_range<Rng> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>>,
        bool>
    operator()(Rng&& rng, Pred pred, Proj proj = Proj{}) const
    {
        return any_of_fn::impl(nano::begin(rng), nano::end(rng),
                               pred, proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::any_of_fn, any_of)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/binary_search.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_BINARY_SEARCH_HPP_INCLUDED
#define NANORANGE_ALGORITHM_BINARY_SEARCH_HPP_INCLUDED

// nanorange/algorithm/lower_bound.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_LOWER_BOUND_HPP_INCLUDED
#define NANORANGE_ALGORITHM_LOWER_BOUND_HPP_INCLUDED

// nanorange/algorithm/partition_point.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Uses code from CMCSTL2

//  Copyright Eric Niebler 2014
//  Copyright Casey Carter 2015


//===-------------------------- algorithm ---------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//

#ifndef NANORANGE_ALGORITHM_PARTITION_POINT_HPP_INCLUDED
#define NANORANGE_ALGORITHM_PARTITION_POINT_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct partition_point_fn {
private:
    friend struct lower_bound_fn;
    friend struct upper_bound_fn;

    template <typename I, typename Pred, typename Proj>
    static constexpr I impl_n(I first, iter_difference_t<I> n, Pred& pred,
                              Proj& proj)
    {
        while (n != 0) {
            const auto half = n/2;

            auto middle = nano::next(first, half);

            if (nano::invoke(pred, nano::invoke(proj, *middle))) {
                first = std::move(++middle);
                n -= half + 1;
            } else {
                n = half;
            }
        }

        return first;
    }

    template <typename I, typename S, typename Pred, typename Proj>
    static constexpr std::enable_if_t<sized_sentinel_for<S, I>, I>
    impl(I first, S last, Pred& pred, Proj& proj)
    {
        const auto n = nano::distance(first, std::move(last));
        return partition_point_fn::impl_n(std::move(first), n, pred, proj);
    }

    template <typename I, typename S, typename Pred, typename Proj>
    static constexpr std::enable_if_t<!sized_sentinel_for<S, I>, I>
    impl(I first, S last, Pred& pred, Proj& proj)
    {
        // Probe exponentially for either end-of-range or an iterator
        // that is past the partition point (i.e., does not satisfy pred).
        iter_difference_t<I> n{1};

        while (true) {
            auto m = first;
            auto d = nano::advance(m, n, last);
            if (m == last || !nano::invoke(pred, nano::invoke(proj, *m))) {
                n -= d;
                return partition_point_fn::impl_n(std::move(first), n,
                                                  pred, proj);
            }
            first = std::move(m);
            n *= 2;
        }
    }

public:
    template <typename I, typename S, typename Pred, typename Proj = identity>
    std::enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                         indirect_unary_predicate<Pred, projected<I, Proj>>, I>
    constexpr operator()(I first, S last, Pred pred, Proj proj = Proj{}) const
    {
        return partition_point_fn::impl(std::move(first), std::move(last),
                                        pred, proj);
    }

    template <typename Rng, typename Pred, typename Proj = identity>
    std::enable_if_t<
        forward_range<Rng> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>>,
        borrowed_iterator_t<Rng>>
    constexpr operator()(Rng&& rng, Pred pred, Proj proj = Proj{}) const
    {
        return partition_point_fn::impl(nano::begin(rng), nano::end(rng),
                                        pred, proj);
    }
};

}

NANO_INLINE_VAR(detail::partition_point_fn, partition_point)

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

namespace detail {

struct lower_bound_fn {
private:
    friend struct binary_search_fn;
    friend struct equal_range_fn;

    template <typename Comp, typename T>
    struct compare {
        Comp& comp;
        const T& val;

        template <typename U>
        constexpr bool operator()(U&& u) const
        {
            return nano::invoke(comp, std::forward<U>(u), val);
        }
    };


    template <typename I, typename S, typename T, typename Comp, typename Proj>
    static constexpr I impl(I first, S last, const T& value, Comp& comp, Proj& proj)
    {
        const auto comparator = compare<Comp, T>{comp, value};
        return partition_point_fn::impl(std::move(first), std::move(last),
                                        comparator, proj);
    }

public:
    template <typename I, typename S, typename T, typename Comp = ranges::less,
              typename Proj = identity>
    std::enable_if_t<
        forward_iterator<I> && sentinel_for<S, I> &&
            indirect_strict_weak_order<Comp, const T*, projected<I, Proj>>,
        I>
    constexpr operator()(I first, S last, const T& value, Comp comp = Comp{},
                         Proj proj = Proj{}) const
    {
        return lower_bound_fn::impl(std::move(first), std::move(last),
                                    value, comp, proj);
    }

    template <typename Rng, typename T, typename Comp = ranges::less,
              typename Proj = identity>
    std::enable_if_t<forward_range<Rng> &&
                         indirect_strict_weak_order<Comp, const T*, projected<iterator_t<Rng>, Proj>>,
                     borrowed_iterator_t<Rng>>
    constexpr operator()(Rng&& rng, const T& value, Comp comp = Comp{},
                         Proj proj = Proj{}) const
    {
        return lower_bound_fn::impl(nano::begin(rng), nano::end(rng),
                                    value, comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::lower_bound_fn, lower_bound)

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

namespace detail {

struct binary_search_fn {
private:
    template <typename I, typename S, typename T, typename Comp, typename Proj>
    static constexpr bool impl(I first, S last, const T& value, Comp& comp,
                               Proj& proj)
    {
        first = lower_bound_fn::impl(std::move(first), last, value, comp, proj);
        return (first != last && !nano::invoke(comp, value, nano::invoke(proj, *first)));
    }

public:
    template <typename I, typename S, typename T, typename Comp = ranges::less,
              typename Proj = identity>
    std::enable_if_t<
        forward_iterator<I> && sentinel_for<S, I> &&
            indirect_strict_weak_order<Comp, const T*, projected<I, Proj>>,
    bool>
    constexpr operator()(I first, S last, const T& value, Comp comp = Comp{},
               Proj proj = Proj{}) const
    {
        return binary_search_fn::impl(std::move(first), std::move(last),
                                      value, comp, proj);
    }

    template <typename Rng, typename T, typename Comp = ranges::less,
              typename Proj = identity>
    std::enable_if_t<forward_range<Rng> &&
                         indirect_strict_weak_order<Comp, const T*, projected<iterator_t<Rng>, Proj>>,
    bool>
    constexpr operator()(Rng&& rng, const T& value, Comp comp = Comp{},
                         Proj proj = Proj{}) const
    {
        return binary_search_fn::impl(nano::begin(rng), nano::end(rng),
                                      value, comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::binary_search_fn, binary_search)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/clamp.hpp
//
// Copyright (c) 2020 Boris Staletic (boris dot staletic at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_CLAMP_HPP_INCLUDED
#define NANORANGE_ALGORITHM_CLAMP_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct clamp_fn {
    template <typename T, typename Proj = identity, typename Comp = nano::less>
    constexpr std::enable_if_t<indirect_strict_weak_order<Comp, projected<const T*, Proj>>, const T&>
    operator()(const T& value, const T& low, const T& high, Comp comp = {}, Proj proj = Proj{}) const
    {
        auto&& projected_value = nano::invoke(proj, value);
        if (nano::invoke(comp, projected_value, nano::invoke(proj, low))) {
            return low;
        } else if (nano::invoke(comp, nano::invoke(proj, high), projected_value)) {
            return high;
        } else {
            return value;
        }
    }
};
} // namespace detail

NANO_INLINE_VAR(detail::clamp_fn, clamp)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/copy.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_COPY_HPP_INCLUDED
#define NANORANGE_ALGORITHM_COPY_HPP_INCLUDED

// nanorange/detail/algorithm/result_types.hpp
//
// Copyright (c) 2020 Boris Staletic (boris dot staletic at gmail dot com)
// Copyright (c) 2020 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_ALGORITHM_RETURN_TYPES
#define NANORANGE_DETAIL_ALGORITHM_RETURN_TYPES




#include <type_traits>

NANO_BEGIN_NAMESPACE

template <typename I, typename F>
struct in_fun_result {
    NANO_NO_UNIQUE_ADDRESS I in;
    NANO_NO_UNIQUE_ADDRESS F fun;

    template <typename I2, typename F2,
              std::enable_if_t<convertible_to<const I&, I2> &&
                               convertible_to<const F&, F2>, int> = 0>
    constexpr operator in_fun_result<I2, F2>() const &
    {
        return {in, fun};
    }

    template <typename I2, typename F2,
        std::enable_if_t<convertible_to<I, I2> &&
                         convertible_to<F, F2>, int> = 0>
    constexpr operator in_fun_result<I2, F2>() &&
    {
        return {std::move(in), std::move(fun)};
    }
};

template <typename I1, typename I2>
struct in_in_result {
    NANO_NO_UNIQUE_ADDRESS I1 in1;
    NANO_NO_UNIQUE_ADDRESS I2 in2;

    template <typename II1, typename II2,
        std::enable_if_t<convertible_to<const I1&, II1> &&
                         convertible_to<const I2&, II2>, int> = 0>
    constexpr operator in_in_result<II1, II2>() const &
    {
        return {in1, in2};
    }

    template <typename II1, typename II2,
        std::enable_if_t<convertible_to<I1, II1> &&
                         convertible_to<I2, II2>, int> = 0>
    constexpr operator in_in_result<II1, II2>() &&
    {
        return {std::move(in1), std::move(in2)};
    }
};

template <typename I, typename O>
struct in_out_result {
    NANO_NO_UNIQUE_ADDRESS I in;
    NANO_NO_UNIQUE_ADDRESS O out;

    template <typename I2, typename O2,
              std::enable_if_t<convertible_to<const I&, I2> &&
                               convertible_to<const O&, O2>, int> = 0>
    constexpr operator in_out_result<I2, O2>() const &
    {
        return {in, out};
    }

    template <typename I2, typename O2,
              std::enable_if_t<convertible_to<I, I2> &&
                               convertible_to<O, O2>, int> = 0>
    constexpr operator in_out_result<I2, O2>() &&
    {
        return {std::move(in), std::move(out)};
    }
};

template <typename I1, typename I2, typename O>
struct in_in_out_result {
    NANO_NO_UNIQUE_ADDRESS I1 in1;
    NANO_NO_UNIQUE_ADDRESS I2 in2;
    NANO_NO_UNIQUE_ADDRESS O out;

    template <typename II1, typename II2, typename O2,
              std::enable_if_t<convertible_to<const I1&, II1> &&
                               convertible_to<const I2&, II2> &&
                               convertible_to<const O&, O2>, int> = 0>
    constexpr operator in_in_out_result<II1, II2, O2>() const &
    {
        return {in1, in2, out};
    }

    template <typename II1, typename II2, typename O2,
              std::enable_if_t<convertible_to<I1, II1> &&
                               convertible_to<I2, II2> &&
                               convertible_to<O, O2>, int> = 0>
    constexpr operator in_in_out_result<II1, II2, O2>() &&
    {
        return {std::move(in1), std::move(in2), std::move(out)};
    }
};

template <typename I, typename O1, typename O2>
struct in_out_out_result {
    NANO_NO_UNIQUE_ADDRESS I in;
    NANO_NO_UNIQUE_ADDRESS O1 out1;
    NANO_NO_UNIQUE_ADDRESS O2 out2;

    template <typename II, typename OO1, typename OO2,
              std::enable_if_t<convertible_to<const I&, II> &&
                               convertible_to<const O1&, OO1> &&
                               convertible_to<const O2&, OO2>, int> = 0>
    constexpr operator in_out_out_result<II, OO1, OO2>() const &
    {
        return {in, out1, out2};
    }

    template <typename II, typename OO1, typename OO2,
        std::enable_if_t<convertible_to<I, II> &&
                         convertible_to<O1, OO1> &&
                         convertible_to<O2, OO2>, int> = 0>
    constexpr operator in_out_out_result<II, OO1, OO2>() &&
    {
        return {std::move(in), std::move(out1), std::move(out2)};
    }
};

template <typename T>
struct min_max_result {
    NANO_NO_UNIQUE_ADDRESS T min;
    NANO_NO_UNIQUE_ADDRESS T max;

    template <typename T2,
              std::enable_if_t<convertible_to<const T&, T2>, int> = 0>
    constexpr operator min_max_result<T2>() const &
    {
        return {min, max};
    }

    template <typename T2,
              std::enable_if_t<convertible_to<T, T2>, int> = 0>
    constexpr operator min_max_result<T2>() &&
    {
        return {std::move(min), std::move(max)};
    }
};

template <typename I>
struct in_found_result {
    NANO_NO_UNIQUE_ADDRESS I in;
    bool found;
    template<class I2,
             std::enable_if_t< convertible_to<const I&, I2>, int> = 0>
    constexpr operator in_found_result<I2>() const & {
      return {in, found};
    }
    template<class I2,
             std::enable_if_t< convertible_to<const I&, I2>, int> = 0>
    constexpr operator in_found_result<I2>() && {
      return {std::move(in), found};
    }
};

NANO_END_NAMESPACE

#endif



NANO_BEGIN_NAMESPACE

template <typename I, typename O>
using copy_result = in_out_result<I, O>;

namespace detail {

struct copy_fn {
private:
    // If we know the distance between first and last, we can use that
    // information to (potentially) allow better codegen
    template <typename I, typename S, typename O>
    static constexpr std::enable_if_t<sized_sentinel_for<S, I>, copy_result<I, O>>
    impl(I first, S last, O result, priority_tag<1>)
    {
        const auto dist = last - first;

        for (iter_difference_t<I> i = 0; i < dist; ++i) {
            *result = *first;
            ++first;
            ++result;
        }

        return {std::move(first), std::move(result)};
    }

    template <typename I, typename S, typename O>
    static constexpr copy_result<I, O> impl(I first, S last, O result,
                                            priority_tag<0>)
    {
        while (first != last) {
            *result = *first;
            ++first;
            ++result;
        }

        return {std::move(first), std::move(result)};
    }

public:
    template <typename I, typename S, typename O>
    constexpr std::enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                                   weakly_incrementable<O> &&
                                   indirectly_copyable<I, O>,
                               copy_result<I, O>>
    operator()(I first, S last, O result) const
    {
        return copy_fn::impl(std::move(first), std::move(last),
                             std::move(result), priority_tag<1>{});
    }

    template <typename Rng, typename O>
    constexpr std::enable_if_t<input_range<Rng> && weakly_incrementable<O> &&
                                   indirectly_copyable<iterator_t<Rng>, O>,
                               copy_result<borrowed_iterator_t<Rng>, O>>
    operator()(Rng&& rng, O result) const
    {
        return copy_fn::impl(nano::begin(rng), nano::end(rng),
                             std::move(result), priority_tag<1>{});
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::copy_fn, copy)

template <typename I, typename O>
using copy_n_result = in_out_result<I, O>;

namespace detail {

struct copy_n_fn {
    template <typename I, typename O>
    constexpr std::enable_if_t<input_iterator<I> && weakly_incrementable<O> &&
                                   indirectly_copyable<I, O>,
                               copy_n_result<I, O>>
    operator()(I first, iter_difference_t<I> n, O result) const
    {
        for (iter_difference_t<I> i{}; i < n; i++) {
            *result = *first;
            ++first;
            ++result;
        }

        return {std::move(first), std::move(result)};
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::copy_n_fn, copy_n)

template <typename I, typename O>
using copy_if_result = in_out_result<I, O>;

namespace detail {

struct copy_if_fn {
private:
    template <typename I, typename S, typename O, typename Pred, typename Proj>
    static constexpr copy_if_result<I, O> impl(I first, S last, O result,
                                               Pred pred, Proj proj)
    {
        while (first != last) {
            if (nano::invoke(pred, nano::invoke(proj, *first))) {
                *result = *first;
                ++result;
            }
            ++first;
        }

        return {std::move(first), std::move(result)};
    }

public:
    template <typename I, typename S, typename O, typename Proj = identity,
              typename Pred>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> && weakly_incrementable<O> &&
            indirect_unary_predicate<Pred, projected<I, Proj>> &&
            indirectly_copyable<I, O>,
        copy_if_result<I, O>>
    operator()(I first, S last, O result, Pred pred, Proj proj = Proj{}) const
    {
        return copy_if_fn::impl(std::move(first), std::move(last),
                                std::move(result), std::move(pred),
                                std::move(proj));
    }

    template <typename Rng, typename O, typename Proj = identity, typename Pred>
    constexpr std::enable_if_t<
        input_range<Rng> && weakly_incrementable<O> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>>,
        copy_if_result<borrowed_iterator_t<Rng>, O>>
    operator()(Rng&& rng, O result, Pred pred, Proj proj = Proj{}) const
    {
        return copy_if_fn::impl(nano::begin(rng), nano::end(rng),
                                std::move(result), std::move(pred),
                                std::move(proj));
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::copy_if_fn, copy_if)

template <typename I, typename O>
using copy_backward_result = in_out_result<I, O>;

namespace detail {

struct copy_backward_fn {
private:
    template <typename I1, typename S1, typename I2>
    static constexpr copy_backward_result<I1, I2>
    impl(I1 first, S1 last, I2 result)
    {
        I1 last_it = nano::next(first, std::move(last));
        I1 it = last_it;

        while (it != first) {
            *--result = *--it;
        }

        return {std::move(last_it), std::move(result)};
    }

public:
    template <typename I1, typename S1, typename I2>
    constexpr std::enable_if_t<
        bidirectional_iterator<I1> && sentinel_for<S1, I1> &&
            bidirectional_iterator<I2> && indirectly_copyable<I1, I2>,
                               copy_backward_result<I1, I2>>
    operator()(I1 first, S1 last, I2 result) const
    {
        return copy_backward_fn::impl(std::move(first), std::move(last),
                                      std::move(result));
    }

    template <typename Rng, typename I>
    constexpr std::enable_if_t<bidirectional_range<Rng> &&
                                   bidirectional_iterator<I> &&
                                   indirectly_copyable<iterator_t<Rng>, I>,
                               copy_backward_result<borrowed_iterator_t<Rng>, I>>
    operator()(Rng&& rng, I result) const
    {
        return copy_backward_fn::impl(nano::begin(rng), nano::end(rng),
                                      std::move(result));
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::copy_backward_fn, copy_backward)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/count.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_COUNT_HPP_INCLUDED
#define NANORANGE_ALGORITHM_COUNT_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

// [rng.alg.count]

namespace detail {

struct count_if_fn {
private:
    friend struct count_fn;
    friend struct is_permutation_fn;

    template <typename I, typename S, typename Proj, typename Pred>
    static constexpr iter_difference_t<I> impl(I first, S last, Pred& pred,
                                               Proj& proj)
    {
        iter_difference_t<I> counter = 0;

        for (; first != last; ++first) {
            if (nano::invoke(pred, nano::invoke(proj, *first))) {
                ++counter;
            }
        }

        return counter;
    }

public:
    template <typename I, typename S, typename Proj = identity, typename Pred>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            indirect_unary_predicate<Pred, projected<I, Proj>>,
        iter_difference_t<I>>
    operator()(I first, S last, Pred pred, Proj proj = Proj{}) const
    {
        return count_if_fn::impl(std::move(first), std::move(last),
                                 pred, proj);
    }

    template <typename Rng, typename Proj = identity, typename Pred>
    constexpr std::enable_if_t<
        input_range<Rng> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>>,
        range_difference_t<Rng>>
    operator()(Rng&& rng, Pred pred, Proj proj = Proj{}) const
    {
        return count_if_fn::impl(nano::begin(rng), nano::end(rng),
                                 pred, proj);
    }
};
} // namespace detail

NANO_INLINE_VAR(detail::count_if_fn, count_if)

namespace detail {

struct count_fn {
    template <typename I, typename S, typename T, typename Proj = identity>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            indirect_relation<ranges::equal_to, projected<I, Proj>, const T*>,
        iter_difference_t<I>>
    operator()(I first, S last, const T& value, Proj proj = Proj{}) const
    {
        const auto pred = [&value] (const auto& t) { return t == value; };
        return count_if_fn::impl(std::move(first), std::move(last),
                                 pred, proj);
    }

    template <typename Rng, typename T, typename Proj = identity>
    constexpr std::enable_if_t<
        input_range<Rng> &&
            indirect_relation<ranges::equal_to, projected<iterator_t<Rng>, Proj>,
                             const T*>,
        range_difference_t<Rng>>
    operator()(Rng&& rng, const T& value, Proj proj = Proj{}) const
    {
        const auto pred = [&value] (const auto& t) { return t == value; };
        return count_if_fn::impl(nano::begin(rng), nano::end(rng),
                                 pred, proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::count_fn, count)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/equal.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_EQUAL_HPP_INCLUDED
#define NANORANGE_ALGORITHM_EQUAL_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

namespace detail {

struct equal_fn {
private:
    template <typename I1, typename S1, typename I2, typename S2, typename Pred,
              typename Proj1, typename Proj2>
    static constexpr bool impl4(I1 first1, S1 last1, I2 first2, S2 last2,
                                Pred& pred, Proj1& proj1, Proj2& proj2)
    {
        while (first1 != last1 && first2 != last2) {
            if (!nano::invoke(pred, nano::invoke(proj1, *first1),
                              nano::invoke(proj2, *first2))) {
                return false;
            }
            ++first1;
            ++first2;
        }

        return first1 == last1 && first2 == last2;
    }

    template <typename I1, typename S1, typename I2, typename Pred,
              typename Proj1, typename Proj2>
    static constexpr bool impl3(I1 first1, S1 last1, I2 first2, Pred pred,
                                Proj1& proj1, Proj2& proj2)
    {
        while (first1 != last1) {
            if (!nano::invoke(pred, nano::invoke(proj1, *first1),
                              nano::invoke(proj2, *first2))) {
                return false;
            }
            ++first1;
            ++first2;
        }

        return true;
    }

public:
    // Four-legged, sized sentinels
    template <typename I1, typename S1, typename I2, typename S2,
              typename Pred = ranges::equal_to, typename Proj1 = identity,
              typename Proj2 = identity>
    constexpr std::enable_if_t<
        input_iterator<I1> && sentinel_for<S1, I1> && input_iterator<I2> &&
            sentinel_for<S2, I2> &&
            indirectly_comparable<I1, I2, Pred, Proj1, Proj2> &&
            sized_sentinel_for<S1, I1> && sized_sentinel_for<S2, I2>,
        bool>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        if (last1 - first1 != last2 - first2) {
            return false;
        }

        // Ranges are the same size, so call the 3-legged version
        // and save ourselves a comparison
        return equal_fn::impl3(std::move(first1), std::move(last1),
                               std::move(first2), pred,
                               proj1, proj2);
    }

    // Four-legged, unsized sentinels
    template <typename I1, typename S1, typename I2, typename S2,
              typename Pred = ranges::equal_to, typename Proj1 = identity,
              typename Proj2 = identity>
    constexpr std::enable_if_t<
        input_iterator<I1> && sentinel_for<S1, I1> && input_iterator<I2> &&
            sentinel_for<S2, I2> &&
            indirectly_comparable<I1, I2, Pred, Proj1, Proj2> &&
            !(sized_sentinel_for<S1, I1> && sized_sentinel_for<S2, I2>),
        bool>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return equal_fn::impl4(std::move(first1), std::move(last1),
                               std::move(first2), std::move(last2),
                               pred, proj1, proj2);
    }

    // Three legged
    template <typename I1, typename S1, typename I2, typename Pred = ranges::equal_to,
              typename Proj1 = identity, typename Proj2 = identity>
    NANO_DEPRECATED constexpr std::enable_if_t<
        input_iterator<I1> && sentinel_for<S1, I1> &&
            input_iterator<std::decay_t<I2>> &&
                !input_range<I2> &&
            indirectly_comparable<I1, std::decay_t<I2>, Pred, Proj1, Proj2>,
        bool>
    operator()(I1 first1, S1 last1, I2 first2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return equal_fn::impl3(std::move(first1), std::move(last1),
                               std::forward<I2>(first2), pred, proj1, proj2);
    }

    // Two ranges, both sized
    template <typename Rng1, typename Rng2, typename Pred = ranges::equal_to,
              typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<
        input_range<Rng1> && input_range<Rng2> &&
            indirectly_comparable<iterator_t<Rng1>, iterator_t<Rng2>, Pred,
                                 Proj1, Proj2> &&
            sized_range<Rng1> && sized_range<Rng2>,
        bool>
    operator()(Rng1&& rng1, Rng2&& rng2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        if (nano::distance(rng1) != nano::distance(rng2)) {
            return false;
        }

        return equal_fn::impl3(nano::begin(rng1), nano::end(rng1),
                               nano::begin(rng2), pred, proj1, proj2);
    }

    // Two ranges, not both sized
    template <typename Rng1, typename Rng2, typename Pred = ranges::equal_to,
              typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<
        input_range<Rng1> && input_range<Rng2> &&
            indirectly_comparable<iterator_t<Rng1>, iterator_t<Rng2>, Pred,
                                 Proj1, Proj2> &&
            !(sized_range<Rng1> && sized_range<Rng2>),
        bool>
    operator()(Rng1&& rng1, Rng2&& rng2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return equal_fn::impl4(nano::begin(rng1), nano::end(rng1),
                               nano::begin(rng2), nano::end(rng2),
                               pred, proj1, proj2);
    }

    // Range and a half
    template <typename Rng1, typename I2, typename Pred = ranges::equal_to,
              typename Proj1 = identity, typename Proj2 = identity>
    NANO_DEPRECATED constexpr std::enable_if_t<
        input_range<Rng1> && input_iterator<std::decay_t<I2>> &&
                !input_range<I2> &&
            indirectly_comparable<iterator_t<Rng1>, std::decay_t<I2>, Pred, Proj1, Proj2>,
        bool>
    operator()(Rng1&& rng1, I2&& first2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return equal_fn::impl3(nano::begin(rng1), nano::end(rng1),
                               std::forward<I2>(first2), pred, proj1, proj2);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::equal_fn, equal)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/equal_range.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_EQUAL_RANGE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_EQUAL_RANGE_HPP_INCLUDED


// nanorange/algorithm/upper_bound.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_UPPER_BOUND_HPP_INCLUDED
#define NANORANGE_ALGORITHM_UPPER_BOUND_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct upper_bound_fn {
private:
    friend struct equal_range_fn;

    template <typename Comp, typename T>
    struct compare {
        Comp& comp;
        const T& val;

        template <typename U>
        constexpr bool operator()(U&& u) const
        {
            return !nano::invoke(comp, val, std::forward<U>(u));
        }
    };


    template <typename I, typename S, typename T, typename Comp, typename Proj>
    static constexpr I impl(I first, S last, const T& value, Comp& comp, Proj& proj)
    {
        const auto comparator = compare<Comp, T>{comp, value};
        return partition_point_fn::impl(std::move(first), std::move(last),
                                        comparator, proj);
    }

public:
    template <typename I, typename S, typename T, typename Comp = ranges::less,
              typename Proj = identity>
    std::enable_if_t<
        forward_iterator<I> && sentinel_for<S, I> &&
            indirect_strict_weak_order<Comp, const T*, projected<I, Proj>>,
    I>
    constexpr operator()(I first, S last, const T& value, Comp comp = Comp{},
                         Proj proj = Proj{}) const
    {
        return upper_bound_fn::impl(std::move(first), std::move(last),
                                    value, comp, proj);
    }

    template <typename Rng, typename T, typename Comp = ranges::less,
              typename Proj = identity>
    std::enable_if_t<forward_range<Rng> &&
                         indirect_strict_weak_order<Comp, const T*, projected<iterator_t<Rng>, Proj>>,
                     borrowed_iterator_t<Rng>>
    constexpr operator()(Rng&& rng, const T& value, Comp comp = Comp{},
                         Proj proj = Proj{}) const
    {
        return upper_bound_fn::impl(nano::begin(rng), nano::end(rng),
                                    value, comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::upper_bound_fn, upper_bound)

NANO_END_NAMESPACE

#endif

// nanorange/views/subrange.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_SUBRANGE_HPP_INCLUDED
#define NANORANGE_VIEWS_SUBRANGE_HPP_INCLUDED


// nanorange/views/interface.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_INTERFACE_HPP_INCLUDED
#define NANORANGE_VIEWS_INTERFACE_HPP_INCLUDED


// nanorange/iterator/common_iterator.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_COMMON_ITERATOR_HPP_INCLUDED
#define NANORANGE_ITERATOR_COMMON_ITERATOR_HPP_INCLUDED

// nanorange/iterator/concepts.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_CONCEPTS_HPP_INCLUDED
#define NANORANGE_ITERATOR_CONCEPTS_HPP_INCLUDED





#endif


NANO_BEGIN_NAMESPACE

// [range.iterators.common]

namespace common_iterator_ {

template <typename I, typename S>
class common_iterator {
    static_assert(input_or_output_iterator<I>, "");
    static_assert(sentinel_for<S, I>, "");
    static_assert(!same_as<I, S>, "");

    template <typename II, typename SS>
    friend class common_iterator;

    class op_arrow_proxy {
        iter_value_t<I> keep_;

        constexpr op_arrow_proxy(iter_reference_t<I>&& x) : keep_(std::move(x)) {}

    public:
        constexpr const iter_value_t<I>* operator->() const
        {
            return std::addressof(keep_);
        }
    };

    template <typename II>
    using op_arrow_t = decltype(std::declval<const II&>().operator->());

    template <typename II>
    static constexpr auto do_op_arrow(const II& i, detail::priority_tag<2>)
        -> std::enable_if_t<
            std::is_pointer<II>::value || detail::exists_v<op_arrow_t, II>, I>
    {
        return i;
    }

    template <typename II>
    static constexpr auto do_op_arrow(const II& i, detail::priority_tag<1>)
        -> std::enable_if_t<std::is_reference<iter_reference_t<const II>>::value,
                            std::add_pointer_t<iter_reference_t<const II>>>
    {
        auto&& tmp = *i;
        return std::addressof(tmp);
    }

    template <typename II>
    static constexpr auto do_op_arrow(const II& i, detail::priority_tag<0>)
        -> op_arrow_proxy
    {
        return {i};
    }

public:
    using difference_type = iter_difference_t<I>;

    constexpr common_iterator() : is_sentinel_{false}, iter_{} {}

    constexpr common_iterator(I i) : is_sentinel_{false}, iter_(i) {}

    constexpr common_iterator(S s) : is_sentinel_{true}, sentinel_{s} {}

    template <
        typename II, typename SS,
        std::enable_if_t<convertible_to<II, I> && convertible_to<SS, S>, int> = 0>
    constexpr common_iterator(const common_iterator<II, SS>& other)
        : is_sentinel_{other.is_sentinel_},
          iter_(other.iter_),
          sentinel_(other.sentinel_)
    {}

    template <typename II, typename SS>
    constexpr std::enable_if_t<convertible_to<II, I> && convertible_to<SS, S>,
                               common_iterator&>
    operator=(const common_iterator<II, SS>& other)
    {
        is_sentinel_ = other.is_sentinel_;
        iter_ = other.iter_;
        sentinel_ = other.sentinel_;
        return *this;
    }

    constexpr decltype(auto) operator*() { return *iter_; }

    template <typename II = I,
              std::enable_if_t<detail::dereferenceable<const I>, int> = 0>
    constexpr decltype(auto) operator*() const
    {
        return *iter_;
    }

    template <typename II = I>
    constexpr auto operator-> () const
        -> decltype(common_iterator::do_op_arrow(std::declval<const II&>(),
                                                 detail::priority_tag<2>{}))
    {
        return do_op_arrow(iter_, detail::priority_tag<2>{});
    }

    constexpr common_iterator& operator++()
    {
        ++iter_;
        return *this;
    }

    template <typename II = I, std::enable_if_t<!forward_iterator<II>, int> = 0>
    constexpr  decltype(auto) operator++(int)
    {
        return iter_++;
    }

    template <typename II = I, std::enable_if_t<forward_iterator<II>, int> = 0>
    constexpr common_iterator operator++(int)
    {
        common_iterator tmp = *this;
        ++iter_;
        return tmp;
    }

    template <typename I2, typename S2>
    friend constexpr auto operator==(const common_iterator& x,
                                     const common_iterator<I2, S2>& y)
        -> std::enable_if_t<sentinel_for<S2, I> && sentinel_for<S, I2> &&
                            !equality_comparable_with<I, I2>, bool>
    {
        return x.is_sentinel_ ? (y.is_sentinel_ || y.iter_ == x.sentinel_)
                              : (!y.is_sentinel_ || x.iter_ == y.sentinel_);
    }

    template <typename I2, typename S2>
    friend constexpr auto operator==(const common_iterator& x,
                                     const common_iterator<I2, S2>& y)
        -> std::enable_if_t<sentinel_for<S2, I> && sentinel_for<S, I2> &&
                            equality_comparable_with<I, I2>, bool>
    {
        return x.is_sentinel_
               ? (y.is_sentinel_ || y.iter_ == x.sentinel_)
               : (y.is_sentinel_ ? x.iter_ == y.sentinel_ : x.iter_ == y.iter_);
    }

    template <typename I2, typename S2>
    friend constexpr auto operator!=(const common_iterator& x,
                                     const common_iterator<I2, S2>& y)
        -> std::enable_if_t<sentinel_for<S2, I> && sentinel_for<S, I2>, bool>
    {
        return !(x == y);
    }

    template <typename I2, typename S2>
    friend constexpr auto operator-(const common_iterator& x,
                                    const common_iterator<I2, S2>& y)
        -> std::enable_if_t<sized_sentinel_for<I, I2> && sized_sentinel_for<S, I2> &&
                            sized_sentinel_for<S, I2>, iter_difference_t<I2>>
    {
        return x.is_sentinel_
               ? (y.is_sentinel_ ? 0 : x.sentinel_ - y.iter_)
               : (y.is_sentinel_ ? x.iter_ - y.sentinel_ : x.iter_ - y.iter_);
    }

    friend constexpr iter_rvalue_reference_t<I> iter_move(const common_iterator& i)
    {
        return ranges::iter_move(i.iter_);
    }

    template <typename I2, typename S2>
    friend constexpr std::enable_if_t<indirectly_swappable<I2, I>>
    iter_swap(const common_iterator& x, const common_iterator<I2, S2>& y)
    {
        return ranges::iter_swap(x.iter_, y.iter_);
    }

    // private:
    // TODO: Some sort of variant-like union
    bool is_sentinel_{};
    I iter_{};
    S sentinel_{};
};

} // namespace common_iterator_

using common_iterator_::common_iterator;

template <typename I, typename S>
struct readable_traits<common_iterator<I, S>> {
    using value_type = iter_value_t<I>;
};

template <typename I, typename S>
struct iterator_category<common_iterator<I, S>>
    : std::conditional<forward_iterator<I>, forward_iterator_tag,
                       input_iterator_tag> {
};

NANO_END_NAMESPACE

namespace std {

template <typename I, typename S>
struct iterator_traits<::nano::common_iterator<I, S>> {
    using difference_type =
        ::nano::iter_difference_t<::nano::common_iterator<I, S>>;
    using value_type = ::nano::iter_value_t<::nano::common_iterator<I, S>>;
    using pointer =
        std::add_pointer_t<::nano::iter_reference_t<::nano::common_iterator<I, S>>>;
    using reference = ::nano::iter_reference_t<::nano::common_iterator<I, S>>;
    using iterator_category =
        ::nano::detail::conditional_t<::nano::forward_iterator<I>,
                           std::forward_iterator_tag,
                           std::input_iterator_tag>;
};

} // namespace std

#endif



NANO_BEGIN_NAMESPACE

// [ranges.view_interface]

namespace detail {

template <typename, typename = void>
struct range_common_iterator_impl;

template <typename R>
struct range_common_iterator_impl<
    R, std::enable_if_t<range<R> && !common_range<R>>> {
    using type = common_iterator<iterator_t<R>, sentinel_t<R>>;
};

template <typename R>
struct range_common_iterator_impl<R, std::enable_if_t<common_range<R>>> {
    using type = iterator_t<R>;
};

template <typename R>
using range_common_iterator_t = typename range_common_iterator_impl<R>::type;

} // namespace detail

template <typename D>
class view_interface : public view_base {

    static_assert(std::is_class<D>::value, "");
    static_assert(same_as<D, std::remove_cv_t<D>>, "");

private:
    constexpr D& derived() noexcept { return static_cast<D&>(*this); }

    constexpr const D& derived() const noexcept
    {
        return static_cast<const D&>(*this);
    }

public:
    template <typename R = D>
    [[nodiscard]] constexpr auto empty()
        -> std::enable_if_t<forward_range<R>, bool>
    {
        return ranges::begin(derived()) == ranges::end(derived());
    }

    template <typename R = D>
    [[nodiscard]] constexpr auto empty() const
        -> std::enable_if_t<forward_range<const R>, bool>
    {
        return ranges::begin(derived()) == ranges::end(derived());
    }

    template <typename R = D, typename = decltype(ranges::empty(std::declval<R&>()))>
    constexpr explicit operator bool()
    {
        return !ranges::empty(derived());
    }

    template <typename R = D, typename = decltype(ranges::empty(std::declval<const R&>()))>
    constexpr explicit operator bool() const
    {
        return !ranges::empty(derived());
    }

    template <typename R = D, typename = std::enable_if_t<contiguous_iterator<iterator_t<R>>>>
    constexpr auto data()
    {
        return ranges::empty(derived()) ? nullptr : std::addressof(*ranges::begin(derived()));
    }

    template <typename R = D, typename = std::enable_if_t<
                  range<const R> && contiguous_iterator<iterator_t<const R>>>>
    constexpr auto data() const
    {
        return ranges::empty(derived()) ? nullptr : std::addressof(*ranges::begin(derived()));
    }

    template <typename R = D, typename = std::enable_if_t<
                  forward_range<R> &&
                  sized_sentinel_for<sentinel_t<R>, iterator_t<R>>>>
    constexpr auto size()
    {
        return ranges::end(derived()) - ranges::begin(derived());
    }

    template <typename R = D, typename = std::enable_if_t<
                  forward_range<const R> &&
                  sized_sentinel_for<sentinel_t<const R>, iterator_t<const R>>>>
    constexpr auto size() const
    {
        return ranges::end(derived()) - ranges::begin(derived());
    }

    template <typename R = D, typename = std::enable_if_t<forward_range<R>>>
    constexpr decltype(auto) front()
    {
        return *ranges::begin(derived());
    }

    template <typename R = D, typename = std::enable_if_t<forward_range<const R>>>
    constexpr decltype(auto) front() const
    {
        return *ranges::begin(derived());
    }

    template <typename R = D, typename = std::enable_if_t<
                                  bidirectional_range<R> && common_range<R>>>
    constexpr decltype(auto) back()
    {
        return *ranges::prev(ranges::end(derived()));
    }

    template <typename R = D, typename = std::enable_if_t<bidirectional_range<const R> &&
                                          common_range<const R>>>
    constexpr decltype(auto) back() const
    {
        return *ranges::prev(ranges::end(derived()));
    }

    template <typename R = D, typename = std::enable_if_t<random_access_range<R>>>
    constexpr decltype(auto) operator[](iter_difference_t<iterator_t<R>> n)
    {
        return ranges::begin(derived())[n];
    }

    template <typename R = D,  typename = std::enable_if_t<random_access_range<const R>>>
    constexpr decltype(auto) operator[](iter_difference_t<iterator_t<const R>> n) const
    {
        return ranges::begin(derived())[n];
    }
};

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

// [ranges.subrange]

enum class subrange_kind : bool { unsized, sized };

namespace detail {

template <typename I, typename S, bool = sized_sentinel_for<S, I>>
struct default_subrange_kind {
    static constexpr subrange_kind kind = subrange_kind::unsized;
};

template <typename I, typename S>
struct default_subrange_kind<I, S, true> {
    static constexpr subrange_kind kind = subrange_kind::sized;
};

}

namespace subrange_ {

template <typename I, typename S = I,
          subrange_kind = detail::default_subrange_kind<I, S>::kind>
class subrange;

}

using subrange_::subrange;

namespace detail {

struct convertible_to_non_slicing_concept {
    template <typename, typename>
    static auto test(long) -> std::false_type;

    template <typename From, typename To>
    static auto test(int) -> std::enable_if_t<
        convertible_to<From, To> &&
        !(std::is_pointer_v<std::decay_t<From>> &&
          std::is_pointer_v<std::decay_t<To>> &&
          not_same_as<std::remove_pointer_t<std::decay_t<From>>,
                      std::remove_pointer_t<std::decay_t<To>>>),
        std::true_type>;
};

template <typename From, typename To>
NANO_CONCEPT convertible_to_non_slicing =
    decltype(convertible_to_non_slicing_concept::test<From, To>(0))::value;

struct pair_like_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename T,
              typename = typename std::tuple_size<T>::type,
              typename = std::enable_if_t<detail::requires_<pair_like_concept, T>>>
    static auto test(int) -> std::true_type;

    template <typename T>
    auto requires_(T t) -> decltype(
        requires_expr<derived_from<std::tuple_size<T>, std::integral_constant<std::size_t, 2>>>{},
        std::declval<std::tuple_element_t<0, std::remove_const_t<T>>>(),
        std::declval<std::tuple_element_t<1, std::remove_const_t<T>>>(),
        requires_expr<convertible_to<decltype(std::get<0>(t)), const std::tuple_element<0, T>&>>{},
        requires_expr<convertible_to<decltype(std::get<1>(t)), const std::tuple_element<1, T>&>>{}
    );
};

template <typename T>
NANO_CONCEPT pair_like = !std::is_reference_v<T> &&
    decltype(pair_like_concept::test<T>(0))::value;

struct pair_like_convertible_from_concept {
    template <typename, typename, typename>
    static auto test(long) -> std::false_type;

    template <typename T, typename U, typename V,
              std::enable_if_t<!range<T>, int> = 0,
              std::enable_if_t<pair_like<T>, int> = 0,
              std::enable_if_t<constructible_from<T, U, V>, int> = 0,
              std::enable_if_t<convertible_to_non_slicing<U, std::tuple_element<0, T>>, int> = 0,
              std::enable_if_t<convertible_to<V, std::tuple_element<1, T>>, int> = 0>
    static auto test(int) -> std::true_type;
};

template <typename T, typename U, typename V>
NANO_CONCEPT pair_like_convertible_from =
    decltype(pair_like_convertible_from_concept::test<T, U, V>(0))::value;


struct iterator_sentinel_pair_concept {
    template <typename T>
    static auto test(long) -> std::false_type;

    template <typename T>
    static auto test(int) -> std::enable_if_t<
        !range<T> && pair_like<T> &&
        sentinel_for<std::tuple_element_t<1, T>,  std::tuple_element_t<0, T>>,
        std::true_type>;
};

template <typename T>
NANO_CONCEPT iterator_sentinel_pair =
    decltype(iterator_sentinel_pair_concept::test<T>(0))::value;

template <typename I, typename S, bool StoreSize = false>
struct subrange_data {
    constexpr subrange_data() = default;

    constexpr subrange_data(I&& begin, S&& end)
        : begin_(std::move(begin)), end_(std::move(end))
    {}

    constexpr subrange_data(I&& begin, S&& end, iter_difference_t<I> /*unused*/)
        : begin_(std::move(begin)), end_(std::move(end))
    {}

    I begin_{};
    S end_{};
};

template <typename I, typename S>
struct subrange_data<I, S, true> {
    constexpr subrange_data() = default;

    constexpr subrange_data(I&& begin, S&& end, iter_difference_t<I> size)
        : begin_(std::move(begin)), end_(std::move(end)), size_(size)
    {}

    I begin_{};
    S end_{};
    iter_difference_t<I> size_ = 0;
};

// MSVC gets confused if enable_if conditions in template param lists are too
// complex, so give it some help by calculating the constraints in a helper
// variable
template <typename R, typename I, typename S, subrange_kind K>
auto subrange_range_constructor_constraint_helper_fn(long) -> std::false_type;

template <typename R, typename I, typename S, subrange_kind K>
auto subrange_range_constructor_constraint_helper_fn(int) -> std::enable_if_t<borrowed_range<R>&&
                convertible_to_non_slicing<iterator_t<R>, I> &&
                convertible_to<sentinel_t<R>, S>, std::true_type>;

template <typename R, typename I, typename S, subrange_kind K>
constexpr bool subrange_range_constructor_constraint_helper =
    decltype(subrange_range_constructor_constraint_helper_fn<R, I, S, K>(0))::value;

template <typename R>
constexpr subrange_kind subrange_deduction_guide_helper()
{
    return (sized_range<R> || sized_sentinel_for<sentinel_t<R>, iterator_t<R>>)
           ? subrange_kind::sized : subrange_kind::unsized;
}

} // namespace detail

namespace subrange_ {

template <typename I, typename S, subrange_kind K>
class subrange : public view_interface<subrange<I, S, K>> {
    static_assert(input_or_output_iterator<I>);
    static_assert(sentinel_for<S, I>);
    static_assert(K == subrange_kind::sized || !sized_sentinel_for<S, I>, "");

private:
    static constexpr bool StoreSize =
            K == subrange_kind::sized && !sized_sentinel_for<S, I>;

    detail::subrange_data<I, S, StoreSize> data_{};

public:
    subrange() = default;

    template <typename II, bool SS = StoreSize,
             typename = std::enable_if_t<
                 detail::convertible_to_non_slicing<II, I> &&
                 !SS>>
    constexpr subrange(II i, S s)
            : data_{std::move(i), std::move(s)} {}

    template <typename II, subrange_kind KK = K,
              typename = std::enable_if_t<
                  detail::convertible_to_non_slicing<II, I> &&
                  KK == subrange_kind::sized>>
    constexpr subrange(II i, S s, iter_difference_t<I> n)
            : data_{std::move(i), std::move(s), n} {}

    template <typename R, bool SS = StoreSize,
            std::enable_if_t<detail::not_same_as<R, subrange>, int> = 0,
            std::enable_if_t<
                    detail::subrange_range_constructor_constraint_helper<R, I, S, K>
                    && SS && sized_range<R>, int> = 0>
    constexpr subrange(R&& r)
            : subrange(ranges::begin(r), ranges::end(r), ranges::size(r)) {}

    template <typename R, bool SS = StoreSize,
            std::enable_if_t<detail::not_same_as<R, subrange>, int> = 0,
            std::enable_if_t<
                    detail::subrange_range_constructor_constraint_helper<R, I, S, K>
                     && !SS, int> = 0>
    constexpr subrange(R&& r)
            : subrange(ranges::begin(r), ranges::end(r)) {}

    template <typename R, subrange_kind KK = K, std::enable_if_t<
            borrowed_range<R> &&
            detail::convertible_to_non_slicing<iterator_t<R>, I> &&
            convertible_to<sentinel_t<R>, S> &&
            KK == subrange_kind::sized, int> = 0>
    constexpr subrange(R&& r, iter_difference_t<I> n)
            : subrange(ranges::begin(r), ranges::end(r), n) {}


    template <typename PairLike_,
            std::enable_if_t<detail::not_same_as<PairLike_, subrange>, int> = 0,
            std::enable_if_t<detail::pair_like_convertible_from<
                                PairLike_, const I&, const S&>, int> = 0>
    constexpr operator PairLike_() const
    {
        return PairLike_(begin(), end());
    }

    constexpr I begin() const { return data_.begin_; }

    constexpr S end() const { return data_.end_; }

    [[nodiscard]] constexpr bool empty() const
    {
        return data_.begin_ == data_.end_;
    }

    template <subrange_kind KK = K>
    constexpr auto size() const
        -> std::enable_if_t<KK == subrange_kind::sized, iter_difference_t<I>>
    {
        if constexpr (StoreSize) {
            return data_.size_;
        } else {
            return data_.end_ - data_.begin_;
        }
    }

    [[nodiscard]] constexpr subrange next(iter_difference_t<I> n = 1) const
    {
        auto tmp = *this;
        tmp.advance(n);
        return tmp;
    }

    template <typename II = I>
    [[nodiscard]] constexpr auto prev(iter_difference_t<I> n = 1) const
        -> std::enable_if_t<bidirectional_iterator<II>, subrange>
    {
        auto tmp = *this;
        tmp.advance(-n);
        return tmp;
    }

    constexpr subrange& advance(iter_difference_t<I> n)
    {
        if constexpr (StoreSize) {
            data_.size_ -= n - ranges::advance(data_.begin_, n, data_.end_);
        } else {
            ranges::advance(data_.begin_, n, data_.end_);
        }
        return *this;
    }
};

template <typename I, typename S,
          std::enable_if_t<input_or_output_iterator<I> && sentinel_for<S, I>, int> = 0>
subrange(I, S) -> subrange<I, S>;

template <typename I, typename S, std::enable_if_t<input_or_output_iterator<I> && sentinel_for<S, I>, int> = 0>
subrange(I, S, iter_difference_t<I>) -> subrange<I, S, subrange_kind::sized>;

template <typename P, std::enable_if_t<detail::iterator_sentinel_pair<P>, int> = 0>
subrange(P) -> subrange<std::tuple_element_t<0, P>, std::tuple_element_t<1, P>>;

template <typename P, std::enable_if_t<detail::iterator_sentinel_pair<P>, int> = 0>
subrange(P, iter_difference_t<std::tuple_element_t<0, P>>) ->
    subrange<std::tuple_element_t<0, P>, std::tuple_element_t<1, P>, subrange_kind::sized>;

template <typename R, std::enable_if_t<borrowed_range<R>, int> = 0>
subrange(R&&) ->
    subrange<iterator_t<R>, sentinel_t<R>,
             detail::subrange_deduction_guide_helper<R>()>;

template <typename R, std::enable_if_t<borrowed_range<R>, int> = 0>
subrange(R&&, iter_difference_t<iterator_t<R>>) ->
    subrange<iterator_t<R>, sentinel_t<R>, subrange_kind::sized>;

} // namespace subrange_

template <typename I, typename S, subrange_kind K>
inline constexpr bool enable_borrowed_range<subrange<I, S, K>> = true;

template <std::size_t N, typename I, typename S, subrange_kind K,
          std::enable_if_t<(N < 2), int> = 0>
constexpr auto get(const subrange<I, S, K>& r)
{
    if constexpr (N == 0) {
        return r.begin();
    } else {
        return r.end();
    }
}

template <typename R>
using borrowed_subrange_t =
    detail::conditional_t<borrowed_range<R>, subrange<iterator_t<R>>, dangling>;

NANO_END_NAMESPACE

namespace std {

template <typename I, typename S, ::nano::subrange_kind K>
class tuple_size<::nano::subrange<I, S, K>>
    : public integral_constant<size_t, 2> {
};

template <typename I, typename S, ::nano::subrange_kind K>
class tuple_element<0, ::nano::subrange<I, S, K>> {
public:
    using type = I;
};

template <typename I, typename S, ::nano::subrange_kind K>
class tuple_element<1, ::nano::subrange<I, S, K>> {
public:
    using type = S;
};

using ::nano::ranges::get;

} // namespace std

#endif


NANO_BEGIN_NAMESPACE

namespace detail {

struct equal_range_fn {
private:
    template <typename I, typename S, typename T, typename Comp, typename Proj>
    static constexpr subrange<I> impl(I first, S last, const T& value,
                                      Comp& comp, Proj& proj)
    {
        return {lower_bound_fn::impl(first, last, value, comp, proj),
                upper_bound_fn::impl(first, last, value, comp, proj)};
    }

public:
    template <typename I, typename S, typename T, typename Comp = ranges::less,
              typename Proj = identity>
    std::enable_if_t<
        forward_iterator<I> && sentinel_for<S, I> &&
            indirect_strict_weak_order<Comp, const T*, projected<I, Proj>>,
    subrange<I>>
    constexpr operator()(I first, S last, const T& value, Comp comp = Comp{},
               Proj proj = Proj{}) const
    {
        return equal_range_fn::impl(std::move(first), std::move(last),
                                    value, comp, proj);
    }

    template <typename Rng, typename T, typename Comp = ranges::less,
              typename Proj = identity>
    std::enable_if_t<forward_range<Rng> &&
                         indirect_strict_weak_order<Comp, const T*, projected<iterator_t<Rng>, Proj>>,
                     borrowed_subrange_t<Rng>>
    constexpr operator()(Rng&& rng, const T& value, Comp comp = Comp{},
                         Proj proj = Proj{}) const
    {
        return equal_range_fn::impl(nano::begin(rng), nano::end(rng),
                                    value, comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::equal_range_fn, equal_range)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/fill.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_FILL_HPP_INCLUDED
#define NANORANGE_ALGORITHM_FILL_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct fill_fn {
private:
    template <typename T, typename O, typename S>
    static constexpr O impl(O first, S last, const T& value)
    {
        while (first != last) {
            *first = value;
            ++first;
        }

        return first;
    }

public:
    template <typename T, typename O, typename S>
    constexpr std::enable_if_t<
        output_iterator<O, const T&> && sentinel_for<S, O>, O>
    operator()(O first, S last, const T& value) const
    {
        return fill_fn::impl(std::move(first), std::move(last), value);
    }

    template <typename T, typename Rng>
    constexpr std::enable_if_t<output_range<Rng, const T&>,
                               borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, const T& value) const
    {
        return fill_fn::impl(nano::begin(rng), nano::end(rng), value);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::fill_fn, fill)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/fill_n.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_FILL_N_HPP_INCLUDED
#define NANORANGE_ALGORITHM_FILL_N_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct fill_n_fn {
    template <typename T, typename O>
    constexpr std::enable_if_t<output_iterator<O, const T&>, O>
    operator()(O first, iter_difference_t<O> n, const T& value) const
    {
        for (iter_difference_t<O> i{0}; i < n; ++i, ++first) {
            *first = value;
        }
        return first;
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::fill_n_fn, fill_n)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/find.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_FIND_HPP_INCLUDED
#define NANORANGE_ALGORITHM_FIND_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

// [ranges.alg.find]

namespace detail {

struct find_if_fn {
private:
    friend struct find_fn;
    friend struct find_if_not_fn;

    template <typename I, typename S, typename Pred, typename Proj>
    static constexpr I impl(I first, S last, Pred& pred, Proj& proj)
    {
        while (first != last) {
            if (nano::invoke(pred, nano::invoke(proj, *first))) {
                return first;
            }
            ++first;
        }
        return first;
    }

public:
    template <typename I, typename S, typename Proj = identity, typename Pred>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            indirect_unary_predicate<Pred, projected<I, Proj>>,
        I>
    operator()(I first, S last, Pred pred, Proj proj = Proj{}) const
    {
        return find_if_fn::impl(std::move(first), std::move(last), pred, proj);
    }

    template <typename Rng, typename Proj = identity, typename Pred>
    constexpr std::enable_if_t<
        input_range<Rng> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>>,
        borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Pred pred, Proj proj = Proj{}) const
    {
        return find_if_fn::impl(nano::begin(rng), nano::end(rng), pred, proj);
    }
};
} // namespace detail

NANO_INLINE_VAR(detail::find_if_fn, find_if)

namespace detail {

struct find_fn {
    template <typename I, typename S, typename T, typename Proj = identity>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            indirect_relation<ranges::equal_to, projected<I, Proj>, const T*>,
        I>
    operator()(I first, S last, const T& value, Proj proj = Proj{}) const
    {
        const auto pred = [&value] (const auto& t) { return t == value; };
        return find_if_fn::impl(std::move(first), std::move(last), pred, proj);
    }

    template <typename Rng, typename T, typename Proj = identity>
    constexpr std::enable_if_t<
        input_range<Rng> &&
            indirect_relation<ranges::equal_to, projected<iterator_t<Rng>, Proj>,
                             const T*>,
        borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, const T& value, Proj proj = Proj{}) const
    {
        const auto pred = [&value] (const auto& t) { return t == value; };
        return find_if_fn::impl(nano::begin(rng), nano::end(rng), pred, proj);
    }
};
} // namespace detail

NANO_INLINE_VAR(detail::find_fn, find)

namespace detail {

struct find_if_not_fn {
private:
    template <typename Pred>
    struct not_pred {
        Pred& p;

        template <typename T>
        constexpr bool operator()(T&& t) const
        {
            return !nano::invoke(p, std::forward<T>(t));
        }
    };

public:
    template <typename I, typename S, typename Proj = identity, typename Pred>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            indirect_unary_predicate<Pred, projected<I, Proj>>,
        I>
    operator()(I first, S last, Pred pred, Proj proj = Proj{}) const
    {
        const auto find_if_pred = not_pred<Pred>{pred};
        return find_if_fn::impl(std::move(first), std::move(last),
                                find_if_pred, proj);
    }

    template <typename Rng, typename Proj = identity, typename Pred>
    constexpr std::enable_if_t<
        input_range<Rng> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>>,
        borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Pred pred, Proj proj = Proj{}) const
    {
        const auto find_if_pred = not_pred<Pred>{pred};
        return find_if_fn::impl(nano::begin(rng), nano::end(rng),
                                find_if_pred, proj);
    }
};
} // namespace detail

NANO_INLINE_VAR(detail::find_if_not_fn, find_if_not)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/find_end.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_FIND_END_HPP_INCLUDED
#define NANORANGE_ALGORITHM_FIND_END_HPP_INCLUDED



// nanorange/algorithm/search.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_SEARCH_HPP_INCLUDED
#define NANORANGE_ALGORITHM_SEARCH_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

namespace detail {

struct search_fn {
private:
    friend struct find_end_fn;

    template <typename I1, typename S1, typename I2, typename S2,
              typename Pred, typename Proj1, typename Proj2 = identity>
    static constexpr subrange<I1>
    impl(I1 first1, S1 last1, I2 first2, S2 last2, Pred& pred, Proj1& proj1,
         Proj2& proj2)
    {
        while (true) {
            auto it1 = first1;
            auto it2 = first2;

            while (true) {
                if (it2 == last2) {
                    return {first1, it1};
                }
                if (it1 == last1) {
                    return {it1, it1};
                }
                if (!nano::invoke(pred, nano::invoke(proj1, *it1), nano::invoke(proj2, *it2))) {
                    break;
                }
                ++it1; ++it2;
            }


            ++first1;
        }
    }

public:
    template <typename I1, typename S1, typename I2, typename S2,
              typename Pred = ranges::equal_to, typename Proj1 = identity,
              typename Proj2 = identity>
    constexpr std::enable_if_t<
        forward_iterator<I1> && sentinel_for<S1, I1> && forward_iterator<I2> &&
            sentinel_for<S2, I2> &&
            indirectly_comparable<I1, I2, Pred, Proj1, Proj2>,
         subrange<I1>>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2,
               Pred pred = Pred{}, Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return search_fn::impl(std::move(first1), std::move(last1),
                               std::move(first2), std::move(last2),
                               pred, proj1, proj2);
    }

    template <typename Rng1, typename Rng2, typename Pred = ranges::equal_to,
              typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<
        forward_range<Rng1> && forward_range<Rng2> &&
            indirectly_comparable<iterator_t<Rng1>, iterator_t<Rng2>, Pred, Proj1, Proj2>,
        borrowed_subrange_t<Rng1>>
    operator()(Rng1&& rng1, Rng2&& rng2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return search_fn::impl(nano::begin(rng1), nano::end(rng1),
                               nano::begin(rng2), nano::end(rng2),
                               pred, proj1, proj2);
    }
};

}

NANO_INLINE_VAR(detail::search_fn, search)

NANO_END_NAMESPACE

#endif

NANO_BEGIN_NAMESPACE

// [ranges.alg.find.end]
namespace detail {

// TODO: For BiDir iterators, we can be smarter and search backwards
struct find_end_fn {
private:
    template <typename I1, typename S1, typename I2, typename S2,
              typename Pred, typename Proj1, typename Proj2>
    static constexpr subrange<I1>
    impl(I1 first1, S1 last1, I2 first2, S2 last2, Pred& pred, Proj1& proj1,
         Proj2& proj2)
    {
        if (first2 == last2) {
            auto last_it = nano::next(first1, last1);
            return {last_it, last_it};
        }

        auto result = search_fn::impl(std::move(first1), last1, first2, last2, pred, proj1, proj2);

        if (result.empty()) {
            return result;
        }

        while (true) {
            auto new_result = search_fn::impl(next(result.begin()), last1, first2, last2, pred, proj1, proj2);
            if (new_result.empty()) {
                return result;
            } else {
                result = std::move(new_result);
            }
        }
    }

public:
    template <typename I1, typename S1, typename I2, typename S2,
            typename Pred = ranges::equal_to, typename Proj1 = identity,
            typename Proj2 = identity>
    constexpr std::enable_if_t<
        forward_iterator<I1> && sentinel_for<S1, I1> && forward_iterator<I2> &&
            sentinel_for<S2, I2> &&
            indirectly_comparable<I1, I2, Pred, Proj1, Proj2>,
        subrange<I1>>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return find_end_fn::impl(std::move(first1), std::move(last1),
                                 std::move(first2), std::move(last2),
                                 pred, proj1, proj2);
    }

    template <typename Rng1, typename Rng2, typename Pred = ranges::equal_to,
              typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<
        forward_range<Rng1> && forward_range<Rng2> &&
            indirectly_comparable<iterator_t<Rng1>, iterator_t<Rng2>, Pred, Proj1, Proj2>,
        borrowed_subrange_t<Rng1>>
    operator()(Rng1&& rng1, Rng2&& rng2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return find_end_fn::impl(nano::begin(rng1), nano::end(rng1),
                                 nano::begin(rng2), nano::end(rng2),
                                 pred, proj1, proj2);
    }

};

}

NANO_INLINE_VAR(detail::find_end_fn, find_end)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/find_first_of.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_FIND_FIRST_OF_HPP_INCLUDED
#define NANORANGE_ALGORITHM_FIND_FIRST_OF_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

// [range.alg.find.first.of]

namespace detail {

struct find_first_of_fn {
private:
    template <typename I1, typename S1, typename I2, typename S2, typename Pred,
              typename Proj1, typename Proj2>
    static constexpr I1 impl(I1 first1, S1 last1, I2 first2, S2 last2,
                             Pred& pred, Proj1& proj1, Proj2& proj2)
    {
        for (; first1 != last1; ++first1) {
            for (I2 it = first2; it != last2; ++it) {
                if (nano::invoke(pred, nano::invoke(proj1, *first1),
                                 nano::invoke(proj2, *it))) {
                    return first1;
                }
            }
        }

        return first1;
    }

public:
    template <typename I1, typename S1, typename I2, typename S2,
              typename Proj1 = identity, typename Proj2 = identity,
              typename Pred = ranges::equal_to>
    constexpr std::enable_if_t<
        input_iterator<I1> && sentinel_for<S1, I1> && forward_iterator<I2> &&
            sentinel_for<S2, I2> &&
            indirect_relation<Pred, projected<I1, Proj1>, projected<I2, Proj2>>,
        I1>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return find_first_of_fn::impl(std::move(first1), std::move(last1),
                                      std::move(first2), std::move(last2),
                                      pred, proj1, proj2);
    }

    template <typename Rng1, typename Rng2, typename Proj1 = identity,
              typename Proj2 = identity, typename Pred = ranges::equal_to>
    constexpr std::enable_if_t<
        input_range<Rng1> && forward_range<Rng2> &&
            indirect_relation<Pred, projected<iterator_t<Rng1>, Proj1>,
                             projected<iterator_t<Rng2>, Proj2>>,
        borrowed_iterator_t<Rng1>>
    operator()(Rng1&& rng1, Rng2&& rng2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return find_first_of_fn::impl(nano::begin(rng1), nano::end(rng1),
                                      nano::begin(rng2), nano::end(rng2),
                                      pred, proj1, proj2);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::find_first_of_fn, find_first_of)

NANO_END_NAMESPACE

#endif
// nanorange/algorithm/for_each.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_FOR_EACH_HPP_INCLUDED
#define NANORANGE_ALGORITHM_FOR_EACH_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

// [range.alg.foreach]

template <typename I, typename F>
using for_each_result = in_fun_result<I, F>;

namespace detail {

struct for_each_fn {
private:
    template <typename I, typename S, typename Proj, typename Fun>
    static constexpr for_each_result<I, Fun>
    impl(I first, S last, Fun& fun, Proj& proj)
    {
        while (first != last) {
            nano::invoke(fun, nano::invoke(proj, *first));
            ++first;
        }
        return {first, std::move(fun)};
    }

public:
    template <typename I, typename S, typename Proj = identity, typename Fun>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            indirect_unary_invocable<Fun, projected<I, Proj>>,
        for_each_result<I, Fun>>
    operator()(I first, S last, Fun fun, Proj proj = Proj{}) const
    {
        return for_each_fn::impl(std::move(first), std::move(last),
                                 fun, proj);
    }

    template <typename Rng, typename Proj = identity, typename Fun>
    constexpr std::enable_if_t<
        input_range<Rng> &&
            indirect_unary_invocable<Fun, projected<iterator_t<Rng>, Proj>>,
        for_each_result<borrowed_iterator_t<Rng>, Fun>>
    operator()(Rng&& rng, Fun fun, Proj proj = Proj{}) const
    {
        return for_each_fn::impl(nano::begin(rng), nano::end(rng),
                                 fun, proj);
    }
};
} // namespace detail

NANO_INLINE_VAR(detail::for_each_fn, for_each)

template <typename I, typename F>
using for_each_n_result = in_fun_result<I, F>;

namespace detail {

struct for_each_n_fn {
    template <typename I, typename Proj = identity, typename Fun>
    constexpr std::enable_if_t<
        input_iterator<I> &&
            indirect_unary_invocable<Fun, projected<I, Proj>>,
        for_each_n_result<I, Fun>>
    operator()(I first, iter_difference_t<I> n, Fun fun, Proj proj = Proj{}) const
    {
        while (n-- > 0) {
            nano::invoke(fun, nano::invoke(proj, *first));
            ++first;
        }
        return {std::move(first), std::move(fun)};
    }
};
} // namespace detail

NANO_INLINE_VAR(detail::for_each_n_fn, for_each_n)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/generate.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_GENERATE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_GENERATE_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct generate_fn {
private:
    template <typename O, typename S, typename F>
    static constexpr O impl(O first, S last, F& gen)
    {
        while (first != last) {
            *first = gen();
            ++first;
        }

        return first;
    }

public:
    template <typename O, typename S, typename F>
    constexpr std::enable_if_t<input_or_output_iterator<O> &&
                                   sentinel_for<S, O> &&
                                   copy_constructible<F> && invocable<F&> &&
                                   writable<O, invoke_result_t<F&>>,
                               O>
    operator()(O first, S last, F gen) const
    {
        return generate_fn::impl(std::move(first), std::move(last), gen);
    }

    template <typename Rng, typename F>
    constexpr std::enable_if_t<invocable<F&> &&
                                   output_range<Rng, invoke_result_t<F&>>,
                               borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, F gen) const
    {
        return generate_fn::impl(nano::begin(rng), nano::end(rng), gen);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::generate_fn, generate)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/generate_n.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_GENERATE_N_HPP_INCLUDED
#define NANORANGE_ALGORITHM_GENERATE_N_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct generate_n_fn {
    template <typename O, typename F>
    constexpr std::enable_if_t<input_or_output_iterator<O> && copy_constructible<F> &&
                                   invocable<F&> &&
                                   writable<O, invoke_result_t<F&>>,
                               O>
    operator()(O first, iter_difference_t<O> n, F gen) const
    {
        for (iter_difference_t<O> i{0}; i < n; ++i, ++first) {
            *first = gen();
        }

        return first;
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::generate_n_fn, generate_n)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/includes.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_INCLUDES_HPP_INCLUDED
#define NANORANGE_ALGORITHM_INCLUDES_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct includes_fn {
private:
    template <typename I1, typename S1, typename I2, typename S2,
              typename Comp, typename Proj1, typename Proj2>
     static constexpr bool impl(I1 first1, S1 last1, I2 first2, S2 last2,
                                Comp& comp, Proj1& proj1, Proj2& proj2)
    {
        while (first2 != last2) {
            // If range1 is done but we still have elements in range2, then
            // it is not a subset
            if (first1 == last1)  {
                return false;
            }

            // If the current element of r2 is less than the current
            // element of r1, then it is not in r1 => not a subset
            if (nano::invoke(comp, nano::invoke(proj2, *first2),
                             nano::invoke(proj1, *first1))) {
                return false;
            }

            // Now we know that that !(r2 < r1). If we also have !(r1 < r2),
            // then it must be equal, so in range1 -- so move onto the next
            // element
            if (!nano::invoke(comp, nano::invoke(proj1, *first1),
                              nano::invoke(proj2, *first2))) {
                ++first2;
            }

            ++first1;
        }

        return true;
    }

public:
    template <typename I1, typename S1, typename I2, typename S2,
              typename Comp = ranges::less, typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<
        input_iterator<I1> && sentinel_for<S1, I1> && input_iterator<I2> &&
            sentinel_for<S2, I2> &&
            indirect_strict_weak_order<Comp, projected<I1, Proj1>, projected<I2, Proj2>>,
        bool>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2,
               Comp comp = Comp{}, Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return includes_fn::impl(std::move(first1), std::move(last1),
                                 std::move(first2), std::move(last2),
                                 comp, proj1, proj2);
    }

    template <typename Rng1, typename Rng2, typename Comp = ranges::less,
              typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<
        input_range<Rng1> && input_range<Rng2> &&
            indirect_strict_weak_order<Comp,
                                projected<iterator_t<Rng1>, Proj1>,
                                projected<iterator_t<Rng2>, Proj2>>,
        bool>
    operator()(Rng1&& rng1, Rng2&& rng2, Comp comp = Comp{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return includes_fn::impl(nano::begin(rng1), nano::end(rng1),
                                 nano::begin(rng2), nano::end(rng2),
                                 comp, proj1, proj2);
    }
};

}

NANO_INLINE_VAR(detail::includes_fn, includes)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/inplace_merge.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Uses code from cmcstl2 - A concept-enabled C++ standard library
//
//  Copyright Eric Niebler 2014
//  Copyright Casey Carter 2015
//

//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef NANORANGE_ALGORITHM_INPLACE_MERGE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_INPLACE_MERGE_HPP_INCLUDED


// nanorange/algorithm/merge.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_MERGE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_MERGE_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I1, typename I2, typename O>
using merge_result = in_in_out_result<I1, I2, O>;

namespace detail {

struct merge_fn {
private:
    template <typename I1, typename S1, typename I2, typename S2, typename O,
              typename Comp, typename Proj1, typename Proj2>
    static constexpr merge_result<I1, I2, O>
    impl(I1 first1, S1 last1, I2 first2, S2 last2, O result, Comp& comp,
         Proj1& proj1, Proj2& proj2)
    {
        while (first1 != last1) {
            // If we've reached the end of the second range, copy any remaining
            // elements from the first range directly
            if (first2 == last2) {
                auto res = nano::copy(std::move(first1),
                                      std::move(last1),
                                      std::move(result));
                first1 = std::move(res.in);
                result = std::move(res.out);
                break;
            }

            // (Only) if the element from range2 compares less than the element
            // from range1, copy it. Otherwise copy the element from the first
            if (nano::invoke(comp, nano::invoke(proj2, *first2),
                             nano::invoke(proj1, *first1))) {
                *result = *first2;
                ++first2;
            } else {
                *result = *first1;
                ++first1;
            }
            ++result;
        }

        // We've reached the end of range1, so copy any remaining elements
        // from range2
        auto res = nano::copy(std::move(first2), std::move(last2),
                              std::move(result));
        first2 = std::move(res.in);
        result = std::move(res.out);

        return {std::move(first1), std::move(first2),  std::move(result)};
    }

public:
    template <typename I1, typename S1, typename I2, typename S2, typename O,
              typename Comp = ranges::less, typename Proj1 = identity,
              typename Proj2 = identity>
    constexpr std::enable_if_t<input_iterator<I1> && sentinel_for<S1, I1> &&
                                   input_iterator<I2> && sentinel_for<S2, I2> &&
        weakly_incrementable<O> &&
                                   mergeable<I1, I2, O, Comp, Proj1, Proj1>,
        merge_result<I1, I2, O>>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, O result,
               Comp comp = Comp{}, Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return merge_fn::impl(std::move(first1), std::move(last1),
                              std::move(first2), std::move(last2),
                              std::move(result), comp,
                              proj1, proj2);
    }

    template <typename Rng1, typename Rng2, typename O, typename Comp = ranges::less,
              typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<
        input_range<Rng1> && input_range<Rng2> &&
        weakly_incrementable<O> &&
            mergeable<iterator_t<Rng1>, iterator_t<Rng2>, O, Comp, Proj1, Proj2>,
        merge_result<borrowed_iterator_t<Rng1>, borrowed_iterator_t<Rng2>, O>>
    operator()(Rng1&& rng1, Rng2&& rng2, O result, Comp comp = Comp{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return merge_fn::impl(nano::begin(rng1), nano::end(rng1),
                              nano::begin(rng2), nano::end(rng2),
                              std::move(result), comp,
                              proj1, proj2);
    }
};

}

NANO_INLINE_VAR(detail::merge_fn, merge)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/min.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_MIN_HPP_INCLUDED
#define NANORANGE_ALGORITHM_MIN_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct min_fn {
private:
    template <typename Rng, typename Comp, typename Proj>
    static constexpr iter_value_t<iterator_t<Rng>>
    impl(Rng&& rng, Comp& comp, Proj& proj)
    {
        auto first = nano::begin(rng);
        const auto last = nano::end(rng);

        // Empty ranges not allowed
        auto result = *first;

        while(++first != last) {
            auto&& val = *first;
            if (nano::invoke(comp, nano::invoke(proj, val),
                             nano::invoke(proj, result))) {
                result = std::forward<decltype(val)>(val);
            }
        }

        return result;
    }

public:
    template <typename T, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        indirect_strict_weak_order<Comp, projected<const T*, Proj>>,
        const T&>
    operator()(const T& a, const T& b, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return nano::invoke(comp, nano::invoke(proj, b),
                            nano::invoke(proj, a)) ? b : a;
    }

    template <typename T, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        copyable<T> &&
            indirect_strict_weak_order<Comp, projected<const T*, Proj>>,
        T>
    operator()(std::initializer_list<T> rng, Comp comp = Comp{},
               Proj proj = Proj{}) const
    {
        return min_fn::impl(rng, comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        input_range<Rng> && copyable<iter_value_t<iterator_t<Rng>>> &&
            indirect_strict_weak_order<Comp, projected<iterator_t<Rng>, Proj>>,
        range_value_t<Rng>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return min_fn::impl(std::forward<Rng>(rng), comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::min_fn, min)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/move.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_MOVE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_MOVE_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I, typename O>
using move_result = in_out_result<I, O>;

namespace detail {

struct move_fn {
private:
    template <typename I, typename S, typename O>
    static constexpr std::enable_if_t<sized_sentinel_for<S, I>, move_result<I, O>>
    impl(I first, S last, O result, priority_tag<1>)
    {
        const auto dist = last - first;

        for (iter_difference_t<I> i{0}; i < dist; i++) {
            *result = nano::iter_move(first);
            ++first;
            ++result;
        }

        return {std::move(first), std::move(result)};
    }

    template <typename I, typename S, typename O>
    static constexpr move_result<I, O> impl(I first, S last, O result,
                                            priority_tag<0>)
    {
        while (first != last) {
            *result = nano::iter_move(first);
            ++first;
            ++result;
        }

        return {std::move(first), std::move(result)};
    }

public:
    template <typename I, typename S, typename O>
    constexpr std::enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                                   weakly_incrementable<O> &&
                                   indirectly_movable<I, O>,
                               move_result<I, O>>
    operator()(I first, S last, O result) const
    {
        return move_fn::impl(std::move(first), std::move(last),
                             std::move(result), priority_tag<1>{});
    }

    template <typename Rng, typename O>
    constexpr std::enable_if_t<input_range<Rng> && weakly_incrementable<O> &&
                                   indirectly_movable<iterator_t<Rng>, O>,
                               move_result<borrowed_iterator_t<Rng>, O>>
    operator()(Rng&& rng, O result) const
    {
        return move_fn::impl(nano::begin(rng), nano::end(rng),
                             std::move(result), priority_tag<1>{});
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::move_fn, move)

template <typename I, typename O>
using move_backward_result = in_out_result<I, O>;

namespace detail {

struct move_backward_fn {
private:
    template <typename I, typename O>
    static constexpr move_backward_result<I, O> impl(I first, I last, O result)
    {
        auto it = last;

        while (it != first) {
            *--result = nano::iter_move(--it);
        }

        return {std::move(last), std::move(result)};
    }

    template <typename I, typename S, typename O>
    static constexpr std::enable_if_t<!same_as<I, S>, move_backward_result<I, O>>
    impl(I first, S sent, O result)
    {
        I last = nano::next(first, sent);
        return impl(std::move(first), std::move(last), std::move(result));
    }

public:
    template <typename I, typename S, typename O>
    constexpr std::enable_if_t<
        bidirectional_iterator<I> && sentinel_for<S, I> &&
            bidirectional_iterator<O> && indirectly_movable<I, O>,
                               move_backward_result<I, O>>
    operator()(I first, S last, O result) const
    {
        return move_backward_fn::impl(std::move(first), std::move(last),
                                      std::move(result));
    }

    template <typename Rng, typename O>
    constexpr std::enable_if_t<bidirectional_range<Rng> &&
                                   bidirectional_iterator<O> &&
                                   indirectly_movable<iterator_t<Rng>, O>,
                               move_backward_result<borrowed_iterator_t<Rng>, O>>
    operator()(Rng&& rng, O result) const
    {
        return move_backward_fn::impl(nano::begin(rng), nano::end(rng),
                                      std::move(result));
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::move_backward_fn, move_backward)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/rotate.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_ROTATE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_ROTATE_HPP_INCLUDED


// nanorange/algorithm/swap_ranges.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_SWAP_RANGES_HPP_INCLUDED
#define NANORANGE_ALGORITHM_SWAP_RANGES_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I1, typename I2>
using swap_ranges_result = in_in_result<I1, I2>;

namespace detail {

struct swap_ranges_fn {
    template <typename I1, typename S1, typename I2, typename S2>
    static constexpr swap_ranges_result<I1, I2>
    impl4(I1 first1 ,S1 last1, I2 first2, S2 last2)
    {
        while (first1 != last1 && first2 != last2) {
            nano::iter_swap(first1, first2);
            ++first1; ++first2;
        }
        return {std::move(first1), std::move(first2)};
    }

    template <typename I1, typename S1, typename I2>
    static constexpr swap_ranges_result<I1, I2>
    impl3(I1 first1, S1 last1, I2 first2)
    {
        while (first1 != last1) {
            nano::iter_swap(first1, first2);
            ++first1; ++first2;
        }
        return {std::move(first1), std::move(first2)};
    }

public:
    template <typename I1, typename S1, typename I2, typename S2>
    constexpr std::enable_if_t<
        forward_iterator<I1> && sentinel_for<S1, I1> && forward_iterator<I2> &&
            sentinel_for<S2, I2> && indirectly_swappable<I1, I2>,
        swap_ranges_result<I1, I2>>
    operator()(I1 first1 ,S1 last1, I2 first2, S2 last2) const
    {
        return swap_ranges_fn::impl4(std::move(first1), std::move(last1),
                                     std::move(first2), std::move(last2));
    }

    template <typename I1, typename S1, typename I2>
    NANO_DEPRECATED
    constexpr std::enable_if_t<
        forward_iterator<I1> && sentinel_for<S1, I1> && forward_iterator<I2> &&
            indirectly_swappable<I1, I2>,
            swap_ranges_result<I1, I2>>
    operator()(I1 first1 ,S1 last1, I2 first2) const
    {
        return swap_ranges_fn::impl3(std::move(first1), std::move(last1),
                                     std::move(first2));
    }

    template <typename Rng1, typename Rng2>
    constexpr std::enable_if_t<
        forward_range<Rng1> && forward_range<Rng2> &&
            indirectly_swappable<iterator_t<Rng1>, iterator_t<Rng2>>,
            swap_ranges_result<borrowed_iterator_t<Rng1>,
                           borrowed_iterator_t<Rng2>>>
    operator()(Rng1&& rng1, Rng2&& rng2) const
    {
        return swap_ranges_fn::impl4(nano::begin(rng1), nano::end(rng1),
                                     nano::begin(rng2), nano::end(rng2));
    }

    template <typename Rng1, typename I2>
    NANO_DEPRECATED
    constexpr std::enable_if_t<
        forward_range<Rng1> && forward_iterator<I2> &&
            indirectly_swappable<iterator_t<Rng1>, I2>,
            swap_ranges_result<borrowed_iterator_t<Rng1>, I2>>
    operator()(Rng1&& rng1, I2 first2) const
    {
        return swap_ranges_fn::impl3(nano::begin(rng1), nano::end(rng1),
                                     std::move(first2));
    }
};

}

NANO_INLINE_VAR(detail::swap_ranges_fn, swap_ranges)

NANO_END_NAMESPACE

#endif

// nanorange/iterator/unreachable.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_UNREACHABLE_HPP_INCLUDED
#define NANORANGE_ITERATOR_UNREACHABLE_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

// [range.unreachable.sentinels]

struct unreachable_sentinel_t {
    template<typename I>
    friend constexpr std::enable_if_t<weakly_incrementable<I>, bool>
    operator==(const I &, unreachable_sentinel_t) noexcept
    {
        return false;
    }

    template<typename I>
    friend constexpr std::enable_if_t<weakly_incrementable<I>, bool>
    operator==(unreachable_sentinel_t, const I &) noexcept
    {
        return false;
    }

    template<typename I>
    friend constexpr std::enable_if_t<weakly_incrementable<I>, bool>
    operator!=(const I &, unreachable_sentinel_t) noexcept
    {
        return true;
    }

    template<typename I>
    friend constexpr std::enable_if_t<weakly_incrementable<I>, bool>
    operator!=(unreachable_sentinel_t, const I &) noexcept
    {
        return true;
    }
};

inline constexpr unreachable_sentinel_t unreachable_sentinel{};

NANO_END_NAMESPACE

#endif



NANO_BEGIN_NAMESPACE

namespace detail {

struct rotate_fn {
private:
    template <typename I, typename S>
    static constexpr subrange<I> do_rotate_one_left(I first, S last)
    {
        // Stash the first element and move everything one place
        iter_value_t<I> val = nano::iter_move(first);
        auto ret = nano::move(nano::next(first), std::move(last), first);
        *ret.out = std::move(val);
        return {std::move(ret.out), std::move(ret.in)};
    }

    template <typename I>
    static constexpr subrange<I> do_rotate_one_right(I first, I middle)
    {
        I last = nano::next(middle);
        iter_value_t<I> val = nano::iter_move(middle);
        nano::move_backward(first, middle, last);
        *first = std::move(val);
        return {std::move(++first), std::move(last)};
    }

    template <typename I, typename S>
    static constexpr std::enable_if_t<
        random_access_iterator<I> && sized_sentinel_for<S, I>,
            subrange<I>>
    do_rotate(I first, I middle, S last, priority_tag<2>)
    {
        constexpr bool is_tma = std::is_trivially_move_assignable<iter_value_t<I>>::value;

        auto i = nano::distance(first, middle);
        auto j = nano::distance(first, last) - i;
        I out = first + (last - middle);

        while (i != j) {
            if (i > j) {
                if (is_tma && j == 1) {
                    do_rotate_one_right(middle - i, middle);
                    return {std::move(out), nano::next(first, last)};
                }
                nano::swap_ranges(middle - i, unreachable_sentinel, middle, middle + j);
                i -= j;
            } else {
                if (is_tma && i == 1) {
                    do_rotate_one_left(middle - i, middle + j);
                    return {std::move(out), nano::next(first, last)};
                }
                nano::swap_ranges(middle - i, middle, middle + j - i, unreachable_sentinel);
                j -= i;
            }
        }
        nano::swap_ranges(middle  - i, middle, middle, unreachable_sentinel);

        return {std::move(out), nano::next(first, last)};
    }

    template <typename I, typename S>
    static constexpr std::enable_if_t<bidirectional_iterator<I>, subrange<I>>
    do_rotate(I first, I middle, S last, priority_tag<1>)
    {
        if (std::is_trivially_move_assignable<iter_value_t<I>>::value &&
                nano::next(middle) == last) {
            return do_rotate_one_right(std::move(first), std::move(middle));
        }

        return do_rotate(std::move(first), std::move(middle), std::move(last),
                         priority_tag<0>{});
    }

    template <typename I, typename S>
    static constexpr subrange<I>
    do_rotate(I first, I middle, S last, priority_tag<0>)
    {
        if (std::is_trivially_move_assignable<iter_value_t<I>>::value &&
                nano::next(first) == middle) {
            return do_rotate_one_left(std::move(first), std::move(last));
        }

        if (sized_sentinel_for<I, I> && sized_sentinel_for<S, I> &&
            nano::distance(first, middle) == nano::distance(middle, last))
        {
            auto ret = nano::swap_ranges(first, middle, middle, unreachable_sentinel);
            return {std::move(ret.in1), std::move(ret.in2)};
        }

        I next = middle;

        do {
            nano::iter_swap(first++, next++);
            if (first == middle) {
                middle = next;
            }
        } while (next != last);

        I ret = first;
        next = middle;

        while (next != last) {
            nano::iter_swap(first++, next++);
            if (first == middle) {
                middle = next;
            } else if (next == last) {
                next = middle;
            }
        }

        return {std::move(ret), std::move(next)};
    }

    template <typename I, typename S>
    static constexpr subrange<I> impl(I first, I middle, S last)
    {
        if (first == middle) {
            auto ret = nano::next(first, last);
            return {ret, ret};
        }
        if (middle == last) {
            return {first, middle};
        }

        return do_rotate(std::move(first), std::move(middle), std::move(last),
                         priority_tag<2>{});
    }

public:
    template <typename I, typename S>
    constexpr std::enable_if_t<
        forward_iterator<I> && sentinel_for<S, I> && permutable<I>,
        subrange<I>>
    operator()(I first, I middle, S last) const
    {
        return rotate_fn::impl(std::move(first), std::move(middle), std::move(last));
    }

    template <typename Rng>
    constexpr std::enable_if_t<
        forward_range<Rng> && permutable<iterator_t<Rng>>,
                               borrowed_subrange_t<Rng>>
    operator()(Rng&& rng, iterator_t<Rng> middle) const
    {
        return rotate_fn::impl(nano::begin(rng), std::move(middle), nano::end(rng));
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::rotate_fn, rotate)

NANO_END_NAMESPACE

#endif



// nanorange/detail/memory/temporary_vector.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_MEMORY_TEMPORARY_VECTOR_HPP_INCLUDED
#define NANORANGE_DETAIL_MEMORY_TEMPORARY_VECTOR_HPP_INCLUDED

// nanorange/memory/destroy.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_MEMORY_DESTROY_HPP_INCLUDED
#define NANORANGE_MEMORY_DESTROY_HPP_INCLUDED

// nanorange/detail/memory/concepts.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_MEMORY_CONCEPTS_HPP_INCLUDED
#define NANORANGE_DETAIL_MEMORY_CONCEPTS_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct no_throw_input_iterator_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename I>
    static auto test(int) -> std::enable_if_t<
        input_iterator<I> &&
        std::is_lvalue_reference<iter_reference_t<I>>::value &&
        same_as<remove_cvref_t<iter_reference_t<I>>, iter_value_t<I>>,
        std::true_type>;
};

template <typename I>
NANO_CONCEPT no_throw_input_iterator =
    decltype(no_throw_input_iterator_concept::test<I>(0))::value;

template <typename S, typename I>
NANO_CONCEPT no_throw_sentinel = sentinel_for<S, I>;

struct no_throw_input_range_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename R>
    static auto test(int) -> std::enable_if_t<
        range<R> &&
        no_throw_input_iterator<iterator_t<R>> &&
        no_throw_sentinel<sentinel_t<R>, iterator_t<R>>,
        std::true_type>;
};

template <typename R>
NANO_CONCEPT no_throw_input_range =
    decltype(no_throw_input_range_concept::test<R>(0))::value;

template <typename I>
NANO_CONCEPT no_throw_forward_iterator =
    no_throw_input_iterator<I> && forward_iterator<I> && no_throw_sentinel<I, I>;

struct no_throw_forward_range_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename R>
    static auto test(int) -> std::enable_if_t<
        no_throw_input_range<R> &&
        no_throw_forward_iterator<iterator_t<R>>,
        std::true_type>;
};

template <typename R>
NANO_CONCEPT no_throw_forward_range =
    decltype(no_throw_forward_range_concept::test<R>(0))::value;

template <typename T>
void* voidify(T& ptr) noexcept
{
    return const_cast<void*>(static_cast<const volatile void*>(std::addressof(ptr)));
}

}

NANO_END_NAMESPACE

#endif

// nanorange/iterator/counted_iterator.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_COUNTED_ITERATOR_HPP_INCLUDED
#define NANORANGE_ITERATOR_COUNTED_ITERATOR_HPP_INCLUDED





// nanorange/iterator/default_sentinel.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_DEFAULT_SENTINEL_HPP_INCLUDED
#define NANORANGE_ITERATOR_DEFAULT_SENTINEL_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

struct default_sentinel_t {};

inline constexpr default_sentinel_t default_sentinel{};

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

namespace counted_iterator_ {

template <typename I>
class counted_iterator {
    static_assert(input_or_output_iterator<I>, "");

    template <typename I2>
    friend class counted_iterator;

public:
    using iterator = I;
    using difference_type = iter_difference_t<I>;

    constexpr counted_iterator() = default;

    constexpr counted_iterator(I x, iter_difference_t<I> n)
        : current_(x), cnt_(n)
    {}

    template <typename I2, std::enable_if_t<convertible_to<I2, I>, int> = 0>
    constexpr counted_iterator(const counted_iterator<I2>& i)
        : current_(i.current_), cnt_(i.cnt_)
    {}

    template <typename I2>
    constexpr auto operator=(const counted_iterator<I2>& i)
        -> std::enable_if_t<convertible_to<I2, I>, counted_iterator&>
    {
        current_ = i.current_;
        cnt_ = i.cnt_;
        return *this;
    }

    constexpr I base() const { return current_; }

    constexpr iter_difference_t<I> count() const { return cnt_; }

    constexpr decltype(auto) operator*() { return *current_; }

    template <typename II = I,
              std::enable_if_t<detail::dereferenceable<const II>, int> = 0>
    constexpr decltype(auto) operator*() const
    {
        return *current_;
    }

    constexpr counted_iterator& operator++()
    {
        ++current_;
        --cnt_;
        return *this;
    }

    template <typename II = I, std::enable_if_t<!forward_iterator<II>, int> = 0>
    decltype(auto) operator++(int)
    {
        --cnt_;
        try {
            return current_++;
        } catch (...) {
            ++cnt_;
            throw;
        }
    }

    template <typename II = I>
    constexpr auto
    operator++(int) -> std::enable_if_t<forward_iterator<II>, counted_iterator>
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    template <typename II = I>
    constexpr auto operator--()
        -> std::enable_if_t<bidirectional_iterator<II>, counted_iterator&>
    {
        --current_;
        ++cnt_;
        return *this;
    }

    template <typename II = I>
    constexpr auto operator--(int)
        -> std::enable_if_t<bidirectional_iterator<II>, counted_iterator>
    {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    template <typename II = I>
    constexpr auto operator+(difference_type n) const
        -> std::enable_if_t<random_access_iterator<II>, counted_iterator>
    {
        return counted_iterator(current_ + n, cnt_ - n);
    }

    template <typename II = I, std::enable_if_t<random_access_iterator<II>, int> = 0>
    friend constexpr counted_iterator operator+(iter_difference_t<II> n,
                                                const counted_iterator<II>& x)
    {
        return x + n;
    }

    template <typename II = I>
    constexpr auto operator+=(difference_type n)
        -> std::enable_if_t<random_access_iterator<II>, counted_iterator&>
    {
        current_ += n;
        cnt_ -= n;
        return *this;
    }

    template <typename II = I>
    constexpr auto operator-(difference_type n) const
        -> std::enable_if_t<random_access_iterator<II>, counted_iterator>
    {
        return counted_iterator(current_ - n, cnt_ + n);
    }

    template <typename II = I,
              std::enable_if_t<random_access_iterator<II>, int> = 0>
    constexpr decltype(auto) operator[](difference_type n) const
    {
        return current_[n];
    }

    template <typename I2>
    friend constexpr auto operator==(const counted_iterator& x,
                                     const counted_iterator<I2>& y)
        -> std::enable_if_t<common_with<I2, I>, bool>
    {
        return x.count() == y.count();
    }

    friend constexpr bool operator==(const counted_iterator& x, default_sentinel_t)
    {
        return x.count() == 0;
    }

    friend constexpr bool operator==(default_sentinel_t, const counted_iterator& x)
    {
        return x.count() == 0;
    }

    template <typename I2>
    friend constexpr auto operator!=(const counted_iterator& x,
                                     const counted_iterator<I2>& y)
        -> std::enable_if_t<common_with<I2, I>, bool>
    {
        return !(x == y);
    }

    friend constexpr bool operator!=(const counted_iterator& x, default_sentinel_t y)
    {
        return !(x == y);
    }

    friend constexpr bool operator!=(default_sentinel_t x, const counted_iterator& y)
    {
        return !(x == y);
    }

    template <typename I2>
    friend constexpr auto operator<(const counted_iterator& x,
                                    const counted_iterator<I2>& y)
        -> std::enable_if_t<common_with<I2, I>, bool>
    {
        return y.count() < x.count();
    }

    template <typename I2>
    friend constexpr auto operator>(const counted_iterator& x,
                                    const counted_iterator<I2>& y)
        -> std::enable_if_t<common_with<I2, I>, bool>
    {
        return y < x;
    }

    template <typename I2>
    friend constexpr auto operator<=(const counted_iterator& x,
                                     const counted_iterator<I2>& y)
         -> std::enable_if_t<common_with<I2, I>, bool>
    {
        return !(y < x);
    }

    template <typename I2>
    friend constexpr auto operator>=(const counted_iterator& x,
                                     const counted_iterator<I2>& y)
        -> std::enable_if_t<common_with<I2, I>, bool>
    {
        return !(x < y);
    }

    template <typename I2>
    friend constexpr auto operator-(const counted_iterator& x,
                                    const counted_iterator<I2>& y)
        -> std::enable_if_t<common_with<I2, I>, iter_difference_t<I2>>
    {
        return y.count() - x.count();
    }

    friend constexpr iter_difference_t<I>
    operator-(const counted_iterator& x, default_sentinel_t)
    {
        return -x.cnt_;
    }

    friend constexpr iter_difference_t<I>
    operator-(default_sentinel_t, const counted_iterator& y)
    {
        return y.cnt_;
    }

    template <typename II = I>
    constexpr auto operator-=(difference_type n)
        -> std::enable_if_t<random_access_iterator<II>, counted_iterator&>
    {
        current_ -= n;
        cnt_ += n;
        return *this;
    }

#ifndef _MSC_VER
    // FIXME MSVC: If this is a template, MSVC can't find it via ADL for some reason
    // Making it a non-template doesn't lose much other than the InputIterator guard
    template <typename II = I, std::enable_if_t<input_iterator<II>, int> = 0>
#endif
    friend constexpr iter_rvalue_reference_t<I>
    iter_move(const counted_iterator& i) noexcept(
        noexcept(ranges::iter_move(i.current_)))
    {
        return ranges::iter_move(i.current_);
    }

    template <typename I2>
    friend constexpr auto iter_swap(
        const counted_iterator<I>& x,
        const counted_iterator<I2>&
            y) noexcept(noexcept(ranges::iter_swap(x.current_, y.current_)))
        -> std::enable_if_t<indirectly_swappable<I2, I>>
    {
        ranges::iter_swap(x.current_, y.current_);
    }

private:
    I current_{};
    iter_difference_t<I> cnt_{0};
};

}

using counted_iterator_::counted_iterator;

namespace detail {

template <typename I, typename = void>
struct counted_iterator_readable_traits_helper {
};

template <typename I>
struct counted_iterator_readable_traits_helper<I, std::enable_if_t<readable<I>>> {
    using value_type = iter_value_t<I>;
};

template <typename I, typename = void>
struct counted_iterator_category_helper {
};

template <typename I>
struct counted_iterator_category_helper<I, std::enable_if_t<input_iterator<I>>> {
    using type = iterator_category_t<I>;
};

} // namespace detail

template <typename I>
struct readable_traits<counted_iterator<I>>
        : detail::counted_iterator_readable_traits_helper<I> {
};

template <typename I>
struct iterator_category<counted_iterator<I>>
        : detail::counted_iterator_category_helper<I> {
};

template <typename I>
constexpr auto make_counted_iterator(I i, iter_difference_t<I> n)
    -> std::enable_if_t<input_or_output_iterator<I>, counted_iterator<I>>
{
    return counted_iterator<I>(std::move(i), n);
}

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

template <typename T>
std::enable_if_t<destructible<T>>
destroy_at(T* location) noexcept
{
    location->~T();
}

namespace detail {

struct destroy_fn {
private:
    template <typename I, typename S>
    static I impl(I first, S last) noexcept
    {
        for (; first != last; ++first) {
            nano::destroy_at(std::addressof(*first));
        }
        return first;
    }

public:
    template <typename I, typename S>
    std::enable_if_t<no_throw_input_iterator<I> && no_throw_sentinel<S, I> &&
        destructible<iter_value_t<I>>, I>
    operator()(I first, S last) const noexcept
    {
        return destroy_fn::impl(std::move(first), std::move(last));
    }

    template <typename Rng>
    std::enable_if_t<no_throw_input_range<Rng> &&
        destructible<iter_value_t<iterator_t<Rng>>>,
                     borrowed_iterator_t<Rng>>
    operator()(Rng&& rng) const noexcept
    {
        return destroy_fn::impl(nano::begin(rng), nano::end(rng));
    }
};

}

NANO_INLINE_VAR(detail::destroy_fn, destroy)

namespace detail {

struct destroy_n_fn {
    template <typename I>
    std::enable_if_t<
        no_throw_input_iterator<I> &&
        destructible<iter_value_t<I>>, I>
    operator()(I first, iter_difference_t<I> n) const noexcept
    {
        return nano::destroy(make_counted_iterator(std::move(first), n),
                             default_sentinel).base();
    }


};

}

NANO_INLINE_VAR(detail::destroy_n_fn, destroy_n)

NANO_END_NAMESPACE

#endif


#include <cassert>
#include <memory>
#include <new>

NANO_BEGIN_NAMESPACE

namespace detail {

template <typename T>
struct temporary_vector {
private:
    struct deleter {
        void operator()(T* ptr) const { ::operator delete[](ptr); }
    };

public:
    temporary_vector() = default;

    explicit temporary_vector(std::size_t capacity)
        : start_(static_cast<T*>(::operator new[](capacity * sizeof(T), std::nothrow))),
          end_cap_(start_ ? start_.get() + capacity : nullptr)
    {}

    temporary_vector(temporary_vector&& other) noexcept
        : start_(std::move(other.start_)),
          end_(other.end_),
          end_cap_(other.end_cap_)
    {
        other.end_ = nullptr;
        other.end_cap_ = nullptr;
    }

    temporary_vector& operator=(temporary_vector&& other) noexcept
    {
        nano::swap(start_, other.start_);
        nano::swap(end_, other.end_);
        nano::swap(end_cap_, other.end_cap_);
        return *this;
    }

    ~temporary_vector()
    {
        nano::destroy(begin(), end());
    }

    std::size_t size() const { return end_ - start_.get(); }
    std::size_t capacity() const { return end_cap_ - start_.get(); }
    [[nodiscard]] bool empty() const { return size() == 0; }

    void push_back(const T& elem) { emplace_back(elem); }
    void push_back(T&& elem) { emplace_back(std::move(elem)); }

    template <typename... Args>
    void emplace_back(Args&&... args)
    {
        assert(end_ < end_cap_);
        ::new (end_) T(std::forward<Args>(args)...);
        ++end_;
    }

    T* begin() { return start_.get(); }
    const T* begin() const { return start_.get(); }
    T* end() { return end_; }
    const T* end() const { return end_; }

    void clear()
    {
        nano::destroy(begin(), end());
        end_ = start_.get();
    }

private:
    std::unique_ptr<T, deleter> start_{nullptr};
    T* end_ = start_.get();
    T* end_cap_ = nullptr;
};

} // namespace detail

NANO_END_NAMESPACE

#endif

// nanorange/iterator/back_insert_iterator.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_BACK_INSERT_ITERATOR_HPP_INCLUDED
#define NANORANGE_ITERATOR_BACK_INSERT_ITERATOR_HPP_INCLUDED


#include <iterator>

NANO_BEGIN_NAMESPACE

template <typename Container>
struct back_insert_iterator {
    using container_type = Container;
    using difference_type = std::ptrdiff_t;

    constexpr back_insert_iterator() = default;

    explicit back_insert_iterator(Container& x) : cont_(std::addressof(x)) {}

    back_insert_iterator& operator=(const iter_value_t<iterator_t<Container>>& value)
    {
        cont_->push_back(value);
        return *this;
    }

    back_insert_iterator& operator=(iter_value_t<iterator_t<Container>>&& value)
    {
        cont_->push_back(std::move(value));
        return *this;
    }

    back_insert_iterator& operator*() { return *this; }
    back_insert_iterator& operator++() { return *this; }
    back_insert_iterator& operator++(int) { return *this; }

private:
    container_type* cont_ = nullptr;
};

template <typename Container>
back_insert_iterator<Container> back_inserter(Container& x)
{
    return back_insert_iterator<Container>(x);
}

NANO_END_NAMESPACE

namespace std {

template <typename Cont>
struct iterator_traits<::nano::back_insert_iterator<Cont>> {
    using value_type = void;
    using difference_type = ptrdiff_t;
    using reference = void;
    using pointer = void;
    using iterator_category = std::output_iterator_tag;
};

} // namespace std

#endif

// nanorange/iterator/move_iterator.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_MOVE_ITERATOR_HPP_INCLUDED
#define NANORANGE_ITERATOR_MOVE_ITERATOR_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

namespace move_iterator_ {

template <typename I>
class move_iterator {

    static_assert(
        input_iterator<I>,
        "Template argument to move_iterator must model InputIterator");

    template <typename I2>
    friend class move_iterator;

public:
    using iterator_type = I;
    using difference_type = iter_difference_t<I>;
    using value_type = iter_value_t<I>;
    using iterator_category = input_iterator_tag;
    using reference = iter_rvalue_reference_t<I>;

    constexpr move_iterator() = default;

    explicit constexpr move_iterator(I i) : current_(std::move(i)) {}

    template <typename U, std::enable_if_t<convertible_to<U, I>, int> = 0>
    constexpr move_iterator(const move_iterator<U>& i) : current_(i.current_)
    {}

    template <typename U>
    constexpr std::enable_if_t<convertible_to<U, I>, move_iterator&>
    operator=(const move_iterator<U>& i)
    {
        current_ = i.current_;
        return *this;
    }

    constexpr I base() const { return current_; }

    constexpr reference operator*() const { return iter_move(current_); }

    constexpr move_iterator& operator++()
    {
        ++current_;
        return *this;
    }

    template <typename II = I>
    constexpr auto operator++(int) -> std::enable_if_t<!forward_iterator<II>>
    {
        ++current_;
    }

    template <typename II = I>
    constexpr auto operator++(int)
        -> std::enable_if_t<forward_iterator<II>, move_iterator>
    {
        move_iterator tmp = *this;
        ++current_;
        return tmp;
    }

    template <typename II = I>
    constexpr auto operator--()
        -> std::enable_if_t<bidirectional_iterator<II>, move_iterator&>
    {
        --current_;
        return *this;
    }

    template <typename II = I>
    constexpr auto operator--(int)
        -> std::enable_if_t<bidirectional_iterator<II>, move_iterator>
    {
        move_iterator tmp = *this;
        --current_;
        return tmp;
    }

    template <typename II = I>
    constexpr auto operator+(difference_type n) const
        -> std::enable_if_t<random_access_iterator<II>, move_iterator>
    {
        return move_iterator(current_ + n);
    }

    template <typename II = I>
    constexpr auto operator+=(difference_type n)
        -> std::enable_if_t<random_access_iterator<II>, move_iterator&>
    {
        current_ += n;
        return *this;
    }

    template <typename II = I>
    constexpr auto operator-(difference_type n) const
        -> std::enable_if_t<random_access_iterator<II>, move_iterator>
    {
        return move_iterator(current_ - n);
    }

    template <typename II = I>
    constexpr auto operator-=(difference_type n)
        -> std::enable_if_t<random_access_iterator<II>, move_iterator&>
    {
        current_ -= n;
        return *this;
    }

    template <typename II = I>
    constexpr auto operator[](difference_type n) const
        -> std::enable_if_t<random_access_iterator<II>, reference>
    //   -> decltype(auto)
    {
        return iter_move(current_ + n);
    }

    friend constexpr iter_rvalue_reference_t<I>
    iter_move(const move_iterator& i) noexcept(
        noexcept(ranges::iter_move(i.current_)))
    {
        return ranges::iter_move(i.current_);
    }

    template <typename I2>
    friend constexpr auto
    iter_swap(const move_iterator& x, const move_iterator<I2>& y) noexcept(
        noexcept(ranges::iter_swap(x.current_, y.current_)))
        -> std::enable_if_t<indirectly_swappable<I2, I>>
    {
        ranges::iter_swap(x.current_, y.current_);
    }

private:
    I current_{};
};

template <typename I1, typename I2>
constexpr auto operator==(const move_iterator<I1>& x,
                          const move_iterator<I2>& y)
    -> std::enable_if_t<equality_comparable_with<I1, I2>, bool>
{
    return x.base() == y.base();
}

template <typename I1, typename I2>
constexpr auto operator!=(const move_iterator<I1>& x,
                          const move_iterator<I2>& y)
    -> std::enable_if_t<equality_comparable_with<I1, I2>, bool>
{
    return !(x == y);
}

template <typename I1, typename I2>
constexpr auto operator<(const move_iterator<I1>& x, const move_iterator<I2>& y)
    -> std::enable_if_t<totally_ordered_with<I1, I2>, bool>
{
    return x.base() < y.base();
}

template <typename I1, typename I2>
constexpr auto operator<=(const move_iterator<I1>& x,
                          const move_iterator<I2>& y)
    -> std::enable_if_t<totally_ordered_with<I1, I2>, bool>
{
    return !(y < x);
}

template <typename I1, typename I2>
constexpr auto operator>(const move_iterator<I1>& x, const move_iterator<I2>& y)
    -> std::enable_if_t<totally_ordered_with<I1, I2>, bool>
{
    return y < x;
}

template <typename I1, typename I2>
constexpr auto operator>=(const move_iterator<I1>& x,
                          const move_iterator<I2>& y)
    -> std::enable_if_t<totally_ordered_with<I1, I2>, bool>
{
    return !(x < y);
}

template <typename I1, typename I2>
constexpr auto operator-(const move_iterator<I1>& x, const move_iterator<I2>& y)
    -> std::enable_if_t<sized_sentinel_for<I1, I2>, iter_difference_t<I2>>
{
    return x.base() - y.base();
}

template <typename I>
constexpr auto operator+(iter_difference_t<I> n, const move_iterator<I>& x)
    -> std::enable_if_t<random_access_iterator<I>, move_iterator<I>>
{
    return x + n;
}

} // namespace move_iterator_

using move_iterator_::move_iterator;

template <typename I>
constexpr auto make_move_iterator(I i)
    -> std::enable_if_t<input_iterator<I>, move_iterator<I>>
{
    return move_iterator<I>(std::move(i));
}

template <typename S>
class move_sentinel {
    static_assert(semiregular<S>,
                  "Template argument to move_sentinel must model Semiregular");

public:
    constexpr move_sentinel() = default;

    constexpr explicit move_sentinel(S s) : last_(std::move(s)) {}

    template <typename U, std::enable_if_t<convertible_to<U, S>, int> = 0>
    constexpr move_sentinel(const move_sentinel<U>& s) : last_(s.base())
    {}

    template <typename U>
    constexpr auto operator=(const move_sentinel<U>& s)
        -> std::enable_if_t<convertible_to<U, S>, move_sentinel&>
    {
        last_ = s.base();
        return *this;
    }

    constexpr S base() const { return last_; }

private:
    S last_{};
};

template <typename I, typename S>
constexpr auto operator==(const move_iterator<I>& i, const move_sentinel<S>& s)
    -> std::enable_if_t<sentinel_for<S, I>, bool>
{
    return i.base() == s.base();
}

template <typename I, typename S>
constexpr auto operator==(const move_sentinel<S>& s, const move_iterator<I>& i)
    -> std::enable_if_t<sentinel_for<S, I>, bool>
{
    return i.base() == s.base();
}

template <typename I, typename S>
constexpr auto operator!=(const move_iterator<I>& i, const move_sentinel<S>& s)
    -> std::enable_if_t<sentinel_for<S, I>, bool>
{
    return !(i == s);
}

template <typename I, typename S>
constexpr auto operator!=(const move_sentinel<S>& s, const move_iterator<I>& i)
    -> std::enable_if_t<sentinel_for<S, I>, bool>
{
    return !(i == s);
}

template <typename I, typename S>
constexpr auto operator-(const move_sentinel<S>& s, const move_iterator<I>& i)
    -> std::enable_if_t<sized_sentinel_for<S, I>, iter_difference_t<I>>
{
    return s.base() - i.base();
}

template <typename I, typename S>
constexpr auto operator-(const move_iterator<I>& i, const move_sentinel<S>& s)
    -> std::enable_if_t<sized_sentinel_for<S, I>, iter_difference_t<I>>
{
    return i.base() - s.base();
}

template <typename S>
constexpr auto make_move_sentinel(S s)
    -> std::enable_if_t<semiregular<S>, move_sentinel<S>>
{
    return move_sentinel<S>(std::move(s));
}

NANO_END_NAMESPACE

namespace std {

template <typename I>
struct iterator_traits<::nano::move_iterator_::move_iterator<I>> {
    using value_type =
        typename ::nano::move_iterator_::move_iterator<I>::value_type;
    using reference =
        typename ::nano::move_iterator_::move_iterator<I>::reference;
    using pointer = value_type*;
    using difference_type =
        typename ::nano::move_iterator_::move_iterator<I>::difference_type;
    using iterator_category =
        typename ::nano::move_iterator_::move_iterator<I>::iterator_category;
};

} // namespace std

#endif


NANO_BEGIN_NAMESPACE

namespace detail {

struct inplace_merge_fn {
private:
    friend struct stable_sort_fn;

    template <typename I, typename Pred, typename Proj>
    static void impl_slow(I first, I middle, I last,
                          iter_difference_t<I> len1, iter_difference_t<I> len2,
                          Pred& pred, Proj& proj)
    {
        using dist_t = iter_difference_t<I>;

        while (true) {
            // if middle == end, we're done
            if (len2 == 0) {
                return;
            }

            // shrink [first, middle) as much as possible (with no moves),
            // returning if it shrinks to 0
            for (; true; ++first, --len1) {
                if (len1 == 0) {
                    return;
                }
                if (nano::invoke(pred, nano::invoke(proj, *middle),
                        nano::invoke(proj, *first))) {
                    break;
                }
            }

            /*if (len1 <= buf.size() || len2 <= buf.size()) {
                impl(std::move(begin), std::move(middle),
                     std::move(end), len1, len2, buf, pred, proj);
                return;
            }*/

            // first < middle < end
            // *first > *middle
            // partition [first, m1) [m1, middle) [middle, m2) [m2, end) such that
            //     all elements in:
            //         [first, m1)  <= [middle, m2)
            //         [middle, m2) <  [m1, middle)
            //         [m1, middle) <= [m2, end)
            //     and m1 or m2 is in the middle of its range
            I m1;  // "median" of [first, middle)
            I m2;  // "median" of [middle, end)
            dist_t len11;      // distance(first, m1)
            dist_t len21;      // distance(middle, m2)
            // binary search smaller range
            if (len1 < len2) {
                // len >= 1, len2 >= 2
                len21 = len2 / 2;
                m2 = nano::next(middle, len21);
                m1 = nano::upper_bound(first, middle, nano::invoke(proj, *m2),
                                       std::ref(pred), std::ref(proj));
                len11 = nano::distance(first, m1);
            } else {
                if (len1 == 1) {
                    // len1 >= len2 && len2 > 0, therefore len2 == 1
                    // It is known *first > *middle
                    nano::iter_swap(first, middle);
                    return;
                }
                // len1 >= 2, len2 >= 1
                len11 = len1 / 2;
                m1 = nano::next(first, len11);
                m2 = nano::lower_bound(middle, last, nano::invoke(proj, *m1),
                                 std::ref(pred), std::ref(proj));
                len21 = nano::distance(middle, m2);
            }
            dist_t len12 = len1 - len11;  // distance(m1, middle)
            dist_t len22 = len2 - len21;  // distance(m2, end)
            // [first, m1) [m1, middle) [middle, m2) [m2, end)
            // swap middle two partitions
            middle = nano::rotate(m1, std::move(middle), m2).begin();
            // len12 and len21 now have swapped meanings
            // merge smaller range with recursive call and larger with tail recursion elimination
            if(len11 + len21 < len12 + len22) {
                impl_slow(std::move(first), std::move(m1), middle, len11, len21,
                     pred, proj);
                first = std::move(middle);
                middle = std::move(m2);
                len1 = len12;
                len2 = len22;
            } else {
                impl_slow(middle, std::move(m2), std::move(last), len12, len22,
                     pred, proj);
                last = std::move(middle);
                middle = std::move(m1);
                len1 = len11;
                len2 = len21;
            }
        }
    }

    template <typename I, typename Buf, typename Comp, typename Proj>
    static void impl_buffered(I first, I middle, I last,
                              iter_difference_t<I> len1, iter_difference_t<I> len2,
                              Buf& buf, Comp& comp, Proj& proj)
    {
        if (len1 <= len2) {
            nano::move(first, middle, nano::back_inserter(buf));
            nano::merge(
                nano::make_move_iterator(buf.begin()),
                nano::make_move_sentinel(buf.end()),
                nano::make_move_iterator(std::move(middle)),
                nano::make_move_sentinel(std::move(last)),
                std::move(first), std::ref(comp), std::ref(proj), std::ref(proj));
        } else {
            nano::move(middle, last, nano::back_inserter(buf));
            using ri_t = nano::reverse_iterator<I>;
            // TODO: C++17's not_fn would be useful
            auto not_comp = [&comp] (auto&& a, auto&& b) {
                return !nano::invoke(comp, std::forward<decltype(a)>(a),
                        std::forward<decltype(b)>(b));
            };
            nano::merge(
                nano::make_move_iterator(ri_t{std::move(middle)}),
                nano::make_move_sentinel(ri_t{std::move(first)}),
                nano::make_move_iterator(nano::rbegin(buf)),
                nano::make_move_sentinel(nano::rend(buf)),
                nano::make_reverse_iterator(std::move(last)),
                not_comp, std::ref(proj), std::ref(proj));
        }
    }

    template <typename I, typename S, typename Comp, typename Proj>
    static I impl(I first, I middle, S last, Comp& comp, Proj& proj)
    {
        auto dist1 = nano::distance(first, middle);
        I ilast = middle;
        iter_difference_t<I> dist2 = 0;
        while (ilast != last) {
            ++ilast;
            ++dist2;
        }

        const auto sz = nano::min(dist1, dist2);
        auto buf = detail::temporary_vector<iter_value_t<I>>(sz);

        if (buf.capacity() >= static_cast<std::size_t>(sz)) {
            impl_buffered(std::move(first), std::move(middle), std::move(ilast),
                          dist1, dist2, buf, comp, proj);
        } else {
            impl_slow(std::move(first), std::move(middle), std::move(ilast),
                      dist1, dist2, comp, proj);
        }

        return ilast;
    }

public:
    template <typename I, typename S, typename Comp = ranges::less, typename Proj = identity>
    std::enable_if_t<bidirectional_iterator<I> && sentinel_for<S, I> &&
                         sortable<I, Comp, Proj>, I>
    operator()(I first, I middle, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return inplace_merge_fn::impl(std::move(first), std::move(middle),
                                      std::move(last), comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    std::enable_if_t<bidirectional_range<Rng> &&
                         sortable<iterator_t<Rng>, Comp, Proj>,
                     borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, iterator_t<Rng> middle, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return inplace_merge_fn::impl(nano::begin(rng), std::move(middle),
                                      nano::end(rng), comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::inplace_merge_fn, inplace_merge)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/is_heap.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_IS_HEAP_HPP_INCLUDED
#define NANORANGE_ALGORITHM_IS_HEAP_HPP_INCLUDED

// nanorange/algorithm/is_heap_until.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Uses code from CMCSTL2
//
// Copyright Eric Niebler 2014
// Copyright Casey Carter 2015

#ifndef NANORANGE_ALGORITHM_IS_HEAP_UNTIL_HPP_INCLUDED
#define NANORANGE_ALGORITHM_IS_HEAP_UNTIL_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct is_heap_until_fn {
private:
    friend struct is_heap_fn;

    template <typename I, typename Comp, typename Proj>
    static constexpr I impl(I first, const iter_difference_t<I> n, Comp& comp,
                            Proj& proj)
    {
        iter_difference_t<I> p = 0, c = 1;

        I pp = first;

        while (c < n) {
            I cp = first + c;

            if (nano::invoke(comp, nano::invoke(proj, *pp),
                             nano::invoke(proj, *cp))) {
                return cp;
            }

            ++c;
            ++cp;

            if (c == n || nano::invoke(comp, nano::invoke(proj, *pp),
                                       nano::invoke(proj, *cp))) {
                return cp;
            }

            ++p;
            ++pp;

            c = 2 * p + 1;
        }

        return first + n;
    }

public:
    template <typename I, typename S, typename Comp = ranges::less,
              typename Proj = identity>
    constexpr std::enable_if_t<
        random_access_iterator<I> && sentinel_for<S, I> &&
            indirect_strict_weak_order<Comp, projected<I, Proj>>,
        I>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        auto n = nano::distance(first, last);
        return is_heap_until_fn::impl(std::move(first), n, comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        random_access_range<Rng> &&
        indirect_strict_weak_order<Comp, projected<iterator_t<Rng>, Proj>>,
        borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return is_heap_until_fn::impl(nano::begin(rng), nano::distance(rng),
                                      comp, proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::is_heap_until_fn, is_heap_until)

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

namespace detail {

struct is_heap_fn {
    template <typename I, typename S, typename Comp = ranges::less,
              typename Proj = identity>
    std::enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                         indirect_strict_weak_order<Comp, projected<I, Proj>>,
                     bool>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        const auto n = nano::distance(first, last);
        return is_heap_until_fn::impl(std::move(first), n, comp, proj) == last;
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    std::enable_if_t<
        random_access_range<Rng> &&
            indirect_strict_weak_order<Comp, projected<iterator_t<Rng>, Proj>>,
        bool>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return is_heap_until_fn::impl(nano::begin(rng), nano::distance(rng),
                                      comp, proj) == nano::end(rng);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::is_heap_fn, is_heap)

NANO_END_NAMESPACE

#endif


// nanorange/algorithm/is_partitioned.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_IS_PARTITIONED_HPP_INCLUDED
#define NANORANGE_ALGORITHM_IS_PARTITIONED_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct is_partitioned_fn {
private:
    template <typename I, typename S, typename Pred, typename Proj>
    static constexpr bool impl(I first, S last, Pred& pred, Proj& proj)
    {
        first = nano::find_if_not(std::move(first), last, pred, proj);
        return nano::find_if(std::move(first), last, pred, proj) == last;
    }

public:
    template <typename I, typename S, typename Pred, typename Proj = identity>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            indirect_unary_predicate<Pred, projected<I, Proj>>, bool>
    operator()(I first, S last, Pred pred = Pred{}, Proj proj = Proj{}) const
    {
        return is_partitioned_fn::impl(std::move(first), std::move(last),
                                       pred, proj);
    }

    template <typename Rng, typename Pred, typename Proj = identity>
    constexpr std::enable_if_t<
        input_range<Rng> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>>, bool>
    operator()(Rng&& rng, Pred pred = Pred{}, Proj proj = Proj{}) const
    {
        return is_partitioned_fn::impl(nano::begin(rng), nano::end(rng),
                                       pred, proj);
    }
};

}

NANO_INLINE_VAR(detail::is_partitioned_fn, is_partitioned)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/is_permutation.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_IS_PERMUTATION_HPP_INCLUDED
#define NANORANGE_ALGORITHM_IS_PERMUTATION_HPP_INCLUDED



// nanorange/algorithm/mismatch.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_MISMATCH_HPP_INCLUDED
#define NANORANGE_ALGORITHM_MISMATCH_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

// [range.mismatch]

template <typename I1, typename I2>
using mismatch_result = in_in_result<I1, I2>;

namespace detail {

struct mismatch_fn {
private:
    friend struct is_permutation_fn;

    template <typename I1, typename S1, typename I2, typename Proj1,
              typename Proj2, typename Pred>
    static constexpr mismatch_result<I1, I2>
    impl3(I1 first1, S1 last1, I2 first2, Pred& pred, Proj1& proj1, Proj2& proj2)
    {
        while (first1 != last1 &&
               nano::invoke(pred, nano::invoke(proj1, *first1),
                            nano::invoke(proj2, *first2))) {
            ++first1;
            ++first2;
        }

        return {first1, first2};
    }

    template <typename I1, typename S1, typename I2, typename S2,
              typename Proj1, typename Proj2, typename Pred>
    static constexpr mismatch_result<I1, I2>
    impl4(I1 first1, S1 last1, I2 first2, S2 last2, Pred& pred, Proj1& proj1,
          Proj2& proj2)
    {
        while (first1 != last1 && first2 != last2 &&
               nano::invoke(pred, nano::invoke(proj1, *first1),
                            nano::invoke(proj2, *first2))) {
            ++first1;
            ++first2;
        }

        return {first1, first2};
    }

public:
    // three legged
    template <typename I1, typename S1, typename I2, typename Proj1 = identity,
              typename Proj2 = identity, typename Pred = ranges::equal_to>
    NANO_DEPRECATED constexpr std::enable_if_t<
        input_iterator<I1> && sentinel_for<S1, I1> &&
            input_iterator<std::decay_t<I2>> &&
        !input_range<I1> &&
            indirect_relation<Pred, projected<I1, Proj1>, projected<std::decay_t<I2>, Proj2>>,
        mismatch_result<I1, std::decay_t<I2>>>
    operator()(I1 first1, S1 last1, I2&& first2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return mismatch_fn::impl3(std::move(first1), std::move(last1),
                                  std::forward<I2>(first2), pred,
                                  proj1, proj2);
    }

    // range and a half
    template <typename Rng1, typename I2, typename Proj1 = identity,
              typename Proj2 = identity, typename Pred = ranges::equal_to>
    NANO_DEPRECATED constexpr std::enable_if_t<
        input_range<Rng1> && input_iterator<std::decay_t<I2>> &&
                !input_range<I2> &&
            indirect_relation<Pred, projected<iterator_t<Rng1>, Proj1>,
                             projected<std::decay_t<I2>, Proj2>>,
        mismatch_result<borrowed_iterator_t<Rng1>, std::decay_t<I2>>>
    operator()(Rng1&& rng1, I2&& first2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return mismatch_fn::impl3(nano::begin(rng1), nano::end(rng1),
                                  std::forward<I2>(first2), pred,
                                  proj1, proj2);
    }

    // four legged
    template <typename I1, typename S1, typename I2, typename S2,
              typename Proj1 = identity, typename Proj2 = identity,
              typename Pred = ranges::equal_to>
    constexpr std::enable_if_t<
        input_iterator<I1> && sentinel_for<S1, I1> && input_iterator<I2> &&
            sentinel_for<S2, I2> &&
            indirect_relation<Pred, projected<I1, Proj1>, projected<I2, Proj2>>,
        mismatch_result<I1, I2>>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return mismatch_fn::impl4(std::move(first1), std::move(last1),
                                  std::move(first2), std::move(last2),
                                  pred, proj1, proj2);
    }

    // two ranges
    template <typename Rng1, typename Rng2, typename Proj1 = identity,
              typename Proj2 = identity, typename Pred = ranges::equal_to>
    constexpr std::enable_if_t<
        input_range<Rng1> && input_range<Rng2> &&
            indirect_relation<Pred, projected<iterator_t<Rng1>, Proj1>,
                             projected<iterator_t<Rng2>, Proj2>>,
        mismatch_result<borrowed_iterator_t<Rng1>, borrowed_iterator_t<Rng2>>>
    operator()(Rng1&& rng1, Rng2&& rng2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return mismatch_fn::impl4(nano::begin(rng1), nano::end(rng1),
                                  nano::begin(rng2), nano::end(rng2),
                                  pred, proj1, proj2);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::mismatch_fn, mismatch)

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

namespace detail {

struct is_permutation_fn {
private:
    template <typename I1, typename S1, typename I2, typename S2, typename Pred,
              typename Proj1, typename Proj2>
    static constexpr bool process_tail(I1 first1, S1 last1, I2 first2, S2 last2,
                                       Pred& pred, Proj1& proj1, Proj2& proj2)
    {
        for (auto it = first1; it != last1; ++it) {
            const auto comp = [&pred, val = nano::invoke(proj1, *it)]
                    (const auto& t) { return nano::invoke(pred, t, val); };

            // Check whether we have already seen this value
            if (any_of_fn::impl(first1, it, comp, proj1)) {
                continue;
            }

            // Count how many times *it appears in range2
            const auto count1 = count_if_fn::impl(first2, last2, comp, proj2);

            // If we have a count of zero, we know the ranges are different
            if (count1 == 0) {
                return false;
            }

            // Count how many times *it appears in the remainder of range1
            // (we can start from one)
            const auto count2 = iter_difference_t<I1>{1} +
                count_if_fn::impl(nano::next(it), last1, comp, proj1);

            if (count1 != count2) {
                return false;
            }
        }

        return true;
    }

    template <typename I1, typename S1, typename I2, typename Pred,
        typename Proj1, typename Proj2>
    static constexpr bool impl3(I1 first1, S1 last1, I2 first2,
                                Pred& pred, Proj1& proj1, Proj2& proj2)
    {
        // Strip equal prefixes from both ranges
        auto result = mismatch_fn::impl3(std::move(first1), last1,
                                         std::move(first2),
                                         pred, proj1, proj2);
        first1 = std::move(result).in1;
        first2 = std::move(result).in2;

        if (first1 == last1) {
            return true;
        }

        // If we have only one value left in range1, it can't be in range2
        const auto d = nano::distance(first1, last1);
        if (d == 1) {
            return false;
        }

        auto last2 = nano::next(first2, d);

        return is_permutation_fn::process_tail(std::move(first1), std::move(last1),
                                               std::move(first2), std::move(last2),
                                               pred, proj1, proj2);
    }

    template <typename I1, typename S1, typename I2, typename S2,
              typename Pred, typename Proj1, typename Proj2>
    static constexpr bool impl4(I1 first1, S1 last1, I2 first2, S2 last2,
                                Pred& pred, Proj1& proj1, Proj2& proj2)
    {
        // Strip equal prefixes from both ranges
        auto result = mismatch_fn::impl4(std::move(first1), last1,
                                         std::move(first2), last2,
                                         pred, proj1, proj2);
        first1 = std::move(result).in1;
        first2 = std::move(result).in2;

        // If we have reached the end of both ranges, they were the same
        if (first1 == last1 && first2 == last2) {
            return true;
        }

        // If we have different numbers of elements left in the ranges,
        // they are not permutations of one another
        if (nano::distance(first1, last1) != nano::distance(first2, last2)) {
            return false;
        }

        return is_permutation_fn::process_tail(std::move(first1), std::move(last1),
                                               std::move(first2), std::move(last2),
                                               pred, proj1, proj2);
    }

public:
    // Four-legged
    template <typename I1, typename S1, typename I2, typename S2,
              typename Pred = ranges::equal_to, typename Proj1 = identity,
              typename Proj2 = identity>
    constexpr
        std::enable_if_t<
        forward_iterator<I1> && sentinel_for<S1, I1> && forward_iterator<I2> &&
            sentinel_for<S2, I2> &&
            indirectly_comparable<I1, I2, Pred, Proj1, Proj2>,
                         bool>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        if constexpr (sized_sentinel_for<S1, I1> &&
                      sized_sentinel_for<S2, I2>) {
            if (nano::distance(first1, last1) != nano::distance(first2, last2)) {
                return false;
            }
            return is_permutation_fn::impl3(std::move(first1), std::move(last1),
                                            std::move(first2), pred,
                                            proj1, proj2);
        }

        return is_permutation_fn::impl4(std::move(first1), std::move(last1),
                                        std::move(first2), std::move(last2),
                                        pred, proj1, proj2);
    }

    // Three-legged
    template <typename I1, typename S1, typename I2,
        typename Pred = ranges::equal_to, typename Proj1 = identity,
        typename Proj2 = identity>
    NANO_DEPRECATED
    constexpr
    std::enable_if_t<
        forward_iterator<I1> && sentinel_for<S1, I1> && forward_iterator<I2> &&
            indirectly_comparable<I1, I2, Pred, Proj1, Proj2>,
        bool>
    operator()(I1 first1, S1 last1, I2 first2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return is_permutation_fn::impl3(std::move(first1), std::move(last1),
                                        std::move(first2), pred,
                                        proj1, proj2);

    }

    // Two ranges
    template <typename Rng1, typename Rng2, typename Pred = ranges::equal_to,
              typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<
        forward_range<Rng1> && forward_range<Rng2> &&
            indirectly_comparable<iterator_t<Rng1>, iterator_t<Rng2>, Pred,
                                 Proj1, Proj2>,
        bool>
    operator()(Rng1&& rng1, Rng2&& rng2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        if (sized_range<Rng1> && sized_range<Rng2>) {
            if (nano::distance(rng1) != nano::distance(rng2)) {
                return false;
            }

            return is_permutation_fn::impl3(nano::begin(rng1), nano::end(rng1),
                                            nano::begin(rng2), pred,
                                            proj1, proj2);
        }

        return is_permutation_fn::impl4(nano::begin(rng1), nano::end(rng1),
                                        nano::begin(rng2), nano::end(rng2),
                                        pred, proj1, proj2);
    }

    // Range and a half
    template <typename Rng1, typename I2, typename Pred = ranges::equal_to,
        typename Proj1 = identity, typename Proj2 = identity>
    NANO_DEPRECATED
    constexpr std::enable_if_t<
        forward_range<Rng1> && forward_iterator<std::decay_t<I2>> &&
        !range<I2> &&
            indirectly_comparable<iterator_t<Rng1>, I2, Pred, Proj1, Proj2>,
        bool>
    operator()(Rng1&& rng1, I2&& first2, Pred pred = Pred{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const {
        return is_permutation_fn::impl3(nano::begin(rng1), nano::end(rng1),
                                        std::forward<I2>(first2), pred,
                                        proj1, proj2);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::is_permutation_fn, is_permutation)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/is_sorted.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_IS_SORTED_HPP_INCLUDED
#define NANORANGE_ALGORITHM_IS_SORTED_HPP_INCLUDED

// nanorange/algorithm/is_sorted_until.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_IS_SORTED_UNTIL_HPP_INCLUDED
#define NANORANGE_ALGORITHM_IS_SORTED_UNTIL_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct is_sorted_until_fn {
private:
    friend struct is_sorted_fn;

    template <typename I, typename S, typename Comp, typename Proj>
    static constexpr I impl(I first, S last, Comp& comp, Proj& proj)
    {
        if (first == last) {
            return first;
        }

        I n = next(first);

        while (n != last) {
            if (nano::invoke(comp, nano::invoke(proj, *n),
                              nano::invoke(proj, *first))) {
                return n;
            }
            ++first;
            ++n;
        }

        return n;
    }

public:
    template <typename I, typename S, typename Comp = ranges::less,
              typename Proj = identity>
    constexpr std::enable_if_t<
        forward_iterator<I> && sentinel_for<S, I> &&
            indirect_strict_weak_order<Comp, projected<I, Proj>>, I>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return is_sorted_until_fn::impl(std::move(first), std::move(last),
                                        comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        forward_range<Rng> &&
            indirect_strict_weak_order<Comp, projected<iterator_t<Rng>, Proj>>,
        borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return is_sorted_until_fn::impl(nano::begin(rng), nano::end(rng),
                                        comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::is_sorted_until_fn, is_sorted_until)

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

namespace detail {

struct is_sorted_fn {
    template <typename I, typename S, typename Comp = ranges::less,
            typename Proj = identity>
    constexpr std::enable_if_t<
        forward_iterator<I> && sentinel_for<S, I> &&
            indirect_strict_weak_order<Comp, projected<I, Proj>>, bool>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return is_sorted_until_fn::impl(std::move(first), last,
                                        comp, proj) == last;
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        forward_range<Rng> &&
            indirect_strict_weak_order<Comp, projected<iterator_t<Rng>, Proj>>,
        bool>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return is_sorted_until_fn::impl(nano::begin(rng), nano::end(rng),
                                        comp, proj) == nano::end(rng);
    }
};

}

NANO_INLINE_VAR(detail::is_sorted_fn, is_sorted)

NANO_END_NAMESPACE

#endif


// nanorange/algorithm/stl/lexicographical_compare.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_LEXICOGRAPHICAL_COMPARE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_LEXICOGRAPHICAL_COMPARE_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct lexicographical_compare_fn {
private:
    template <typename I1, typename S1, typename I2, typename S2,
              typename Comp, typename Proj1, typename Proj2>
    static constexpr bool impl(I1 first1, S1 last1, I2 first2, S2 last2,
                                Comp& comp, Proj1& proj1, Proj2& proj2)
    {
        while (first1 != last1 && first2 != last2) {
            if (nano::invoke(comp, nano::invoke(proj1, *first1),
                              nano::invoke(proj2, *first2))) {
                return true;
            }
            if (nano::invoke(comp, nano::invoke(proj2, *first2),
                             nano::invoke(proj1, *first1))) {
                return false;
            }
            ++first1; ++first2;
        }

        return first1 == last1 && first2 != last2;
    }

public:
    template <typename I1, typename S1, typename I2, typename S2,
              typename Comp = ranges::less, typename Proj1 = identity,
              typename Proj2 = identity>
    constexpr std::enable_if_t<
        input_iterator<I1> && sentinel_for<S1, I1> && input_iterator<I2> &&
            sentinel_for<S2, I2> &&
            indirect_strict_weak_order<Comp, projected<I1, Proj1>, projected<I2, Proj2>>,
        bool>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, Comp comp = Comp{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return lexicographical_compare_fn::impl(
                std::move(first1), std::move(last1),
                std::move(first2), std::move(last2),
                comp, proj1, proj2);
    }

    template <typename Rng1, typename Rng2, typename Comp = ranges::less,
              typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<
        input_range<Rng1> && input_range<Rng2> &&
            indirect_strict_weak_order<Comp, projected<iterator_t<Rng1>, Proj1>,
                                      projected<iterator_t<Rng2>, Proj2>>,
            bool>
    operator()(Rng1&& rng1, Rng2&& rng2, Comp comp = Comp{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return lexicographical_compare_fn::impl(
                nano::begin(rng1), nano::end(rng1),
                nano::begin(rng2), nano::end(rng2),
                comp, proj1, proj2);
    }
};

}

NANO_INLINE_VAR(detail::lexicographical_compare_fn, lexicographical_compare)

NANO_END_NAMESPACE

#endif


// nanorange/algorithm/make_heap.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Uses code from CMCSTL2
//  Copyright Eric Niebler 2014
//  Copyright Casey Carter 2015

#ifndef NANORANGE_ALGORITHM_MAKE_HEAP_HPP_INCLUDED
#define NANORANGE_ALGORITHM_MAKE_HEAP_HPP_INCLUDED

// nanorange/detail/algorithm/heap_sift.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// cmcstl2 - A concept-enabled C++ standard library
//
//  Copyright Eric Niebler 2014
//  Copyright Casey Carter 2015
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/caseycarter/cmcstl2
//
//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
#ifndef NANORANGE_DETAIL_ALGORITHM_HEAP_SIFT_HPP
#define NANORANGE_DETAIL_ALGORITHM_HEAP_SIFT_HPP





///////////////////////////////////////////////////////////////////////////
// detail::sift_up_n and detail::sift_down_n
// (heap implementation details)
//

NANO_BEGIN_NAMESPACE

namespace detail {

template <typename I, typename Comp, typename Proj>
constexpr void sift_up_n(I first, iter_difference_t<I> n, Comp& comp,
                         Proj& proj)
{
    if (n > 1) {
        I last = first + n;
        n = (n - 2) / 2;
        I i = first + n;
        if (nano::invoke(comp, nano::invoke(proj, *i),
                         nano::invoke(proj, *--last))) {
            iter_value_t<I> v = nano::iter_move(last);
            do {
                *last = nano::iter_move(i);
                last = i;
                if (n == 0) {
                    break;
                }
                n = (n - 1) / 2;
                i = first + n;
            } while (nano::invoke(comp, nano::invoke(proj, *i),
                                  nano::invoke(proj, v)));
            *last = std::move(v);
        }
    }
}

template <typename I, typename Comp, typename Proj>
constexpr void sift_down_n(I first, iter_difference_t<I> n, I start, Comp& comp,
                           Proj& proj)
{
    // left-child of start is at 2 * start + 1
    // right-child of start is at 2 * start + 2
    auto child = start - first;

    if (n < 2 || (n - 2) / 2 < child) {
        return;
    }

    child = 2 * child + 1;
    I child_i = first + child;

    if ((child + 1) < n && nano::invoke(comp, nano::invoke(proj, *child_i),
                                        nano::invoke(proj, *(child_i + 1)))) {
        // right-child exists and is greater than left-child
        ++child_i;
        ++child;
    }

    // check if we are in heap-order
    if (nano::invoke(comp, nano::invoke(proj, *child_i),
                     nano::invoke(proj, *start))) {
        // we are, start is larger than its largest child
        return;
    }

    iter_value_t<I> top = nano::iter_move(start);
    do {
        // we are not in heap-order, swap the parent with it's largest child
        *start = nano::iter_move(child_i);
        start = child_i;

        if ((n - 2) / 2 < child) {
            break;
        }

        // recompute the child based off of the updated parent
        child = 2 * child + 1;
        child_i = first + child;

        if ((child + 1) < n &&
            nano::invoke(comp, nano::invoke(proj, *child_i),
                         nano::invoke(proj, *(child_i + 1)))) {
            // right-child exists and is greater than left-child
            ++child_i;
            ++child;
        }

        // check if we are in heap-order
    } while (!nano::invoke(comp, nano::invoke(proj, *child_i),
                           nano::invoke(proj, top)));
    *start = std::move(top);
}

} // namespace detail

NANO_END_NAMESPACE

#endif



NANO_BEGIN_NAMESPACE

namespace detail {

struct make_heap_fn {
private:
    template <typename I, typename Comp, typename Proj>
    static constexpr I impl(I first, iter_difference_t<I> n, Comp& comp,
                            Proj& proj)
    {
        if (n > 1) {
            // start from the first parent, there is no need to consider
            // children
            for (auto start = (n - 2) / 2; start >= 0; --start) {
                detail::sift_down_n(first, n, first + start, comp, proj);
            }
        }

        return first + n;
    }

public:
    template <typename I, typename S, typename Comp = ranges::less,
              typename Proj = identity>
    constexpr std::enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                                   sortable<I, Comp, Proj>, I>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        const auto n = nano::distance(first, last);
        return make_heap_fn::impl(std::move(first), n, comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<random_access_range<Rng> &&
                                   sortable<iterator_t<Rng>, Comp>,
                               borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return make_heap_fn::impl(nano::begin(rng), nano::distance(rng), comp,
                                  proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::make_heap_fn, make_heap)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/max.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_MAX_HPP_INCLUDED
#define NANORANGE_ALGORITHM_MAX_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct max_fn {
private:
    template <typename Rng, typename Comp, typename Proj>
    static constexpr iter_value_t<iterator_t<Rng>>
    impl(Rng&& rng, Comp& comp, Proj& proj)
    {
        auto first = nano::begin(rng);
        const auto last = nano::end(rng);

        // Empty ranges not allowed
        auto result = *first;

        while(++first != last) {
            auto&& val = *first;
            if (nano::invoke(comp, nano::invoke(proj, result),
                              nano::invoke(proj, val))) {
                result = std::forward<decltype(val)>(val);
            }
        }

        return result;
    }

public:
    template <typename T, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        indirect_strict_weak_order<Comp, projected<const T*, Proj>>,
    const T&>
    operator()(const T& a, const T& b, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        // *sigh*, this should be fixed in STL2
        return !nano::invoke(comp, nano::invoke(proj, a),
                            nano::invoke(proj, b)) ? a : b;
    }

    template <typename T, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        copyable<T> &&
            indirect_strict_weak_order<Comp, projected<const T*, Proj>>,
            T>
    operator()(std::initializer_list<T> rng, Comp comp = Comp{},
               Proj proj = Proj{}) const
    {
        return max_fn::impl(rng, comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        input_range<Rng> && copyable<iter_value_t<iterator_t<Rng>>> &&
            indirect_strict_weak_order<Comp, projected<iterator_t<Rng>, Proj>>,
    range_value_t<Rng>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return max_fn::impl(std::forward<Rng>(rng), comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::max_fn, max)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/max_element.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_MAX_ELEMENT_HPP_INCLUDED
#define NANORANGE_ALGORITHM_MAX_ELEMENT_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct max_element_fn {
    template <typename I, typename S, typename Comp, typename Proj>
    static constexpr I impl(I first, S last, Comp& comp, Proj& proj)
    {
        if (first == last) {
            return first;
        }

        I i = nano::next(first);
        while (i != last) {
            if (!nano::invoke(comp, nano::invoke(proj, *i),
                              nano::invoke(proj, *first))) {
                first = i;
            }
            ++i;
        }

        return first;
    }

public:
    template <typename I, typename S, typename Comp = ranges::less,
            typename Proj = identity>
    constexpr std::enable_if_t<
        forward_iterator<I> && sentinel_for<S, I> &&
            indirect_strict_weak_order<Comp, projected<I, Proj>>, I>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return max_element_fn::impl(std::move(first), std::move(last),
                                    comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        forward_range<Rng> &&
            indirect_strict_weak_order<Comp, projected<iterator_t<Rng>, Proj>>,
        borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return max_element_fn::impl(nano::begin(rng), nano::end(rng),
                                    comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::max_element_fn, max_element)

NANO_END_NAMESPACE

#endif



// nanorange/algorithm/min_element.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_MIN_ELEMENT_HPP_INCLUDED
#define NANORANGE_ALGORITHM_MIN_ELEMENT_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct min_element_fn {
private:
    friend struct nth_element_fn;

    template <typename I, typename S, typename Comp, typename Proj>
    static constexpr I impl(I first, S last, Comp& comp, Proj& proj)
    {
        if (first == last) {
            return first;
        }

        I i = nano::next(first);
        while (i != last) {
            if (nano::invoke(comp, nano::invoke(proj, *i),
                             nano::invoke(proj, *first))) {
                first = i;
            }
            ++i;
        }

        return first;
    }

public:
    template <typename I, typename S, typename Comp = ranges::less,
              typename Proj = identity>
    constexpr std::enable_if_t<
        forward_iterator<I> && sentinel_for<S, I> &&
            indirect_strict_weak_order<Comp, projected<I, Proj>>, I>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return min_element_fn::impl(std::move(first), std::move(last),
                                    comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        forward_range<Rng> &&
            indirect_strict_weak_order<Comp, projected<iterator_t<Rng>, Proj>>,
        borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return min_element_fn::impl(nano::begin(rng), nano::end(rng),
                                    comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::min_element_fn, min_element)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/minmax.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Uses code from CMCSTL2
// Copyright Casey Carter 2015

#ifndef NANORANGE_ALGORITHM_MINMAX_HPP_INCLUDED
#define NANORANGE_ALGORITHM_MINMAX_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename T>
using minmax_result = min_max_result<T>;

namespace detail {

struct minmax_fn {
private:
    template <typename Rng, typename Comp, typename Proj,
              typename T = iter_value_t<iterator_t<Rng>>>
    static constexpr minmax_result<T>
    impl(Rng&& rng, Comp& comp, Proj& proj)
    {
        auto first = nano::begin(rng);
        const auto last = nano::end(rng);

        // Empty ranges not allowed
        auto temp = *first;
        minmax_result<T> result{temp, std::move(temp)};

        if (++first != last) {
            {
                auto&& val = *first;
                if (nano::invoke(comp, nano::invoke(proj, val),
                                 nano::invoke(proj, result.min))) {
                    result.min = std::forward<decltype(val)>(val);
                } else if (!nano::invoke(comp, nano::invoke(proj, val),
                                         nano::invoke(proj, result.max))){
                    result.max = std::forward<decltype(val)>(val);
                }
            }

            while (++first != last) {
                T val1 = *first;

                // Last iteration
                if (++first == last) {
                    if (nano::invoke(comp, nano::invoke(proj, val1),
                                     nano::invoke(proj, result.min))) {
                        result.min = std::move(val1);
                    }
                    else if (!nano::invoke(comp, nano::invoke(proj, val1),
                                           nano::invoke(proj, result.max))){
                        result.max = std::move(val1);
                    }
                    break;
                }

                auto&& val2 = *first;
                if (nano::invoke(comp, nano::invoke(proj, val2),
                                 nano::invoke(proj, val1))) {
                    if (nano::invoke(comp, nano::invoke(proj, val2),
                                     nano::invoke(proj, result.min))) {
                        result.min = std::forward<decltype(val2)>(val2);
                    }
                    if (!nano::invoke(comp, nano::invoke(proj, val1),
                                      nano::invoke(proj, result.max))) {
                        result.max = std::move(val1);
                    }
                } else {
                    if (nano::invoke(comp, nano::invoke(proj, val1),
                                     nano::invoke(proj, result.min))) {
                        result.min = std::move(val1);
                    }
                    if (!nano::invoke(comp, nano::invoke(proj, val2),
                                      nano::invoke(proj, result.max))) {
                        result.max = std::forward<decltype(val2)>(val2);
                    }
                }
            }
        }

        return result;
    }

public:
    template <typename T, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        indirect_strict_weak_order<Comp, projected<const T*, Proj>>,
        minmax_result<const T&>>
    operator()(const T& a, const T& b, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        if (nano::invoke(comp, nano::invoke(proj, b), nano::invoke(proj, a))) {
            return {b, a};
        } else {
            return {a, b};
        }
    }

    template <typename T, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        copyable<T> &&
            indirect_strict_weak_order<Comp, projected<const T*, Proj>>,
        minmax_result<T>>
    operator()(std::initializer_list<T> rng, Comp comp = Comp{},
               Proj proj = Proj{}) const
    {
        return minmax_fn::impl(rng, comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        input_range<Rng> && copyable<iter_value_t<iterator_t<Rng>>> &&
            indirect_strict_weak_order<Comp, projected<iterator_t<Rng>, Proj>>,
        minmax_result<range_value_t<Rng>>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return minmax_fn::impl(std::forward<Rng>(rng), comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::minmax_fn, minmax)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/minmax_element.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Uses code from CMCSTL2
// Copyright Casey Carter 2015

#ifndef NANORANGE_ALGORITHM_MINMAX_ELEMENT_HPP_INCLUDED
#define NANORANGE_ALGORITHM_MINMAX_ELEMENT_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename T>
using minmax_element_result = min_max_result<T>;

namespace detail {

struct minmax_element_fn {
private:
    template <typename I, typename S, typename Comp, typename Proj>
    static constexpr minmax_element_result<I> impl(I first, S last, Comp& comp, Proj& proj)
    {
        minmax_element_result<I> result{first, first};

        if (first == last || ++first == last) {
            return result;
        }

       if (nano::invoke(comp, nano::invoke(proj, *first),
                        nano::invoke(proj, *result.min))) {
           result.min = first;
       } else if (!nano::invoke(comp, nano::invoke(proj, *first),
                                nano::invoke(proj, *result.max))){
           result.max = first;
       }

       while (++first != last) {
           I it = first;

           // Last iteration
           if (++first == last) {
               if (nano::invoke(comp, nano::invoke(proj, *it),
                                nano::invoke(proj, *result.min))) {
                   result.min = std::move(it);
               }
               else if (!nano::invoke(comp, nano::invoke(proj, *it),
                                      nano::invoke(proj, *result.max))) {
                   result.max = std::move(it);
               }
               break;
           }

           if (nano::invoke(comp, nano::invoke(proj, *first),
                            nano::invoke(proj, *it))) {
               if (nano::invoke(comp, nano::invoke(proj, *first),
                                nano::invoke(proj, *result.min))) {
                   result.min = first;
               }
               if (!nano::invoke(comp, nano::invoke(proj, *it),
                                 nano::invoke(proj, *result.max))) {
                   result.max = it;
               }
           }
           else {
               if (nano::invoke(comp, nano::invoke(proj, *it),
                                nano::invoke(proj, *result.min))) {
                   result.min = it;
               }
               if (!nano::invoke(comp, nano::invoke(proj, *first),
                                 nano::invoke(proj, *result.max))) {
                   result.max = first;
               }
           }
       }

        return result;
    }


public:
    template <typename I, typename S, typename Comp = ranges::less,
            typename Proj = identity>
    constexpr std::enable_if_t<
        forward_iterator<I> && sentinel_for<S, I> &&
            indirect_strict_weak_order<Comp, projected<I, Proj>>,
        minmax_element_result<I>>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return minmax_element_fn::impl(std::move(first), std::move(last),
                                    comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        forward_range<Rng> &&
            indirect_strict_weak_order<Comp, projected<iterator_t<Rng>, Proj>>,
        minmax_element_result<borrowed_iterator_t<Rng>>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return minmax_element_fn::impl(nano::begin(rng), nano::end(rng),
                                       comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::minmax_element_fn, minmax_element)

NANO_END_NAMESPACE

#endif



// nanorange/algorithm/next_permutation.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Taken from Range-V3
//
// Copyright Eric Niebler 2014-2018
//
//===-------------------------- algorithm ---------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef NANORANGE_ALGORITHM_NEXT_PERMUTATION_HPP_INCLUDED
#define NANORANGE_ALGORITHM_NEXT_PERMUTATION_HPP_INCLUDED

// nanorange/algorithm/reverse.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_REVERSE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_REVERSE_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct reverse_fn {
private:
    template <typename I>
    static constexpr I impl(I first, I last)
    {
        I ret = last;
        while (first != last && first !=  --last) {
            nano::iter_swap(first, last);
            ++first;
        }

        return ret;
    }

    template <typename I, typename S>
    static constexpr std::enable_if_t<
        !same_as<I, S>, I>
    impl(I first, S bound)
    {
        I last = next(first, bound);
        return reverse_fn::impl(std::move(first), std::move(last));
    }

public:
    template <typename I, typename S>
    constexpr std::enable_if_t<bidirectional_iterator<I> && sentinel_for<S, I>,
        I>
    operator()(I first, S last) const
    {
        return reverse_fn::impl(std::move(first), std::move(last));
    }

    template <typename Rng>
    constexpr std::enable_if_t<bidirectional_range<Rng>,
                               borrowed_iterator_t<Rng>>
    operator()(Rng&& rng) const
    {
        return reverse_fn::impl(nano::begin(rng), nano::end(rng));
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::reverse_fn, reverse)

NANO_END_NAMESPACE

#endif



NANO_BEGIN_NAMESPACE

template <typename I>
using next_permutation_result = in_found_result<I>;

namespace detail {

struct next_permutation_fn {
private:
    template <typename I, typename S, typename Comp, typename Proj>
    static constexpr next_permutation_result<I>
    impl(I first, S last, Comp& comp, Proj& proj)
    {
        if (first == last) {
            return {std::move(first), false};
        }

        I last_it = nano::next(first, last);
        I i = last_it;

        if (first == --i) {
            return {std::move(last_it), false};
        }

        while (true) {
            I ip1 = i;

            if (nano::invoke(comp, nano::invoke(proj, *--i),
                             nano::invoke(proj, *ip1))) {
                I j = last_it;
                while (!nano::invoke(comp, nano::invoke(proj, *i),
                                     nano::invoke(proj, *--j)));

                nano::iter_swap(i, j);
                nano::reverse(ip1, last_it);
                return {std::move(last_it), true};
            }

            if (i == first) {
                nano::reverse(first, last_it);
                return {std::move(last_it), false};
            }
        }
    }

public:
    template <typename I, typename S, typename Comp = ranges::less,
              typename Proj = identity>
    constexpr std::enable_if_t<bidirectional_iterator<I> && sentinel_for<S, I> &&
                                   sortable<I, Comp, Proj>,
        next_permutation_result<I>>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return next_permutation_fn::impl(std::move(first), std::move(last),
                                         comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        bidirectional_range<Rng> && sortable<iterator_t<Rng>, Comp, Proj>,
        next_permutation_result<borrowed_iterator_t<Rng>>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return next_permutation_fn::impl(nano::begin(rng), nano::end(rng),
                                         comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::next_permutation_fn, next_permutation)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/none_of.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_NONE_OF_HPP_INCLUDED
#define NANORANGE_ALGORITHM_NONE_OF_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

// [ranges.alg.none_of]

namespace detail {

struct none_of_fn {

    template <typename I, typename S, typename Proj = identity, typename Pred>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            indirect_unary_predicate<Pred, projected<I, Proj>>,
        bool>
    operator()(I first, S last, Pred pred, Proj proj = Proj{}) const
    {
        return !any_of_fn::impl(first, last, pred, proj);
    }

    template <typename Rng, typename Proj = identity, typename Pred>
    constexpr std::enable_if_t<
        input_range<Rng> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>>,
        bool>
    operator()(Rng&& rng, Pred pred, Proj proj = Proj{}) const
    {
        return !any_of_fn::impl(nano::begin(rng), nano::end(rng),
                                pred, proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::none_of_fn, none_of)

NANO_END_NAMESPACE

#endif
// nanorange/algorithm/nth_element.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Uses code from cmcstl2 - A concept-enabled C++ standard library
//
//  Copyright Eric Niebler 2014
//  Copyright Casey Carter 2015
//

//
//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef NANORANGE_ALGORITHM_NTH_ELEMENT_HPP_INCLUDED
#define NANORANGE_ALGORITHM_NTH_ELEMENT_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct nth_element_fn {
private:
    template <typename I, typename Comp, typename Proj>
    static constexpr void impl(I first, I nth, I last, Comp& comp, Proj& proj)
    {
        constexpr iter_difference_t<I> limit = 7;

        const auto pred = [&comp, &proj] (auto&& t, auto&& u) {
            return nano::invoke(comp, nano::invoke(proj, std::forward<decltype(t)>(t)),
                                nano::invoke(proj, std::forward<decltype(u)>(u)));
        };

        I end = last;

        while (true) {
            if (nth == end) return;

            iter_difference_t<I> len = end - first;

            switch (len) {
            case 0:
            case 1:
                return;
            case 2:
                if (pred(*--end, *first)) iter_swap(first, end);
                return;
            case 3:
            {
                I m = first;
                sort3(first, ++m, --end, comp, proj);
                return;
            }
            default: break;
            }
            if (len <= limit) {
                selection_sort(first, end, comp, proj);
                return;
            }
            // Post: len > limit

            I m = first + len / 2;
            I lm1 = end;
            unsigned n_swaps = sort3(first, m, --lm1, comp, proj);
            // Post: *m is median

            // partition [first, m) < *m and *m <= [m, end)
            //(this inhibits tossing elements equivalent to m around unnecessarily)
            I i = first;
            I j = lm1;
            // j points beyond range to be tested, *lm1 is known to be <= *m
            // The search going up is known to be guarded but the search coming down isn't.
            // Prime the downward search with a guard.
            if (!pred(*i, *m)) {  // if *first == *m
                // *first == *m, *first doesn't go in first part
                // manually guard downward moving j against i
                bool restart = false;
                while (true) {
                    if (i == --j) {
                        // *first == *m, *m <= all other elements
                        // Parition instead into [first, i) == *first and *first < [i, end)
                        ++i;  // first + 1
                        j = end;
                        if (!pred(*first, *--j)) {  // we need a guard if *first == *(end-1)
                            while (true) {
                                if (i == j) {
                                    return;  // [first, end) all equivalent elements
                                }
                                if (pred(*first, *i)) {
                                    iter_swap(i, j);
                                    ++n_swaps;
                                    ++i;
                                    break;
                                }
                                ++i;
                            }
                        }
                        // [first, i) == *first and *first < [j, end) and j == end - 1
                        if (i == j) return;

                        while (true) {
                            while (!pred(*first, *i)) { ++i; }
                            while (pred(*first, *--j)) {}
                            if (i >= j) break;
                            iter_swap(i, j);
                            ++n_swaps;
                            ++i;
                        }
                        // [first, i) == *first and *first < [i, end)
                        // The first part is sorted,
                        if (nth < i) return;

                        // nth_element the second part
                        // nth_element<C>(i, nth, end, comp);
                        restart = true;
                        break;
                    }
                    if (pred(*j, *m)) {
                        iter_swap(i, j);
                        ++n_swaps;
                        break;  // found guard for downward moving j, now use unguarded partition
                    }
                }
                if (restart) {
                    first = i;
                    continue;
                }
            }
            ++i;
            // j points beyond range to be tested, *lm1 is known to be <= *m
            // if not yet partitioned...
            if (i < j) {
                // known that *(i - 1) < *m
                while (true) {
                    // m still guards upward moving i
                    while (pred(*i, *m)) { ++i; }
                    // It is now known that a guard exists for downward moving j
                    while (!pred(*--j, *m)) {}
                    if (i >= j) break;
                    iter_swap(i, j);
                    ++n_swaps;
                    // It is known that m != j
                    // If m just moved, follow it
                    if (m == i) m = j;
                    ++i;
                }
            }
            // [first, i) < *m and *m <= [i, end)
            if (i != m && pred(*m, *i)) {
                iter_swap(i, m);
                ++n_swaps;
            }
            // [first, i) < *i and *i <= [i+1, end)
            if (nth == i) return;

            if (n_swaps == 0) {
                // We were given a perfectly partitioned sequence.  Coincidence?
                if (nth < i) {
                    // Check for [first, i) already sorted
                    j = m = first;
                    while (true) {
                        if (++j == i) {
                            // [first, i) sorted
                            return;
                        }
                        if (pred(*j, *m)) {
                            // not yet sorted, so sort
                            break;
                        }
                        m = j;
                    }
                } else {
                    // Check for [i, end) already sorted
                    j = m = i;
                    while (true) {
                        if (++j == end) {
                            // [i, end) sorted
                            return;
                        }
                        if (pred(*j, *m)) {
                            // not yet sorted, so sort
                            break;
                        }
                        m = j;
                    }
                }
            }

            // nth_element on range containing nth
            if (nth < i) {
                // nth_element<C>(first, nth, i, comp);
                end = i;
            } else {
                // nth_element<C>(i+1, nth, end, comp);
                first = ++i;
            }
        }
    }

    template <typename I, typename Comp, typename Proj>
    static constexpr unsigned sort3(I x, I y, I z, Comp& comp, Proj& proj)
    {
        const auto pred = [&comp, &proj] (auto&& t, auto&& u) {
            return nano::invoke(comp, nano::invoke(proj, std::forward<decltype(t)>(t)),
                                nano::invoke(proj, std::forward<decltype(u)>(u)));
        };

        if (!pred(*y, *x)) {      // if x <= y
            if (!pred(*z, *y)) {  // if y <= z
                return 0;         // x <= y && y <= z
            }
            // x <= y && y > z
            iter_swap(y, z);      // x <= z && y < z
            if (pred(*y, *x)) {   // if x > y
                iter_swap(x, y);  // x < y && y <= z
                return 2;
            }
            return 1;             // x <= y && y < z
        }
        if (pred(*z, *y)) {       // x > y, if y > z
            iter_swap(x, z);      // x < y && y < z
            return 1;
        }
        iter_swap(x, y);          // x > y && y <= z
        // x < y && x <= z
        if (pred(*z, *y)) {       // if y > z
            iter_swap(y, z);      // x <= y && y < z
            return 2;
        }
        return 1;
    }

    template <typename I, typename Comp, typename Proj>
    static constexpr void selection_sort(I first, I last, Comp& comp, Proj& proj)
    {
        for (I lm1 = prev(last); first != lm1; ++first) {
            I i = min_element_fn::impl(first, last, comp, proj);
            if (i != first) {
                iter_swap(first, i);
            }
        }
    }

public:
    template <typename I, typename S, typename Comp = ranges::less, typename Proj = identity>
    std::enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                         sortable<I, Comp, Proj>, I>
    constexpr operator()(I first, I nth, S last,
                         Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        const I ilast = nano::next(nth, last);
        impl(std::move(first), nth, std::move(ilast), comp, proj);
        return ilast;
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    std::enable_if_t<random_access_range<Rng> &&
                         sortable<iterator_t<Rng>, Comp, Proj>,
                     borrowed_iterator_t<Rng>>
    constexpr operator()(Rng&& rng, iterator_t<Rng> nth,
                         Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        const auto last = nano::next(nth, nano::end(rng));
        impl(nano::begin(rng), std::move(nth), last, comp, proj);
        return last;
    }
};

}

NANO_INLINE_VAR(detail::nth_element_fn, nth_element)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/partial_sort.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Copyright Eric Niebler 2014
// Copyright Casey Carter 2015
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)



#ifndef NANORANGE_ALGORITHM_PARTIAL_SORT_HPP_INCLUDED
#define NANORANGE_ALGORITHM_PARTIAL_SORT_HPP_INCLUDED


// nanorange/algorithm/sort_heap.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_SORT_HEAP_HPP_INCLUDED
#define NANORANGE_ALGORITHM_SORT_HEAP_HPP_INCLUDED

// nanorange/algorithm/pop_heap.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_POP_HEAP_HPP_INCLUDED
#define NANORANGE_ALGORITHM_POP_HEAP_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

namespace detail {

struct pop_heap_fn {
private:
    friend struct sort_heap_fn;

    template <typename I, typename Comp, typename Proj>
    static constexpr I impl(I first, iter_difference_t<I> n, Comp& comp,
                            Proj& proj)
    {
        if (n > 1) {
            nano::iter_swap(first, first + (n - 1));
            detail::sift_down_n(first, n - 1, first, comp, proj);
        }

        return first + n;
    }

public:
    template <typename I, typename S, typename Comp = ranges::less,
              typename Proj = identity>
    constexpr std::enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                                   sortable<I, Comp, Proj>, I>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        const auto n = nano::distance(first, last);
        return pop_heap_fn::impl(std::move(first), n, comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<random_access_range<Rng> &&
                                   sortable<iterator_t<Rng>, Comp, Proj>,
                               borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return pop_heap_fn::impl(nano::begin(rng), nano::distance(rng), comp,
                                 proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::pop_heap_fn, pop_heap)

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

namespace detail {

struct sort_heap_fn {
private:
    template <typename I, typename Comp, typename Proj>
    static constexpr I impl(I first, iter_difference_t<I> n, Comp& comp,
                            Proj& proj)
    {
        if (n < 2) {
            return first + n;
        }

        for (auto i = n; i > 1; --i) {
            pop_heap_fn::impl(first, i, comp, proj);
        }

        return first + n;
    }

public:
    template <typename I, typename S, typename Comp = ranges::less,
              typename Proj = identity>
    constexpr std::enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                                   sortable<I, Comp, Proj>, I>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        const auto n = nano::distance(first, last);
        return sort_heap_fn::impl(std::move(first), n, comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<random_access_range<Rng> &&
                                   sortable<iterator_t<Rng>, Comp, Proj>,
                               borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return sort_heap_fn::impl(nano::begin(rng), nano::distance(rng), comp,
                                  proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::sort_heap_fn, sort_heap)

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

namespace detail {

struct partial_sort_fn {
private:
    template <typename I, typename S, typename Comp, typename Proj>
    static constexpr I impl(I first, I middle, S last, Comp& comp, Proj& proj)
    {
        nano::make_heap(first, middle, comp, proj);
        const auto len = nano::distance(first, middle);
        I i = middle;

        while (i != last) {
            if (nano::invoke(comp, nano::invoke(proj, *i), nano::invoke(proj, *first))) {
                nano::iter_swap(i, first);
                detail::sift_down_n(first, len, first, comp, proj);
            }
            ++i;
        }
        nano::sort_heap(first, middle, comp, proj);
        return i;
    }

public:
    template <typename I, typename S, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                                   sortable<I, Comp, Proj>, I>
    operator()(I first, I middle, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return partial_sort_fn::impl(std::move(first), std::move(middle),
                                     std::move(last), comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<random_access_range<Rng> &&
                                   sortable<iterator_t<Rng>, Comp, Proj>,
                               borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, iterator_t<Rng> middle, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return partial_sort_fn::impl(nano::begin(rng), std::move(middle),
                                     nano::end(rng), comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::partial_sort_fn, partial_sort)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/partial_sort_copy.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_PARTIAL_SORT_COPY_HPP_INCLUDED
#define NANORANGE_ALGORITHM_PARTIAL_SORT_COPY_HPP_INCLUDED





NANO_BEGIN_NAMESPACE

template <typename I, typename O>
using partial_sort_copy_result = in_out_result<I, O>;

namespace detail {

struct partial_sort_copy_fn {
private:
    template <typename I1, typename S1, typename I2, typename S2,
              typename Comp, typename Proj1, typename Proj2>
    static constexpr partial_sort_copy_result<I1, I2> impl(I1 first, S1 last, I2 result_first,
                             S2 result_last, Comp& comp, Proj1& proj1, Proj2& proj2)
    {
        I2 r = result_first;
        if (r == result_last) {
            // std::move(nano::next()) is needed to avoid GCC ICE.
            return {std::move(nano::next(first, last)), std::move(result_first)};
        }

        while (r != result_last && first != last) {
            *r = *first;
            ++r;
            ++first;
        }

        nano::make_heap(result_first, r, comp, proj2);
        const auto len = nano::distance(result_first, r);

        while (first != last) {
            iter_reference_t<I1>&& x = *first;
            if (nano::invoke(comp, nano::invoke(proj1, x), nano::invoke(proj2, *result_first))) {
                *result_first = std::forward<iter_reference_t<I1>>(x);
                detail::sift_down_n(result_first, len, result_first, comp, proj2);
            }
            ++first;
        }

        nano::sort_heap(result_first, r, comp, proj2);

        return {std::move(first), std::move(r)};
    }

public:
    template <typename I1, typename S1, typename I2, typename S2,
              typename Comp = ranges::less, typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<
        input_iterator<I1> && sentinel_for<S1, I1> &&
            random_access_iterator<I2> &&
            sentinel_for<S2, I2> &&
            indirectly_copyable<I1, I2> && sortable<I2, Comp, Proj2> &&
            indirect_strict_weak_order<Comp, projected<I1, Proj1>, projected<I2, Proj2>>,
    partial_sort_copy_result<I1, I2>>
    operator()(I1 first, S1 last, I2 result_first, S2 result_last, Comp comp = Comp{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return partial_sort_copy_fn::impl(std::move(first), std::move(last),
                                   std::move(result_first), std::move(result_last),
                                   comp, proj1, proj2);
    }

    template <typename Rng1, typename Rng2, typename Comp = ranges::less,
              typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<
        input_range<Rng1> && random_access_range<Rng2> &&
            indirectly_copyable<iterator_t<Rng1>, iterator_t<Rng2>> &&
            sortable<iterator_t<Rng2>, Comp, Proj2> &&
            indirect_strict_weak_order<Comp, projected<iterator_t<Rng1>, Proj1>, projected<iterator_t<Rng2>, Proj2>>,
        partial_sort_copy_result<borrowed_iterator_t<Rng1>, borrowed_iterator_t<Rng2>>>
    operator()(Rng1&& rng, Rng2&& result_rng, Comp comp = Comp{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return partial_sort_copy_fn::impl(nano::begin(rng), nano::end(rng),
                                          nano::begin(result_rng), nano::end(result_rng),
                                          comp, proj1, proj2);
    }
};

}

NANO_INLINE_VAR(detail::partial_sort_copy_fn, partial_sort_copy)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/partition.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_PARTITION_HPP_INCLUDED
#define NANORANGE_ALGORITHM_PARTITION_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

namespace detail {

struct partition_fn {
private:
    template <typename I, typename S, typename Pred, typename Proj>
    static constexpr subrange<I> impl(I first, S last, Pred& pred, Proj& proj)
    {
        I it = nano::find_if_not(first, last, pred, proj);

        if (it == last) {
            return {std::move(it), std::move(nano::next(first, last))};
        }

        auto n = nano::next(it);

        while (n != last) {
            if (nano::invoke(pred, nano::invoke(proj, *n))) {
                nano::iter_swap(n, it);
                ++it;
            }
            ++n;
        }

        return {std::move(it), std::move(n)};
    }

public:
    template <typename I, typename S, typename Pred, typename Proj = identity>
    constexpr std::enable_if_t<
        forward_iterator<I> && sentinel_for<S, I> &&
            indirect_unary_predicate<Pred, projected<I, Proj>>, subrange<I>>
    operator()(I first, S last, Pred pred, Proj proj = Proj{}) const
    {
        return partition_fn::impl(std::move(first), std::move(last),
                                  pred, proj);
    }

    template <typename Rng, typename Pred, typename Proj = identity>
    constexpr std::enable_if_t<
        forward_range<Rng> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>>,
        borrowed_subrange_t<Rng>>
    operator()(Rng&& rng, Pred pred, Proj proj = Proj{}) const
    {
        return partition_fn::impl(nano::begin(rng), nano::end(rng),
                                  pred, proj);
    }
};

}

NANO_INLINE_VAR(detail::partition_fn, partition)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/partition_copy.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_PARTITION_COPY_HPP_INCLUDED
#define NANORANGE_ALGORITHM_PARTITION_COPY_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I, typename O1, typename O2>
using partition_copy_result = in_out_out_result<I, O1, O2>;

namespace detail {

struct partition_copy_fn {
private:
    template <typename I, typename S, typename O1, typename O2,
              typename Pred, typename Proj>
    static constexpr partition_copy_result<I, O1, O2>
    impl(I first, S last, O1 out_true, O2 out_false, Pred& pred, Proj& proj)
    {
        while (first != last) {
            auto&& val = *first;
            if (nano::invoke(pred, nano::invoke(proj, val))) {
                *out_true = std::forward<decltype(val)>(val);
                ++out_true;
            } else {
                *out_false = std::forward<decltype(val)>(val);
                ++out_false;
            }
            ++first;
        }

        return {std::move(first), std::move(out_true), std::move(out_false)};
    }

public:
    template <typename I, typename S, typename O1, typename O2,
              typename Pred, typename Proj = identity>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
        weakly_incrementable<O1> &&
        weakly_incrementable<O2> &&
            indirect_unary_predicate<Pred, projected<I, Proj>> &&
            indirectly_copyable<I, O1> && indirectly_copyable<I, O2>,
        partition_copy_result<I, O1, O2>>
    operator()(I first, S last, O1 out_true, O2 out_false, Pred pred,
               Proj proj = Proj{}) const
    {
        return partition_copy_fn::impl(std::move(first), std::move(last),
                                       std::move(out_true), std::move(out_false),
                                       pred, proj);
    }

    template <typename Rng, typename O1, typename O2,
            typename Pred, typename Proj = identity>
    constexpr std::enable_if_t<
        input_range<Rng> &&
        weakly_incrementable<O1> &&
        weakly_incrementable<O2> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>> &&
            indirectly_copyable<iterator_t<Rng>, O1> &&
            indirectly_copyable<iterator_t<Rng>, O2>,
        partition_copy_result<borrowed_iterator_t<Rng>, O1, O2>>
    operator()(Rng&& rng, O1 out_true, O2 out_false, Pred pred,
            Proj proj = Proj{}) const
    {
        return partition_copy_fn::impl(nano::begin(rng), nano::end(rng),
                                       std::move(out_true), std::move(out_false),
                                       pred, proj);
    }
};

}

NANO_INLINE_VAR(detail::partition_copy_fn, partition_copy)

NANO_END_NAMESPACE

#endif



// nanorange/algorithm/prev_permutation.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Taken from Range-V3
//
// Copyright Eric Niebler 2014-2018
//
//===-------------------------- algorithm ---------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef NANORANGE_ALGORITHM_PREV_PERMUTATION_HPP_INCLUDED
#define NANORANGE_ALGORITHM_PREV_PERMUTATION_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I>
using prev_permutation_result = in_found_result<I>;

namespace detail {

struct prev_permutation_fn {
private:
    template <typename I, typename S, typename Comp, typename Proj>
    static constexpr prev_permutation_result<I>
    impl(I first, S last, Comp& comp, Proj& proj)
    {
        if (first == last) {
            return {std::move(first), false};
        }

        I last_it = nano::next(first, last);
        I i = last_it;

        if (first == --i) {
            return {std::move(last_it), false};
        }

        while (true) {
            I ip1 = i;

            if (nano::invoke(comp, nano::invoke(proj, *ip1),
                             nano::invoke(proj, *--i))) {
                I j = last_it;

                while (!nano::invoke(comp, nano::invoke(proj, *--j),
                                     nano::invoke(proj, *i)));

                nano::iter_swap(i, j);
                nano::reverse(ip1, last_it);
                return {std::move(last_it), true};
            }

            if (i == first) {
                nano::reverse(first, last_it);
                return {std::move(last_it), false};
            }
        }
    }


public:
    template <typename I, typename S, typename Comp = ranges::less,
              typename Proj = identity>
    constexpr std::enable_if_t<bidirectional_iterator<I> && sentinel_for<S, I> &&
                                   sortable<I, Comp, Proj>,
        prev_permutation_result<I>>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return prev_permutation_fn::impl(std::move(first), std::move(last),
                                         comp, proj);
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<
        bidirectional_range<Rng> && sortable<iterator_t<Rng>, Comp, Proj>,
        prev_permutation_result<borrowed_iterator_t<Rng>>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        return prev_permutation_fn::impl(nano::begin(rng), nano::end(rng),
                                         comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::prev_permutation_fn, prev_permutation)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/push_heap.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_PUSH_HEAP_HPP_INCLUDED
#define NANORANGE_ALGORITHM_PUSH_HEAP_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

namespace detail {

struct push_heap_fn {
    template <typename I, typename S, typename Comp = ranges::less,
              typename Proj = identity>
    constexpr std::enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                                   sortable<I, Comp, Proj>, I>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        const auto n = nano::distance(first, last);
        detail::sift_up_n(first, n, comp, proj);
        return first + n;
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<random_access_range<Rng> &&
                                   sortable<iterator_t<Rng>, Comp, Proj>,
                               borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        const auto n = nano::distance(rng);
        detail::sift_up_n(nano::begin(rng), n, comp, proj);
        return nano::begin(rng) + n;
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::push_heap_fn, push_heap)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/remove.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_REMOVE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_REMOVE_HPP_INCLUDED





NANO_BEGIN_NAMESPACE

namespace detail {

struct remove_fn {
private:
    template <typename I, typename S, typename T, typename Proj>
    static constexpr I impl(I first, S last, const T& value, Proj& proj)
    {
        first = nano::find(std::move(first), last, value, proj);

        if (first == last) {
            return first;
        }

        for (auto i = next(first); i != last; ++i) {
            if (!(nano::invoke(proj, *i) == value)) {
                *first = nano::iter_move(i);
                ++first;
            }
        }

        return first;
    }

public:
    template <typename I, typename S, typename T, typename Proj = identity>
    constexpr std::enable_if_t<
        forward_iterator<I> && sentinel_for<S, I> && permutable<I> &&
            indirect_relation<ranges::equal_to, projected<I, Proj>, const T*>,
        I>
    operator()(I first, S last, const T& value, Proj proj = Proj{}) const
    {
        return remove_fn::impl(std::move(first), std::move(last), value, proj);
    }

    template <typename Rng, typename T, typename Proj = identity>
    constexpr std::enable_if_t<
        forward_range<Rng> && permutable<iterator_t<Rng>> &&
            indirect_relation<ranges::equal_to, projected<iterator_t<Rng>, Proj>,
                             const T*>,
        borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, const T& value, Proj proj = Proj{}) const
    {
        return remove_fn::impl(nano::begin(rng), nano::end(rng), value, proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::remove_fn, remove)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/remove_copy.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_REMOVE_COPY_HPP_INCLUDED
#define NANORANGE_ALGORITHM_REMOVE_COPY_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I, typename O>
using remove_copy_result = in_out_result<I, O>;

namespace detail {

struct remove_copy_fn {
private:
    template <typename I, typename S, typename O, typename T, typename Proj>
    static constexpr remove_copy_result<I, O>
    impl(I first, S last, O result, const T& value, Proj& proj)
    {
        while (first != last) {
            auto&& ref = *first;
            if (!(nano::invoke(proj, ref) == value)) {
                *result = std::forward<decltype(ref)>(ref);
                ++result;
            }
            ++first;
        }
        return {std::move(first), std::move(result)};
    }

public:
    template <typename I, typename S, typename O, typename T,
              typename Proj = identity>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> && weakly_incrementable<O> &&
            indirectly_copyable<I, O> &&
            indirect_relation<ranges::equal_to, projected<I, Proj>, const T*>,
        remove_copy_result<I, O>>
    operator()(I first, S last, O result, const T& value,
               Proj proj = Proj{}) const
    {
        return remove_copy_fn::impl(std::move(first), std::move(last),
                                    std::move(result), value, proj);
    }

    template <typename Rng, typename O, typename T, typename Proj = identity>
    constexpr std::enable_if_t<
        input_range<Rng> && weakly_incrementable<O> &&
            indirectly_copyable<iterator_t<Rng>, O> &&
            indirect_relation<ranges::equal_to, projected<iterator_t<Rng>, Proj>,
                             const T*>,
        remove_copy_result<borrowed_iterator_t<Rng>, O>>
    operator()(Rng&& rng, O result, const T& value, Proj proj = Proj{}) const
    {
        return remove_copy_fn::impl(nano::begin(rng), nano::end(rng),
                               std::move(result), value, proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::remove_copy_fn, remove_copy)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/remove_copy_if.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_REMOVE_COPY_IF_HPP_INCLUDED
#define NANORANGE_ALGORITHM_REMOVE_COPY_IF_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I, typename O>
using remove_copy_if_result = in_out_result<I, O>;

namespace detail {

struct remove_copy_if_fn {
private:
    template <typename I, typename S, typename O, typename Pred, typename Proj>
    static constexpr remove_copy_if_result<I, O>
    impl(I first, S last, O result, Pred& pred, Proj& proj)
    {
        while (first != last) {
            auto&& ref = *first;
            if (!nano::invoke(pred, nano::invoke(proj, ref))) {
                *result = std::forward<decltype(ref)>(ref);
                ++result;
            }
            ++first;
        }
        return {std::move(first), std::move(result)};
    }

public:
    template <typename I, typename S, typename O, typename Pred,
              typename Proj = identity>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> && weakly_incrementable<O> &&
            indirectly_copyable<I, O> &&
            indirect_unary_predicate<Pred, projected<I, Proj>>,
        remove_copy_if_result<I, O>>
    operator()(I first, S last, O result, Pred pred,
               Proj proj = Proj{}) const
    {
        return remove_copy_if_fn::impl(std::move(first), std::move(last),
                                    std::move(result), pred, proj);
    }

    template <typename Rng, typename O, typename Pred, typename Proj = identity>
    constexpr std::enable_if_t<
        input_range<Rng> && weakly_incrementable<O> &&
            indirectly_copyable<iterator_t<Rng>, O> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>>,
        remove_copy_if_result<borrowed_iterator_t<Rng>, O>>
    operator()(Rng&& rng, O result, Pred pred, Proj proj = Proj{}) const
    {
        return remove_copy_if_fn::impl(nano::begin(rng), nano::end(rng),
                                       std::move(result), pred, proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::remove_copy_if_fn, remove_copy_if)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/remove_if.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_REMOVE_IF_HPP_INCLUDED
#define NANORANGE_ALGORITHM_REMOVE_IF_HPP_INCLUDED





NANO_BEGIN_NAMESPACE

namespace detail {

struct remove_if_fn {
private:
    template <typename I, typename S, typename Pred, typename Proj>
    static constexpr I impl(I first, S last, Pred& pred, Proj& proj)
    {
        first = nano::find_if(std::move(first), last, pred, proj);

        if (first == last) {
            return first;
        }

        for (auto i = next(first); i != last; ++i) {
            if (!nano::invoke(pred, nano::invoke(proj, *i))) {
                *first = nano::iter_move(i);
                ++first;
            }
        }

        return first;
    }

public:
    template <typename I, typename S, typename Pred, typename Proj = identity>
    constexpr std::enable_if_t<
        forward_iterator<I> && sentinel_for<S, I> && permutable<I> &&
            indirect_unary_predicate<Pred, projected<I, Proj>>,
        I>
    operator()(I first, S last, Pred pred, Proj proj = Proj{}) const
    {
        return remove_if_fn::impl(std::move(first), std::move(last), pred, proj);
    }

    template <typename Rng, typename Pred, typename Proj = identity>
    constexpr std::enable_if_t<
        forward_range<Rng> && permutable<iterator_t<Rng>> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>>,
        borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Pred pred, Proj proj = Proj{}) const
    {
        return remove_if_fn::impl(nano::begin(rng), nano::end(rng), pred, proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::remove_if_fn, remove_if)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/replace.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_REPLACE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_REPLACE_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct replace_fn {
private:
    template <typename I, typename S, typename T1, typename T2, typename Proj>
    static constexpr I impl(I first, S last, const T1& old_value,
                            const T2& new_value, Proj& proj)
    {
        while (first != last) {
            if (nano::invoke(proj, *first) == old_value) {
                *first = new_value;
            }
            ++first;
        }

        return first;
    }

public:
    template <typename I, typename S, typename T1, typename T2,
              typename Proj = identity>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> && writable<I, const T2&> &&
            indirect_relation<ranges::equal_to, projected<I, Proj>, const T1*>,
        I>
    operator()(I first, S last, const T1& old_value, const T2& new_value,
               Proj proj = Proj{}) const
    {
        return replace_fn::impl(std::move(first), std::move(last), old_value,
                                new_value, proj);
    }

    template <typename Rng, typename T1, typename T2, typename Proj = identity>
    constexpr std::enable_if_t<
        input_range<Rng> && writable<iterator_t<Rng>, const T2&> &&
            indirect_relation<ranges::equal_to, projected<iterator_t<Rng>, Proj>,
                             const T1*>,
        borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, const T1& old_value, const T2& new_value,
               Proj proj = Proj{}) const
    {
        return replace_fn::impl(nano::begin(rng), nano::end(rng), old_value,
                                new_value, proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::replace_fn, replace)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/replace_copy.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_REPLACE_COPY_HPP_INCLUDED
#define NANORANGE_ALGORITHM_REPLACE_COPY_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I, typename O>
using replace_copy_result = in_out_result<I, O>;

namespace detail {

struct replace_copy_fn {
private:
    template <typename I, typename S, typename O, typename T1, typename T2,
              typename Proj>
    static constexpr replace_copy_result<I, O>
    impl(I first, S last, O result, const T1& old_value, const T2& new_value,
         Proj& proj)
    {
        while (first != last) {
            if (nano::invoke(proj, *first) == old_value) {
                *result = new_value;
            } else {
                *result = *first;
            }
            ++first;
            ++result;
        }

        return {std::move(first), std::move(result)};
    }

public:
    template <typename I, typename S, typename O, typename T1, typename T2,
              typename Proj = identity>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            output_iterator<O, const T2&> && indirectly_copyable<I, O> &&
            indirect_relation<ranges::equal_to, projected<I, Proj>, const T1*>,
        replace_copy_result<I, O>>
    operator()(I first, S last, O result, const T1& old_value,
               const T2& new_value, Proj proj = Proj{}) const
    {
        return replace_copy_fn::impl(std::move(first), std::move(last),
                                     std::move(result), old_value, new_value,
                                     proj);
    }

    template <typename Rng, typename O, typename T1, typename T2,
              typename Proj = identity>
    constexpr std::enable_if_t<
        input_range<Rng> && output_iterator<O, const T2&> &&
            indirectly_copyable<iterator_t<Rng>, O> &&
            indirect_relation<ranges::equal_to, projected<iterator_t<Rng>, Proj>,
                             const T1*>,
        replace_copy_result<borrowed_iterator_t<Rng>, O>>
    operator()(Rng&& rng, O result, const T1& old_value, const T2& new_value,
               Proj proj = Proj{}) const
    {
        return replace_copy_fn::impl(nano::begin(rng), nano::end(rng),
                                     std::move(result), old_value, new_value,
                                     proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::replace_copy_fn, replace_copy)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/replace_copy_if.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_REPLACE_COPY_IF_HPP_INCLUDED
#define NANORANGE_ALGORITHM_REPLACE_COPY_IF_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I, typename O>
using replace_copy_if_result = in_out_result<I, O>;

namespace detail {

struct replace_copy_if_fn {
private:
    template <typename I, typename S, typename O, typename Pred, typename T,
              typename Proj>
    static constexpr replace_copy_if_result<I, O>
    impl(I first, S last, O result, Pred& pred, const T& new_value, Proj& proj)
    {
        while (first != last) {
            if (nano::invoke(pred, nano::invoke(proj, *first))) {
                *result = new_value;
            } else {
                *result = *first;
            }
            ++first;
            ++result;
        }

        return {std::move(first), std::move(result)};
    }

public:
    template <typename I, typename S, typename O, typename Pred, typename T,
              typename Proj = identity>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            output_iterator<O, const T&> && indirectly_copyable<I, O> &&
            indirect_unary_predicate<Pred, projected<I, Proj>>,
        replace_copy_if_result<I, O>>
    operator()(I first, S last, O result, Pred pred, const T& new_value,
               Proj proj = Proj{}) const
    {
        return replace_copy_if_fn::impl(std::move(first), std::move(last),
                                        std::move(result), pred, new_value,
                                        proj);
    }

    template <typename Rng, typename O, typename Pred, typename T,
              typename Proj = identity>
    constexpr std::enable_if_t<
        input_range<Rng> && output_iterator<O, const T&> &&
            indirectly_copyable<iterator_t<Rng>, O> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>>,
        replace_copy_if_result<borrowed_iterator_t<Rng>, O>>
    operator()(Rng&& rng, O result, Pred pred, const T& new_value,
               Proj proj = Proj{}) const
    {
        return replace_copy_if_fn::impl(nano::begin(rng), nano::end(rng),
                                        std::move(result), pred, new_value,
                                        proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::replace_copy_if_fn, replace_copy_if)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/replace_if.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_REPLACE_IF_HPP_INCLUDED
#define NANORANGE_ALGORITHM_REPLACE_IF_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct replace_if_fn {
private:
    template <typename I, typename S, typename Pred, typename T, typename Proj>
    static constexpr I impl(I first, S last, Pred& pred, const T& new_value,
                            Proj& proj)
    {
        while (first != last) {
            if (nano::invoke(pred, nano::invoke(proj, *first))) {
                *first = new_value;
            }
            ++first;
        }

        return first;
    }

public:
    template <typename I, typename S, typename T, typename Pred,
              typename Proj = identity>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> && writable<I, const T&> &&
            indirect_unary_predicate<Pred, projected<I, Proj>>,
        I>
    operator()(I first, S last, Pred pred, const T& new_value,
               Proj proj = Proj{}) const
    {
        return replace_if_fn::impl(std::move(first), std::move(last), pred,
                                   new_value, proj);
    }

    template <typename Rng, typename Pred, typename T2,
              typename Proj = identity>
    constexpr std::enable_if_t<
        input_range<Rng> && writable<iterator_t<Rng>, const T2&> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>>,
        borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Pred pred, const T2& new_value,
               Proj proj = Proj{}) const
    {
        return replace_if_fn::impl(nano::begin(rng), nano::end(rng), pred,
                                   new_value, proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::replace_if_fn, replace_if)

NANO_END_NAMESPACE

#endif


// nanorange/algorithm/reverse_copy.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_REVERSE_COPY_HPP_INCLUDED
#define NANORANGE_ALGORITHM_REVERSE_COPY_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I, typename O>
using reverse_copy_result = in_out_result<I, O>;

namespace detail {

struct reverse_copy_fn {
private:
    template <typename I, typename O>
    static constexpr reverse_copy_result<I, O> impl(I first, I last, O result)
    {
        auto ret = last;
        while (last != first) {
            *result = *--last;
            ++result;
        }

        return {std::move(ret), std::move(result)};
    }

    template <typename I, typename S, typename O>
    static constexpr std::enable_if_t<
        !same_as<I, S>, reverse_copy_result<I, O>>
    impl(I first, S bound, O result)
    {
        return reverse_copy_fn::impl(std::move(first), nano::next(first, bound),
                                     std::move(result));
    }

public:
    template <typename I, typename S, typename O>
    constexpr std::enable_if_t<
        bidirectional_iterator<I> && sentinel_for<S, I> &&
        weakly_incrementable<O> && indirectly_copyable<I, O>,
        reverse_copy_result<I, O>>
    operator()(I first, S last, O result) const
    {
        return reverse_copy_fn::impl(std::move(first), std::move(last),
                                     std::move(result));
    }

    template <typename Rng, typename O>
    constexpr std::enable_if_t<bidirectional_range<Rng> &&
        weakly_incrementable<O> &&
                                   indirectly_copyable<iterator_t<Rng>, O>,
        reverse_copy_result<borrowed_iterator_t<Rng>, O>>
    operator()(Rng&& rng, O result) const
    {
        return reverse_copy_fn::impl(nano::begin(rng), nano::end(rng),
                                     std::move(result));
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::reverse_copy_fn, reverse_copy)

NANO_END_NAMESPACE

#endif


// nanorange/algorithm/rotate_copy.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_ROTATE_COPY_HPP_INCLUDED
#define NANORANGE_ALGORITHM_ROTATE_COPY_HPP_INCLUDED





NANO_BEGIN_NAMESPACE

template <typename I, typename O>
using rotate_copy_result = in_out_result<I, O>;

namespace detail {

struct rotate_copy_fn {
private:
    template <typename I, typename S, typename O>
    static constexpr rotate_copy_result<I, O>
    impl(I first, I middle, S last, O result)
    {
        auto ret = nano::copy(middle, std::move(last), std::move(result));
        ret.out = nano::copy(std::move(first), std::move(middle), ret.out).out;
        return ret;
    }

public:
    template <typename I, typename S, typename O>
    constexpr std::enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
        weakly_incrementable<O> &&
                                   indirectly_copyable<I, O>,
        rotate_copy_result<I, O>>
    operator()(I first, I middle, S last, O result) const
    {
        return rotate_copy_fn::impl(std::move(first), std::move(middle),
                                    std::move(last), std::move(result));
    }

    template <typename Rng, typename O>
    constexpr std::enable_if_t<forward_range<Rng> &&
        weakly_incrementable<O> &&
                                   indirectly_copyable<iterator_t<Rng>, O>,
        rotate_copy_result<borrowed_iterator_t<Rng>, O>>
    operator()(Rng&& rng, iterator_t<Rng> middle, O result) const
    {
        return rotate_copy_fn::impl(nano::begin(rng), std::move(middle),
                                    nano::end(rng), std::move(result));
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::rotate_copy_fn, rotate_copy)

NANO_END_NAMESPACE

#endif


// nanorange/algorithm/search_n.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_SEARCH_N_HPP_INCLUDED
#define NANORANGE_ALGORITHM_SEARCH_N_HPP_INCLUDED




NANO_BEGIN_NAMESPACE


namespace detail {

struct search_n_fn {
private:
    template <typename I, typename S, typename T, typename Pred, typename Proj>
    static constexpr subrange<I>
    impl(I first, S last, iter_difference_t<I> count, const T& value, Pred pred,
         Proj& proj)
    {
        if (count == iter_difference_t<I>{0}) {
            return {first, first};
        }

        for (; first != last; ++first) {
            if (!nano::invoke(pred, nano::invoke(proj, *first), value)) {
                continue;
            }

            I save = first;
            iter_difference_t<I> running_count{1};

            while (true) {
                if (running_count++ == count) {
                    // Success
                    return {save, nano::next(first)};
                }

                if (++first == last) {
                    // We have run out of elements
                    return {first, first};
                }

                if (!nano::invoke(pred, nano::invoke(proj, *first), value)) {
                    break;
                }
            }
        }

        return {first, first};
    }

public:
    template <typename I, typename S, typename T, typename Pred = ranges::equal_to,
        typename Proj = identity>
    constexpr auto operator()(I first, S last, iter_difference_t<I> count,
                              const T& value, Pred pred = Pred{},
                              Proj proj = Proj{}) const
    -> std::enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                                indirectly_comparable<I, const T*, Pred, Proj>,
        subrange<I>>
    {
        return search_n_fn::impl(std::move(first), std::move(last), count,
                                 value, pred, proj);
    }

    template <typename Rng, typename T, typename Pred = ranges::equal_to,
        typename Proj = identity>
    constexpr auto
    operator()(Rng&& rng, iter_difference_t<iterator_t<Rng>> count,
               const T& value, Pred pred = Pred{}, Proj proj = Proj{}) const
    -> std::enable_if_t<
            forward_range<Rng> &&
                indirectly_comparable<iterator_t<Rng>, const T*, Pred, Proj>,
            borrowed_subrange_t<Rng>>
    {
        return search_n_fn::impl(nano::begin(rng), nano::end(rng), count, value, pred,
                                 proj);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::search_n_fn, search_n)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/set_difference.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_SET_DIFFERENCE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_SET_DIFFERENCE_HPP_INCLUDED






NANO_BEGIN_NAMESPACE

template <typename I, typename O>
using set_difference_result = in_out_result<I, O>;

namespace detail {

struct set_difference_fn {
private:
    template <typename I1, typename S1, typename I2, typename S2, typename O,
              typename Comp, typename Proj1, typename Proj2>
    static constexpr set_difference_result<I1, O>
    impl(I1 first1, S1 last1, I2 first2, S2 last2, O result,
         Comp& comp, Proj1& proj1, Proj2& proj2)
    {
        while (first1 != last1) {
            if (first2 == last2) {
                // We've reached the end of range2, so copy all the remaining
                // elements from range1 and exit
                auto res = nano::copy(std::move(first1),  std::move(last1),
                                      std::move(result));
                first1 = std::move(res.in);
                result = std::move(res.out);

                break;
            }

            // If the element from r1 compares less than the one from r2, then
            // copy it
            if (nano::invoke(comp, nano::invoke(proj1, *first1),
                             nano::invoke(proj2, *first2))) {
                *result = *first1;
                ++first1;
                ++result;
            } else{
                // We now know that !(r1 < r2). If !(r2 < r1) as well, then
                // elements are equal and we can skip
                if (!nano::invoke(comp, nano::invoke(proj2, *first2),
                                  nano::invoke(proj1, *first1))) {
                    ++first1;
                }
                ++first2;
            }
        }

        return {std::move(first1), std::move(result)};
    }


public:
    template <typename I1, typename S1, typename I2, typename S2, typename O,
              typename Comp = ranges::less, typename Proj1 = identity,
              typename Proj2 = identity>
    constexpr std::enable_if_t<input_iterator<I1> &&
        sentinel_for<S1, I1> &&
        input_iterator<I2> &&
        sentinel_for<S2, I2> &&
        weakly_incrementable<O> &&
        mergeable<I1, I2, O, Comp, Proj1, Proj2>,
        set_difference_result<I1, O>>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, O result,
               Comp comp = Comp{}, Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return set_difference_fn::impl(std::move(first1), std::move(last1),
                                       std::move(first2), std::move(last2),
                                       std::move(result), comp,
                                       proj1, proj2);
    }

    template <typename Rng1, typename Rng2, typename O, typename Comp = ranges::less,
              typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<input_range<Rng1> &&
        input_range<Rng2> &&
        weakly_incrementable<O> &&
        mergeable<iterator_t<Rng1>, iterator_t<Rng2>, O, Comp, Proj1, Proj2>,
        set_difference_result<borrowed_iterator_t<Rng1>, O>>
    operator()(Rng1&& rng1, Rng2&& rng2, O result, Comp comp = Comp{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return set_difference_fn::impl(nano::begin(rng1), nano::end(rng1),
                                       nano::begin(rng2), nano::end(rng2),
                                       std::move(result), comp,
                                       proj1, proj2);
    }
};

}

NANO_INLINE_VAR(detail::set_difference_fn, set_difference)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/set_intersection.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_SET_INTERSECTION_HPP_INCLUDED
#define NANORANGE_ALGORITHM_SET_INTERSECTION_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct set_intersection_fn {
private:
    template <typename I1, typename S1, typename I2, typename S2, typename O,
              typename Comp, typename Proj1, typename Proj2>
    static constexpr O impl(I1 first1, S1 last1, I2 first2, S2 last2,
                            O result, Comp& comp, Proj1& proj1, Proj2& proj2)
    {
        while (first1 != last1 && first2 != last2)
        {
            if (nano::invoke(comp, nano::invoke(proj1, *first1),
                             nano::invoke(proj2, *first2))) {
                ++first1;
            } else {
                if (!nano::invoke(comp, nano::invoke(proj2, *first2),
                                 nano::invoke(proj1, *first1))) {
                    *result = *first1;
                    ++result;
                    ++first1;
                }
                ++first2;
            }
        }

        return result;
    }

public:
    template <typename I1, typename S1, typename I2, typename S2, typename O,
              typename Comp = ranges::less, typename Proj1 = identity,
              typename Proj2 = identity>
    constexpr std::enable_if_t<input_iterator<I1> &&
        sentinel_for<S1, I1> &&
        input_iterator<I2> &&
        sentinel_for<S2, I2> &&
        weakly_incrementable<O> &&
        mergeable<I1, I2, O, Comp, Proj1, Proj2>, O>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, O result,
               Comp comp = Comp{}, Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return set_intersection_fn::impl(std::move(first1), std::move(last1),
                                         std::move(first2), std::move(last2),
                                         std::move(result), comp, proj1, proj2);
    }

    template <typename Rng1, typename Rng2, typename O, typename Comp = ranges::less,
              typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<input_range<Rng1> &&
        input_range<Rng2> &&
        weakly_incrementable<O> &&
        mergeable<iterator_t<Rng1>, iterator_t<Rng2>, O, Comp, Proj1, Proj2>,
         O>
    operator()(Rng1&& rng1, Rng2&& rng2, O result, Comp comp = Comp{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return set_intersection_fn::impl(nano::begin(rng1), nano::end(rng1),
                                         nano::begin(rng2), nano::end(rng2),
                                         std::move(result), comp, proj1, proj2);
    }
};

}

NANO_INLINE_VAR(detail::set_intersection_fn, set_intersection)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/set_symmetric_difference.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_SET_SYMMETRIC_DIFFERENCE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_SET_SYMMETRIC_DIFFERENCE_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I1, typename I2, typename O>
using set_symmetric_difference_result = in_in_out_result<I1, I2, O>;

namespace detail {

struct set_symmetric_difference_fn {
private:
    template <typename I1, typename S1, typename I2, typename S2, typename O,
            typename Comp, typename Proj1, typename Proj2>
    static constexpr set_symmetric_difference_result<I1, I2, O>
    impl(I1 first1, S1 last1, I2 first2, S2 last2, O result,
         Comp& comp, Proj1& proj1, Proj2& proj2)
    {
        while (true) {
            if (first1 == last1) {
                auto copy_res = nano::copy(std::move(first2), std::move(last2),
                                           std::move(result));


                return {std::move(first1), std::move(copy_res.in),
                        std::move(copy_res.out)};
            }

            if (first2 == last2) {
                auto copy_res = nano::copy(std::move(first1),  std::move(last1),
                                           std::move(result));
                return {std::move(copy_res.in), std::move(first2),
                        std::move(copy_res.out)};
            }

            // If r1 is less than r2, copy it to the output
            if (nano::invoke(comp, nano::invoke(proj1, *first1),
                             nano::invoke(proj2, *first2))) {
                *result = *first1;
                ++result;
                ++first1;
            } else {
                // We now know that !(r1 < r2). If !(r2 < r1) as well then
                // the elements are equal -- so skip
                if (!nano::invoke(comp, nano::invoke(proj2, *first2),
                                  nano::invoke(proj1, *first1))) {
                    ++first1;
                } else {
                    // Otherwise copy first2
                    *result = *first2;
                    ++result;
                }
                ++first2;
            }
        }
    }

public:
    template <typename I1, typename S1, typename I2, typename S2, typename O,
              typename Comp = ranges::less, typename Proj1 = identity,
              typename Proj2 = identity>
    constexpr std::enable_if_t<input_iterator<I1> &&
        sentinel_for<S1, I1> &&
        input_iterator<I2> &&
        sentinel_for<S2, I2> &&
        weakly_incrementable<O> &&
        mergeable<I1, I2, O, Comp, Proj1, Proj2>,
        set_symmetric_difference_result<I1, I2, O>>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, O result, Comp comp = Comp{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return set_symmetric_difference_fn::impl(std::move(first1), std::move(last1),
                                                 std::move(first2), std::move(last2),
                                                 std::move(result), comp,
                                                 proj1, proj2);
    }

    template <typename Rng1, typename Rng2, typename O, typename Comp = ranges::less,
              typename Proj1 = identity, typename Proj2 = identity>
    std::enable_if_t<input_range<Rng1> &&
        input_range<Rng2> &&
        weakly_incrementable<O> &&
        mergeable<iterator_t<Rng1>, iterator_t<Rng2>, O, Comp, Proj1, Proj2>,
        set_symmetric_difference_result<borrowed_iterator_t<Rng1>,
                                        borrowed_iterator_t<Rng2>, O>>
    operator()(Rng1&& rng1, Rng2&& rng2, O result, Comp comp = Comp{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return set_symmetric_difference_fn::impl(nano::begin(rng1), nano::end(rng1),
                                                 nano::begin(rng2), nano::end(rng2),
                                                 std::move(result), comp,
                                                 proj1, proj2);
    }
};

}

NANO_INLINE_VAR(detail::set_symmetric_difference_fn, set_symmetric_difference)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/set_union.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_SET_UNION_HPP_INCLUDED
#define NANORANGE_ALGORITHM_SET_UNION_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I1, typename I2, typename O>
using set_union_result = in_in_out_result<I1, I2, O>;

namespace detail {

struct set_union_fn {
private:
    template <typename I1, typename S1, typename I2, typename S2, typename O,
              typename Comp, typename Proj1, typename Proj2>
    static constexpr set_union_result<I1, I2, O>
    impl(I1 first1, S1 last1, I2 first2, S2 last2, O result, Comp& comp,
         Proj1& proj1, Proj2& proj2)
    {
        while (first1 != last1) {
            // If we've reached the end of the second range, copy any remaining
            // elements from the first range and quit
            if (first2 == last2) {
                auto copy_res = nano::copy(std::move(first1),  std::move(last1),
                                           std::move(result));

                first1 = std::move(copy_res.in);
                result = std::move(copy_res.out);

                break;
            }

            // If this element from r1 is less than the current element from r2,
            // copy it and move on
            if (nano::invoke(comp, nano::invoke(proj1, *first1),
                             nano::invoke(proj2, *first2))) {
                *result = *first1;
                ++first1;
            } else {
                // Now, we know that !(r1 < r2). If we also have !(r2 < r1) then
                // the elements compare equal, so skip it
                if (!nano::invoke(comp, nano::invoke(proj2, *first2),
                                  nano::invoke(proj1, *first1))) {
                    ++first1;
                }
                *result = *first2;
                ++first2;
            }
            ++result;
        }

        // We've run out of elements of range1, so copy all the remaining
        // elements of range2
        auto copy_res = nano::copy(std::move(first2), std::move(last2),
                                   std::move(result));


        return {std::move(first1), std::move(copy_res.in),
                std::move(copy_res.out)};
    }

public:
    template <typename I1, typename S1, typename I2, typename S2, typename O,
              typename Comp = ranges::less, typename Proj1 = identity,
              typename Proj2 = identity>
    constexpr std::enable_if_t<input_iterator<I1> &&
        sentinel_for<S1, I1> &&
        input_iterator<I2> &&
        sentinel_for<S2, I2> &&
        weakly_incrementable<O> &&
        mergeable<I1, I2, O, Comp, Proj1, Proj2>,
        set_union_result<I1, I2, O>>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, O result,
               Comp comp = Comp{}, Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return set_union_fn::impl(std::move(first1), std::move(last1),
                                  std::move(first2), std::move(last2),
                                  std::move(result), comp,
                                  proj1, proj2);
    }

    template <typename Rng1, typename Rng2, typename O, typename Comp = ranges::less,
              typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<
        input_range<Rng1> &&
        input_range<Rng2> &&
        weakly_incrementable<O> &&
        mergeable<iterator_t<Rng1>, iterator_t<Rng2>, O, Comp, Proj1, Proj2>,
        set_union_result<borrowed_iterator_t<Rng1>,
                                                borrowed_iterator_t<Rng2>, O>>
    operator()(Rng1&& rng1, Rng2&& rng2, O result, Comp comp = Comp{},
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return set_union_fn::impl(nano::begin(rng1), nano::end(rng1),
                                  nano::begin(rng2), nano::end(rng2),
                                  std::move(result), comp,
                                  proj1, proj2);
    }
};

}

NANO_INLINE_VAR(detail::set_union_fn, set_union)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/sample.hpp
//
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef NANORANGE_ALGORITHM_SAMPLE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_SAMPLE_HPP_INCLUDED


// nanorange/random.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_RANDOM_HPP_INCLUDED
#define NANORANGE_RANDOM_HPP_INCLUDED

// nanorange/concepts.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_CONCEPTS_HPP_INCLUDED
#define NANORANGE_CONCEPTS_HPP_INCLUDED






#endif


NANO_BEGIN_NAMESPACE

//  [rand.req.urng]

namespace detail {

struct uniform_random_bit_generator_concept {
    template <typename>
    static auto test(long) -> std::false_type;

    template <typename G>
    static auto test(int) -> std::enable_if_t<
        invocable<G&> &&
        unsigned_integral<invoke_result_t<G&>> &&
        detail::requires_<uniform_random_bit_generator_concept, G>,
        std::true_type>;


    template <typename G>
    auto requires_() -> decltype(
        requires_expr<same_as<decltype(G::min()), invoke_result_t<G&>>>{},
        requires_expr<same_as<decltype(G::max()), invoke_result_t<G&>>>{}
    );
};

} // namespace detail

template <typename G>
NANO_CONCEPT uniform_random_bit_generator =
    decltype(detail::uniform_random_bit_generator_concept::test<G>(0))::value;

NANO_END_NAMESPACE

#endif



#include <random>

NANO_BEGIN_NAMESPACE

namespace detail {

struct sample_fn {
private:
    template <typename I, typename S, typename O, typename Gen>
    static O impl_fwd(I first, S last, O out, iter_difference_t<I> n, Gen& g)
    {
        using diff_t = iter_difference_t<I>;
        using distr_t = std::uniform_int_distribution<diff_t>;
        using param_t = typename distr_t::param_type;

        distr_t D;

        auto unsampled_size = nano::distance(first, last);

        for (n = nano::min(n, unsampled_size); n != 0; ++first) {
            if (D(g, param_t(0, --unsampled_size)) < n) {
                *out++ = *first;
                --n;
            }
        }

        return out;
    }
    template <typename I, typename S, typename O, typename Gen>
    static O impl_ra(I first, S last, O out, iter_difference_t<I> n, Gen& g) {
        using diff_t = iter_difference_t<I>;
        using distr_t = std::uniform_int_distribution<diff_t>;
        using param_t = typename distr_t::param_type;

        distr_t D;
        diff_t k = 0;

        for(; first != last && k < n; ++first, (void) ++k) {
            out[k] = *first;
        }

        diff_t size = k;
        for (; first != last; ++first, (void) ++k) {
            diff_t r = distr_t(0, k)(g);
            if (D(g, param_t(0, k)) < size) {
                out[r] = *first;
            }
        }

        return out + nano::min(n, k);
    }

    template <typename I, typename S, typename O, typename Gen>
    static O impl(I first, S last, O out, iter_difference_t<I> n, Gen& g)
    {
        if constexpr (nano::forward_iterator<I>) {
            return impl_fwd(std::move(first), std::move(last), std::move(out), n, g);
	} else {
            return impl_ra(std::move(first), std::move(last), std::move(out), n, g);
	}
    }

public:
    template <typename I, typename S, typename O, typename Gen>
    std::enable_if_t<
        input_iterator<I> &&
        sentinel_for<S, I> &&
        weakly_incrementable<O> &&
        (forward_iterator<I> || random_access_iterator<O>) &&
        indirectly_copyable<I, O> &&
        uniform_random_bit_generator<std::remove_reference_t<Gen>>,
        O>
    operator()(I first, S last, O out, iter_difference_t<I> n, Gen&& gen) const
    {
        return sample_fn::impl(std::move(first), std::move(last),
                               std::move(out), std::move(n),
                               std::forward<Gen>(gen));
    }

    template <typename Rng, typename O, typename Gen>
    std::enable_if_t<
        input_range<Rng> &&
        weakly_incrementable<O> &&
        (forward_range<Rng> || random_access_iterator<O> ) &&
        indirectly_copyable<iterator_t<Rng>, O> &&
        uniform_random_bit_generator<std::remove_reference_t<Gen>>,
        O>
    operator()(Rng&& rng, O out, range_difference_t<Rng> n, Gen&& gen) const
    {
        return sample_fn::impl(nano::begin(rng), nano::end(rng),
                               std::move(out), std::move(n), std::forward<Gen>(gen));
    }
};

}

NANO_INLINE_VAR(detail::sample_fn, sample)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/shuffle.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_SHUFFLE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_SHUFFLE_HPP_INCLUDED




#include <random>

NANO_BEGIN_NAMESPACE

namespace detail {

struct shuffle_fn {
private:
    template <typename I, typename S, typename Gen>
    static constexpr I impl(I first, S last, Gen&& g)
    {
        using diff_t = iter_difference_t<I>;
        using distr_t = std::uniform_int_distribution<diff_t>;
        using param_t = typename distr_t::param_type;

        distr_t D;
        const auto n = last - first; // OK, we have SizedSentinel

        for (diff_t i = 0; i < n; i++) {
            nano::iter_swap(first + i, first + D(g, param_t(0, i)));
        }

        return next(first, last);
    }

public:
    template <typename I, typename S, typename Gen>
    constexpr std::enable_if_t<
        random_access_iterator<I> && sentinel_for<S, I> &&
            uniform_random_bit_generator<std::remove_reference_t<Gen>> &&
        convertible_to<invoke_result_t<Gen&>, iter_difference_t<I>>,
        I>
    operator()(I first, S last, Gen&& gen) const
    {
        return shuffle_fn::impl(std::move(first), std::move(last),
                                std::forward<Gen>(gen));
    }

    template <typename Rng, typename Gen>
    constexpr std::enable_if_t<
        random_access_range<Rng> &&
            uniform_random_bit_generator<std::remove_reference_t<Gen>> &&
            convertible_to<invoke_result_t<Gen&>, iter_difference_t<iterator_t<Rng>>>,
        borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Gen&& gen) const
    {
        return shuffle_fn::impl(nano::begin(rng), nano::end(rng),
                                std::forward<Gen>(gen));
    }
};

}

NANO_INLINE_VAR(detail::shuffle_fn, shuffle)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/sort.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_SORT_HPP_INCLUDED
#define NANORANGE_ALGORITHM_SORT_HPP_INCLUDED

// nanorange/detail/algorithm/pqdsort.hpp
//
// Copyright Orson Peters 2017.
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Modified from Boost.Sort by Orson Peters
// https://github.com/boostorg/sort/blob/develop/include/boost/sort/pdqsort/pdqsort.hpp

#ifndef NANORANGE_DETAIL_ALGORITHM_PDQSORT_HPP_INCLUDED
#define NANORANGE_DETAIL_ALGORITHM_PDQSORT_HPP_INCLUDED






NANO_BEGIN_NAMESPACE

namespace detail {

// Partitions below this size are sorted using insertion sort.
constexpr int pdqsort_insertion_sort_threshold = 24;

// Partitions above this size use Tukey's ninther to select the pivot.
constexpr int pdqsort_ninther_threshold = 128;

// When we detect an already sorted partition, attempt an insertion sort that
// allows this amount of element moves before giving up.
constexpr int pqdsort_partial_insertion_sort_limit = 8;

// Must be multiple of 8 due to loop unrolling, and < 256 to fit in unsigned
// char.
constexpr int pdqsort_block_size = 64;

// Cacheline size, assumes power of two.
constexpr int pdqsort_cacheline_size = 64;

template <typename>
struct is_default_compare : std::false_type {};
template <>
struct is_default_compare<nano::less> : std::true_type {};
template <>
struct is_default_compare<nano::greater> : std::true_type {};
template <typename T>
struct is_default_compare<std::less<T>> : std::true_type {};
template <typename T>
struct is_default_compare<std::greater<T>> : std::true_type {};

template <typename T>
constexpr bool is_default_compare_v = is_default_compare<T>::value;

// Returns floor(log2(n)), assumes n > 0.
template <class T>
constexpr int log2(T n)
{
    int log = 0;
    while (n >>= 1)
        ++log;
    return log;
}

// Sorts [begin, end) using insertion sort with the given comparison function.
template <typename I, typename Comp, typename Proj>
constexpr void insertion_sort(I begin, I end, Comp& comp, Proj& proj)
{
    using T = iter_value_t<I>;

    if (begin == end) {
        return;
    }

    for (I cur = begin + 1; cur != end; ++cur) {
        I sift = cur;
        I sift_1 = cur - 1;

        // Compare first so we can avoid 2 moves for an element already
        // positioned correctly.
        if (nano::invoke(comp, nano::invoke(proj, *sift),
                         nano::invoke(proj, *sift_1))) {
            T tmp = nano::iter_move(sift);

            do {
                *sift-- = nano::iter_move(sift_1);
            } while (sift != begin &&
                     nano::invoke(comp, nano::invoke(proj, tmp),
                                  nano::invoke(proj, *--sift_1)));

            *sift = std::move(tmp);
        }
    }
}

// Sorts [begin, end) using insertion sort with the given comparison function.
// Assumes
// *(begin - 1) is an element smaller than or equal to any element in [begin,
// end).
template <typename I, typename Comp, typename Proj>
constexpr void unguarded_insertion_sort(I begin, I end, Comp& comp, Proj& proj)
{
    using T = iter_value_t<I>;

    if (begin == end) {
        return;
    }

    for (I cur = begin + 1; cur != end; ++cur) {
        I sift = cur;
        I sift_1 = cur - 1;

        // Compare first so we can avoid 2 moves for an element already
        // positioned correctly.
        if (nano::invoke(comp, nano::invoke(proj, *sift),
                         nano::invoke(proj, *sift_1))) {
            T tmp = nano::iter_move(sift);

            do {
                *sift-- = nano::iter_move(sift_1);
            } while (nano::invoke(comp, nano::invoke(proj, tmp),
                                  nano::invoke(proj, *--sift_1)));

            *sift = std::move(tmp);
        }
    }
}

// Attempts to use insertion sort on [begin, end). Will return false if more
// than partial_insertion_sort_limit elements were moved, and abort sorting.
// Otherwise it will successfully sort and return true.
template <typename I, typename Comp, typename Proj>
constexpr bool partial_insertion_sort(I begin, I end, Comp& comp, Proj& proj)
{
    using T = iter_value_t<I>;

    if (begin == end) {
        return true;
    }

    iter_difference_t<I> limit = 0;
    for (I cur = begin + 1; cur != end; ++cur) {
        if (limit > pqdsort_partial_insertion_sort_limit) {
            return false;
        }

        I sift = cur;
        I sift_1 = cur - 1;

        // Compare first so we can avoid 2 moves for an element already
        // positioned correctly.
        if (nano::invoke(comp, nano::invoke(proj, *sift),
                         nano::invoke(proj, *sift_1))) {
            T tmp = nano::iter_move(sift);

            do {
                *sift-- = nano::iter_move(sift_1);
            } while (sift != begin &&
                     nano::invoke(comp, nano::invoke(proj, tmp),
                                  nano::invoke(proj, *--sift_1)));

            *sift = std::move(tmp);
            limit += cur - sift;
        }
    }

    return true;
}

template <typename I, typename Comp, typename Proj>
constexpr void sort2(I a, I b, Comp& comp, Proj& proj)
{
    if (nano::invoke(comp, nano::invoke(proj, *b), nano::invoke(proj, *a))) {
        nano::iter_swap(a, b);
    }
}

// Sorts the elements *a, *b and *c using comparison function comp.
template <typename I, typename Comp, typename Proj>
constexpr void sort3(I a, I b, I c, Comp& comp, Proj& proj)
{
    sort2(a, b, comp, proj);
    sort2(b, c, comp, proj);
    sort2(a, b, comp, proj);
}

template <typename I>
constexpr void swap_offsets(I first, I last, unsigned char* offsets_l,
                            unsigned char* offsets_r, int num, bool use_swaps)
{
    using T = iter_value_t<I>;
    if (use_swaps) {
        // This case is needed for the descending distribution, where we need
        // to have proper swapping for pdqsort to remain O(n).
        for (int i = 0; i < num; ++i) {
            nano::iter_swap(first + offsets_l[i], last - offsets_r[i]);
        }
    } else if (num > 0) {
        I l = first + offsets_l[0];
        I r = last - offsets_r[0];
        T tmp(nano::iter_move(l));
        *l = nano::iter_move(r);

        for (int i = 1; i < num; ++i) {
            l = first + offsets_l[i];
            *r = nano::iter_move(l);
            r = last - offsets_r[i];
            *l = nano::iter_move(r);
        }
        *r = std::move(tmp);
    }
}

// Partitions [begin, end) around pivot *begin using comparison function comp.
// Elements equal to the pivot are put in the right-hand partition. Returns the
// position of the pivot after partitioning and whether the passed sequence
// already was correctly partitioned. Assumes the pivot is a median of at least
// 3 elements and that [begin, end) is at least insertion_sort_threshold long.
// Uses branchless partitioning.
template <typename I, typename Comp, typename Pred>
constexpr std::pair<I, bool> partition_right_branchless(I begin, I end,
                                                        Comp& comp, Pred& pred)
{
    using T = iter_value_t<I>;

    // Move pivot into local for speed.
    T pivot(nano::iter_move(begin));
    I first = begin;
    I last = end;

    // Find the first element greater than or equal than the pivot (the median
    // of 3 guarantees this exists).
    while (nano::invoke(comp, nano::invoke(pred, *++first),
                        nano::invoke(pred, pivot)))
        ;

    // Find the first element strictly smaller than the pivot. We have to guard
    // this search if there was no element before *first.
    if (first - 1 == begin) {
        while (first < last && !nano::invoke(comp, nano::invoke(pred, *--last),
                                             nano::invoke(pred, pivot)))
            ;
    } else {
        while (!nano::invoke(comp, nano::invoke(pred, *--last),
                             nano::invoke(pred, pivot)))
            ;
    }

    // If the first pair of elements that should be swapped to partition are the
    // same element, the passed in sequence already was correctly partitioned.
    bool already_partitioned = first >= last;
    if (!already_partitioned) {
        nano::iter_swap(first, last);
        ++first;
    }

    // The following branchless partitioning is derived from "BlockQuicksort:
    // How Branch Mispredictions don't affect Quicksort" by Stefan Edelkamp and
    // Armin Weiss.
    alignas(pdqsort_cacheline_size) unsigned char
        offsets_l_storage[pdqsort_block_size] = {};
    alignas(pdqsort_cacheline_size) unsigned char
        offsets_r_storage[pdqsort_block_size] = {};
    unsigned char* offsets_l = offsets_l_storage;
    unsigned char* offsets_r = offsets_r_storage;
    int num_l = 0, num_r = 0, start_l = 0, start_r = 0;

    while (last - first > 2 * pdqsort_block_size) {
        // Fill up offset blocks with elements that are on the wrong side.
        if (num_l == 0) {
            start_l = 0;
            I it = first;
            for (unsigned char i = 0; i < pdqsort_block_size;) {
                offsets_l[num_l] = i++;
                num_l += !nano::invoke(comp, nano::invoke(pred, *it),
                                       nano::invoke(pred, pivot));
                ++it;
                offsets_l[num_l] = i++;
                num_l += !nano::invoke(comp, nano::invoke(pred, *it),
                                       nano::invoke(pred, pivot));
                ++it;
                offsets_l[num_l] = i++;
                num_l += !nano::invoke(comp, nano::invoke(pred, *it),
                                       nano::invoke(pred, pivot));
                ++it;
                offsets_l[num_l] = i++;
                num_l += !nano::invoke(comp, nano::invoke(pred, *it),
                                       nano::invoke(pred, pivot));
                ++it;
                offsets_l[num_l] = i++;
                num_l += !nano::invoke(comp, nano::invoke(pred, *it),
                                       nano::invoke(pred, pivot));
                ++it;
                offsets_l[num_l] = i++;
                num_l += !nano::invoke(comp, nano::invoke(pred, *it),
                                       nano::invoke(pred, pivot));
                ++it;
                offsets_l[num_l] = i++;
                num_l += !nano::invoke(comp, nano::invoke(pred, *it),
                                       nano::invoke(pred, pivot));
                ++it;
                offsets_l[num_l] = i++;
                num_l += !nano::invoke(comp, nano::invoke(pred, *it),
                                       nano::invoke(pred, pivot));
                ++it;
            }
        }
        if (num_r == 0) {
            start_r = 0;
            I it = last;
            for (unsigned char i = 0; i < pdqsort_block_size;) {
                offsets_r[num_r] = ++i;
                num_r += nano::invoke(comp, nano::invoke(pred, *--it),
                                      nano::invoke(pred, pivot));
                offsets_r[num_r] = ++i;
                num_r += nano::invoke(comp, nano::invoke(pred, *--it),
                                      nano::invoke(pred, pivot));
                offsets_r[num_r] = ++i;
                num_r += nano::invoke(comp, nano::invoke(pred, *--it),
                                      nano::invoke(pred, pivot));
                offsets_r[num_r] = ++i;
                num_r += nano::invoke(comp, nano::invoke(pred, *--it),
                                      nano::invoke(pred, pivot));
                offsets_r[num_r] = ++i;
                num_r += nano::invoke(comp, nano::invoke(pred, *--it),
                                      nano::invoke(pred, pivot));
                offsets_r[num_r] = ++i;
                num_r += nano::invoke(comp, nano::invoke(pred, *--it),
                                      nano::invoke(pred, pivot));
                offsets_r[num_r] = ++i;
                num_r += nano::invoke(comp, nano::invoke(pred, *--it),
                                      nano::invoke(pred, pivot));
                offsets_r[num_r] = ++i;
                num_r += nano::invoke(comp, nano::invoke(pred, *--it),
                                      nano::invoke(pred, pivot));
            }
        }

        // Swap elements and update block sizes and first/last boundaries.
        int num = (nano::min)(num_l, num_r);
        swap_offsets(first, last, offsets_l + start_l, offsets_r + start_r, num,
                     num_l == num_r);
        num_l -= num;
        num_r -= num;
        start_l += num;
        start_r += num;
        if (num_l == 0)
            first += pdqsort_block_size;
        if (num_r == 0)
            last -= pdqsort_block_size;
    }

    iter_difference_t<I> l_size = 0, r_size = 0;
    iter_difference_t<I> unknown_left =
        (last - first) - ((num_r || num_l) ? pdqsort_block_size : 0);
    if (num_r) {
        // Handle leftover block by assigning the unknown elements to the other
        // block.
        l_size = unknown_left;
        r_size = pdqsort_block_size;
    } else if (num_l) {
        l_size = pdqsort_block_size;
        r_size = unknown_left;
    } else {
        // No leftover block, split the unknown elements in two blocks.
        l_size = unknown_left / 2;
        r_size = unknown_left - l_size;
    }

    // Fill offset buffers if needed.
    if (unknown_left && !num_l) {
        start_l = 0;
        I it = first;
        for (unsigned char i = 0; i < l_size;) {
            offsets_l[num_l] = i++;
            num_l += !nano::invoke(comp, nano::invoke(pred, *it),
                                   nano::invoke(pred, pivot));
            ++it;
        }
    }
    if (unknown_left && !num_r) {
        start_r = 0;
        I it = last;
        for (unsigned char i = 0; i < r_size;) {
            offsets_r[num_r] = ++i;
            num_r += nano::invoke(comp, nano::invoke(pred, *--it),
                                  nano::invoke(pred, pivot));
        }
    }

    int num = (nano::min)(num_l, num_r);
    swap_offsets(first, last, offsets_l + start_l, offsets_r + start_r, num,
                 num_l == num_r);
    num_l -= num;
    num_r -= num;
    start_l += num;
    start_r += num;
    if (num_l == 0)
        first += l_size;
    if (num_r == 0)
        last -= r_size;

    // We have now fully identified [first, last)'s proper position. Swap the
    // last elements.
    if (num_l) {
        offsets_l += start_l;
        while (num_l--)
            nano::iter_swap(first + offsets_l[num_l], --last);
        first = last;
    }
    if (num_r) {
        offsets_r += start_r;
        while (num_r--)
            nano::iter_swap(last - offsets_r[num_r], first), ++first;
        last = first;
    }

    // Put the pivot in the right place.
    I pivot_pos = first - 1;
    *begin = nano::iter_move(pivot_pos);
    *pivot_pos = std::move(pivot);

    return std::make_pair(std::move(pivot_pos), already_partitioned);
}

// Partitions [begin, end) around pivot *begin using comparison function comp.
// Elements equal to the pivot are put in the right-hand partition. Returns the
// position of the pivot after partitioning and whether the passed sequence
// already was correctly partitioned. Assumes the pivot is a median of at least
// 3 elements and that [begin, end) is at least insertion_sort_threshold long.
template <typename I, typename Comp, typename Proj>
constexpr std::pair<I, bool> partition_right(I begin, I end, Comp& comp,
                                             Proj& proj)
{
    using T = iter_value_t<I>;

    // Move pivot into local for speed.
    T pivot(nano::iter_move(begin));

    I first = begin;
    I last = end;

    // Find the first element greater than or equal than the pivot (the median
    // of 3 guarantees this exists).
    while (nano::invoke(comp, nano::invoke(proj, *++first),
                        nano::invoke(proj, pivot))) {
    }

    // Find the first element strictly smaller than the pivot. We have to guard
    // this search if there was no element before *first.
    if (first - 1 == begin) {
        while (first < last && !nano::invoke(comp, nano::invoke(proj, *--last),
                                             nano::invoke(proj, pivot))) {
        }
    } else {
        while (!nano::invoke(comp, nano::invoke(proj, *--last),
                             nano::invoke(proj, pivot))) {
        }
    }

    // If the first pair of elements that should be swapped to partition are the
    // same element, the passed in sequence already was correctly partitioned.
    bool already_partitioned = first >= last;

    // Keep swapping pairs of elements that are on the wrong side of the pivot.
    // Previously swapped pairs guard the searches, which is why the first
    // iteration is special-cased above.
    while (first < last) {
        nano::iter_swap(first, last);
        while (nano::invoke(comp, nano::invoke(proj, *++first),
                            nano::invoke(proj, pivot)))
            ;
        while (!nano::invoke(comp, nano::invoke(proj, *--last),
                             nano::invoke(proj, pivot)))
            ;
    }

    // Put the pivot in the right place.
    I pivot_pos = first - 1;
    *begin = nano::iter_move(pivot_pos);
    *pivot_pos = std::move(pivot);

    return std::make_pair(std::move(pivot_pos), already_partitioned);
}

// Similar function to the one above, except elements equal to the pivot are put
// to the left of the pivot and it doesn't check or return if the passed
// sequence already was partitioned. Since this is rarely used (the many equal
// case), and in that case pdqsort already has O(n) performance, no block
// quicksort is applied here for simplicity.
template <typename I, typename Comp, typename Proj>
constexpr I partition_left(I begin, I end, Comp& comp, Proj& proj)
{
    using T = iter_value_t<I>;

    T pivot(nano::iter_move(begin));
    I first = begin;
    I last = end;

    while (nano::invoke(comp, nano::invoke(proj, pivot),
                        nano::invoke(proj, *--last)))
        ;

    if (last + 1 == end) {
        while (first < last && !nano::invoke(comp, nano::invoke(proj, pivot),
                                             nano::invoke(proj, *++first)))
            ;
    } else {
        while (!nano::invoke(comp, nano::invoke(proj, pivot),
                             nano::invoke(proj, *++first)))
            ;
    }

    while (first < last) {
        nano::iter_swap(first, last);
        while (nano::invoke(comp, nano::invoke(proj, pivot),
                            nano::invoke(proj, *--last)))
            ;
        while (!nano::invoke(comp, nano::invoke(proj, pivot),
                             nano::invoke(proj, *++first)))
            ;
    }

    I pivot_pos = last;
    *begin = nano::iter_move(pivot_pos);
    *pivot_pos = std::move(pivot);

    return pivot_pos;
}

template <bool Branchless, typename I, typename Comp, typename Proj>
constexpr void pdqsort_loop(I begin, I end, Comp& comp, Proj& proj,
                            int bad_allowed, bool leftmost = true)
{
    using diff_t = iter_difference_t<I>;

    // Use a while loop for tail recursion elimination.
    while (true) {
        diff_t size = nano::distance(begin, end);

        // Insertion sort is faster for small arrays.
        if (size < pdqsort_insertion_sort_threshold) {
            if (leftmost) {
                insertion_sort(begin, end, comp, proj);
            } else {
                unguarded_insertion_sort(begin, end, comp, proj);
            }
            return;
        }

        // Choose pivot as median of 3 or pseudomedian of 9.
        diff_t s2 = size / 2;
        if (size > pdqsort_ninther_threshold) {
            sort3(begin, begin + s2, end - 1, comp, proj);
            sort3(begin + 1, begin + (s2 - 1), end - 2, comp, proj);
            sort3(begin + 2, begin + (s2 + 1), end - 3, comp, proj);
            sort3(begin + (s2 - 1), begin + s2, begin + (s2 + 1), comp, proj);
            nano::iter_swap(begin, begin + s2);
        } else {
            sort3(begin + s2, begin, end - 1, comp, proj);
        }

        // If *(begin - 1) is the end of the right partition of a previous
        // partition operation there is no element in [begin, end) that is
        // smaller than *(begin - 1). Then if our pivot compares equal to
        // *(begin - 1) we change strategy, putting equal elements in the left
        // partition, greater elements in the right partition. We do not have to
        // recurse on the left partition, since it's sorted (all equal).
        if (!leftmost && !nano::invoke(comp, nano::invoke(proj, *(begin - 1)),
                                       nano::invoke(proj, *begin))) {
            begin = partition_left(begin, end, comp, proj) + 1;
            continue;
        }

        // Partition and get results.
        std::pair<I, bool> part_result =
            Branchless ? partition_right_branchless(begin, end, comp, proj)
                       : partition_right(begin, end, comp, proj);
        I pivot_pos = part_result.first;
        bool already_partitioned = part_result.second;

        // Check for a highly unbalanced partition.
        diff_t l_size = pivot_pos - begin;
        diff_t r_size = end - (pivot_pos + 1);
        bool highly_unbalanced = l_size < size / 8 || r_size < size / 8;

        // If we got a highly unbalanced partition we shuffle elements to break
        // many patterns.
        if (highly_unbalanced) {
            // If we had too many bad partitions, switch to heapsort to
            // guarantee O(n log n).
            if (--bad_allowed == 0) {
                nano::make_heap(begin, end, comp, proj);
                nano::sort_heap(begin, end, comp, proj);
                return;
            }

            if (l_size >= pdqsort_insertion_sort_threshold) {
                nano::iter_swap(begin, begin + l_size / 4);
                nano::iter_swap(pivot_pos - 1, pivot_pos - l_size / 4);

                if (l_size > pdqsort_ninther_threshold) {
                    nano::iter_swap(begin + 1, begin + (l_size / 4 + 1));
                    nano::iter_swap(begin + 2, begin + (l_size / 4 + 2));
                    nano::iter_swap(pivot_pos - 2,
                                    pivot_pos - (l_size / 4 + 1));
                    nano::iter_swap(pivot_pos - 3,
                                    pivot_pos - (l_size / 4 + 2));
                }
            }

            if (r_size >= pdqsort_insertion_sort_threshold) {
                nano::iter_swap(pivot_pos + 1, pivot_pos + (1 + r_size / 4));
                nano::iter_swap(end - 1, end - r_size / 4);

                if (r_size > pdqsort_ninther_threshold) {
                    nano::iter_swap(pivot_pos + 2,
                                    pivot_pos + (2 + r_size / 4));
                    nano::iter_swap(pivot_pos + 3,
                                    pivot_pos + (3 + r_size / 4));
                    nano::iter_swap(end - 2, end - (1 + r_size / 4));
                    nano::iter_swap(end - 3, end - (2 + r_size / 4));
                }
            }
        } else {
            // If we were decently balanced and we tried to sort an already
            // partitioned sequence try to use insertion sort.
            if (already_partitioned &&
                partial_insertion_sort(begin, pivot_pos, comp, proj) &&
                partial_insertion_sort(pivot_pos + 1, end, comp, proj))
                return;
        }

        // Sort the left partition first using recursion and do tail recursion
        // elimination for the right-hand partition.
        detail::pdqsort_loop<Branchless>(begin, pivot_pos, comp, proj,
                                         bad_allowed, leftmost);
        begin = pivot_pos + 1;
        leftmost = false;
    }
}

template <typename I, typename Comp, typename Proj,
          bool Branchless = is_default_compare_v<std::remove_const_t<Comp>>&&
              same_as<Proj, identity>&& std::is_arithmetic<iter_value_t<I>>::value>
constexpr void pdqsort(I begin, I end, Comp& comp, Proj& proj)
{
    if (begin == end) {
        return;
    }

    detail::pdqsort_loop<Branchless>(std::move(begin), std::move(end), comp,
                                     proj,
                                     detail::log2(nano::distance(begin, end)));
}

} // namespace detail

NANO_END_NAMESPACE

#endif

NANO_BEGIN_NAMESPACE

namespace detail {

struct sort_fn {
    template <typename I, typename S, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                                   sortable<I, Comp, Proj>, I>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        I last_it = nano::next(first, last);
        detail::pdqsort(std::move(first), last_it, comp, proj);
        return last_it;
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    constexpr std::enable_if_t<random_access_range<Rng> &&
                                   sortable<iterator_t<Rng>, Comp, Proj>,
                               borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        iterator_t<Rng> last_it = nano::next(nano::begin(rng), nano::end(rng));
        detail::pdqsort(nano::begin(rng), last_it, comp, proj);
        return last_it;
    }
};

}

NANO_INLINE_VAR(detail::sort_fn, sort)

NANO_END_NAMESPACE

#endif


// nanorange/algorithm/stl/stable_partition.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//===-------------------------- algorithm ---------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef NANORANGE_ALGORITHM_STABLE_PARTITION_HPP_INCLUDED
#define NANORANGE_ALGORITHM_STABLE_PARTITION_HPP_INCLUDED












NANO_BEGIN_NAMESPACE

namespace detail {

struct stable_partition_fn {
private:
    template <typename I, typename Buf, typename Pred, typename Proj>
    static subrange<I> impl_buffered(I first, I last, Buf& buf, Pred& pred, Proj& proj)
    {
        // first is known to be false, so pop it straight into the buffer
        buf.push_back(nano::iter_move(first));

        const auto res = nano::partition_copy(
                nano::make_move_iterator(nano::next(first)),
                nano::make_move_sentinel(--last),
                first, nano::back_inserter(buf),
                std::ref(pred), std::ref(proj));

        // last is known to be true, move that to the correct pos
        first = std::move(res.out1);
        *first = nano::iter_move(last);
        ++first;

        // Now move all the other elements from the buffer back into the sequence
        nano::move(buf, first);
        return {std::move(first), std::move(last)};
    }

    // Note to self: this is a closed range, last is NOT past-the-end!
    template <typename I, typename Pred, typename Proj>
    static subrange<I> impl_unbuffered(I first, I last, iter_difference_t<I> dist,
                             Pred& pred, Proj& proj)
    {
        using dist_t = iter_difference_t<I>;

        if (dist == 2) {
            // We know first is false and last is true, so swap them
            nano::iter_swap(first, last);
            return {nano::next(first), nano::next(last)};
        }

        if (dist == 3) {
            // We know first is false and last is true, so look at middle
            I middle = nano::next(first);

            if (nano::invoke(pred, nano::invoke(proj, *middle))) {
                nano::iter_swap(first, middle);
                nano::iter_swap(middle, last);
                return {nano::next(first, 2), nano::next(last)};
            }

            // middle is false
            nano::iter_swap(middle, last);
            nano::iter_swap(first, middle);
            return {std::move(middle), std::next(last)};
        }

        const dist_t half = dist/2;
        const I middle = nano::next(first, half);

        I m1 = nano::prev(middle);
        dist_t len_half = half;

        while (m1 != first && !nano::invoke(pred, nano::invoke(proj, *m1))) {
            --len_half;
            --m1;
        }

        const I first_false = (m1 == first) ? first :
                impl_unbuffered(first, m1, len_half, pred, proj).begin();

        m1 = middle;
        len_half = dist - half;

        while (nano::invoke(pred, nano::invoke(proj, *m1))) {
            if (++m1 == last) {
                auto rot = nano::rotate(first_false, middle, ++last);
                return {std::move(rot.begin()), nano::next(last)};
            }
        }

        const I last_false = impl_unbuffered(m1, last, len_half, pred, proj).begin();

        auto rot = nano::rotate(first_false, middle, last_false);
        return {rot.begin(), nano::next(last)};
    }

    template <typename I, typename Pred, typename Proj>
    static subrange<I> impl(I first, I last, Pred& pred, Proj& proj)
    {
        // Find the first non-true value
        first = nano::find_if_not(std::move(first), last, std::ref(pred), std::ref(proj));
        if (first == last) {
            return {std::move(first), std::move(last)};
        }

        // Find the last true value
        I it = nano::find_if(nano::make_reverse_iterator(last),
                             nano::make_reverse_iterator(first),
                             std::ref(pred), std::ref(proj)).base();
        if (it == first) {
            return {std::move(first), std::move(it)};
        }

        const auto dist = nano::distance(first, it);

        auto buf = detail::temporary_vector<iter_value_t<I>>(dist);
        if (buf.capacity() < static_cast<std::size_t>(dist)) {
            return {impl_unbuffered(first, --it, dist, pred, proj).begin(), last};
        }
        return {impl_buffered(first, it, buf, pred, proj).begin(), last};
    }

    template <typename I, typename S, typename Pred, typename Proj>
    static std::enable_if_t<!same_as<I, S>, subrange<I>>
    impl(I first, S last, Pred& pred, Proj& proj)
    {
        return impl(first, nano::next(first, last), pred, proj);
    }

public:
    template <typename I, typename S, typename Pred, typename Proj = identity>
    std::enable_if_t<bidirectional_iterator<I> && sentinel_for<S, I> &&
                         indirect_unary_predicate<Pred, projected<I, Proj>> &&
                         permutable<I>, subrange<I>>
    operator()(I first, S last, Pred pred, Proj proj = Proj{}) const
    {
        return stable_partition_fn::impl(std::move(first), std::move(last),
                                         pred, proj);
    }

    template <typename Rng, typename Pred, typename Proj = identity>
    std::enable_if_t<
        bidirectional_range<Rng> &&
            indirect_unary_predicate<Pred, projected<iterator_t<Rng>, Proj>> &&
            permutable<iterator_t<Rng>>,
        borrowed_subrange_t<Rng>>
    operator()(Rng&& rng, Pred pred, Proj proj = Proj{}) const
    {
        return stable_partition_fn::impl(nano::begin(rng), nano::end(rng),
                                         pred, proj);
    }
};

}

NANO_INLINE_VAR(detail::stable_partition_fn, stable_partition)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/stable_sort.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Uses code from cmcstl2 - A concept-enabled C++ standard library
//
//  Copyright Eric Niebler 2014
//  Copyright Casey Carter 2015
//

//
//  Copyright (c) 1994
//  Hewlett-Packard Company
//
//  Permission to use, copy, modify, distribute and sell this software
//  and its documentation for any purpose is hereby granted without fee,
//  provided that the above copyright notice appear in all copies and
//  that both that copyright notice and this permission notice appear
//  in supporting documentation.  Hewlett-Packard Company makes no
//  representations about the suitability of this software for any
//  purpose.  It is provided "as is" without express or implied warranty.
//
//  Copyright (c) 1996
//  Silicon Graphics Computer Systems, Inc.
//
//  Permission to use, copy, modify, distribute and sell this software
//  and its documentation for any purpose is hereby granted without fee,
//  provided that the above copyright notice appear in all copies and
//  that both that copyright notice and this permission notice appear
//  in supporting documentation.  Silicon Graphics makes no
//  representations about the suitability of this software for any
//  purpose.  It is provided "as is" without express or implied warranty.


#ifndef NANORANGE_ALGORITHM_STABLE_SORT_HPP_INCLUDED
#define NANORANGE_ALGORITHM_STABLE_SORT_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

namespace detail {

struct stable_sort_fn {
private:
    static constexpr int merge_sort_chunk_size = 7;

    template <typename I, typename Comp, typename Proj>
    static void inplace_stable_sort(I first, I last, Comp& comp, Proj& proj)
    {
        if (last - first < 15) {
            detail::insertion_sort(std::move(first), std::move(last), comp, proj);
        } else {
            I middle = first + iter_difference_t<I>(last - first) / 2;
            inplace_stable_sort(first, middle, comp, proj);
            inplace_stable_sort(middle, last, comp, proj);
            inplace_merge_fn::impl_slow(first, middle, last,
                                        middle - first, last - middle,
                                        comp, proj);
        }
    }

    template <typename I, typename Buf, typename Comp, typename Proj>
    static void stable_sort_adaptive(I first, I last, Buf& buf, Comp& comp, Proj& proj)
    {
        auto len = iter_difference_t<I>((last - first + 1) / 2);
        auto middle = first + len;
        if (static_cast<std::size_t>(len) > buf.capacity()) {
            stable_sort_adaptive(first, middle, buf, comp, proj);
            stable_sort_adaptive(middle, last, buf, comp, proj);
        } else {
            merge_sort_with_buffer(first, middle, buf, comp, proj);
            merge_sort_with_buffer(middle, last, buf, comp, proj);
        }
        inplace_merge_fn::impl_buffered(first, middle, last,
                                        middle - first, last - middle, buf,
                                        comp, proj);
    }

    template <typename I, typename Buf, typename Comp, typename Proj>
    static void merge_sort_with_buffer(I first, I last, Buf& buf, Comp& comp, Proj& proj)
    {
        auto len = iter_difference_t<I>(last - first);
        auto step_size = iter_difference_t<I>(merge_sort_chunk_size);
        chunk_insertion_sort(first, last, step_size, comp, proj);
        if (step_size >= len) {
            return;
        }
        assert(buf.empty());
        merge_sort_loop(first, last, nano::back_inserter(buf), step_size, comp, proj);
        step_size *= 2;
        while (true) {
            merge_sort_loop(buf.begin(), buf.end(), first, step_size, comp, proj);
            step_size *= 2;
            if (step_size >= len) {
                buf.clear();
                return;
            }
            merge_sort_loop(first, last, buf.begin(), step_size, comp, proj);
            step_size *= 2;
        }
        buf.clear();
    }

    template <typename I, typename Comp, typename Proj>
    static void chunk_insertion_sort(I first, I last, iter_difference_t<I> chunk_size,
                                     Comp& comp, Proj& proj)
    {
            while (last - first >= chunk_size) {
                detail::insertion_sort(first, first + chunk_size, comp, proj);
                first += chunk_size;
            }
            detail::insertion_sort(first, last, comp, proj);
    }

    template <typename I, typename O, typename Comp, typename Proj>
    static void merge_sort_loop(I first, I last, O result, iter_difference_t<I> step_size,
                                Comp& comp, Proj& proj)
    {
        auto two_step = iter_difference_t<I>(2 * step_size);
        while (last - first >= two_step) {
            result = nano::merge(
                         nano::make_move_iterator(first),
                         nano::make_move_iterator(first + step_size),
                         nano::make_move_iterator(first + step_size),
                         nano::make_move_iterator(first + two_step),
                         result, std::ref(comp), std::ref(proj), std::ref(proj)).out;
            first += two_step;
        }
        step_size = nano::min(iter_difference_t<I>(last - first), step_size);
        nano::merge(
                nano::make_move_iterator(first),
                nano::make_move_iterator(first + step_size),
                nano::make_move_iterator(first + step_size),
                nano::make_move_iterator(last),
                result, std::ref(comp), std::ref(proj), std::ref(proj));
    }

    template <typename I, typename Comp, typename Proj>
    static void impl(I first, I last, Comp& comp, Proj& proj)
    {
        auto len = last - first;
        if (len == 0) {
            return;
        }

        temporary_vector<iter_value_t<I>> buf(len > 256 ? len : 0);

        if (buf.capacity() != 0) {
            stable_sort_adaptive(std::move(first), std::move(last), buf, comp, proj);
        } else {
            inplace_stable_sort(std::move(first), std::move(last), comp, proj);
        }
    }

public:
    template <typename I, typename S, typename Comp = ranges::less, typename Proj = identity>
    std::enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                         sortable<I, Comp, Proj>, I>
    operator()(I first, S last, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        const auto ilast = nano::next(first, last);
        impl(std::move(first), ilast, comp, proj);
        return ilast;
    }

    template <typename Rng, typename Comp = ranges::less, typename Proj = identity>
    std::enable_if_t<random_access_range<Rng> &&
                         sortable<iterator_t<Rng>, Comp, Proj>,
                     borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, Comp comp = Comp{}, Proj proj = Proj{}) const
    {
        auto first = nano::begin(rng);
        const auto last = nano::next(first, nano::end(rng));
        impl(std::move(first), last, comp, proj);
        return last;
    }
};

}

NANO_INLINE_VAR(detail::stable_sort_fn, stable_sort)

NANO_END_NAMESPACE

#endif


// nanorange/algorithm/transform.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_TRANSFORM_HPP_INCLUDED
#define NANORANGE_ALGORITHM_TRANSFORM_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I, typename O>
using unary_transform_result = in_out_result<I, O>;

template <typename I1, typename I2, typename O>
using binary_transform_result  = in_in_out_result<I1, I2, O>;

namespace detail {

struct transform_fn {
private:
    template <typename I, typename S, typename O, typename F, typename Proj>
    static constexpr unary_transform_result<I, O>
    unary_impl(I first, S last, O result, F& op, Proj& proj)
    {
        while (first != last) {
            *result = nano::invoke(op, nano::invoke(proj, *first));
            ++first;
            ++result;
        }

        return {std::move(first), std::move(result)};
    }

    template <typename I1, typename S1, typename I2, typename O, typename F,
              typename Proj1, typename Proj2>
    static constexpr binary_transform_result<I1, I2, O>
    binary_impl3(I1 first1, S1 last1, I2 first2, O result, F& op, Proj1& proj1,
                 Proj2& proj2)
    {
        while (first1 != last1) {
            *result = nano::invoke(op, nano::invoke(proj1, *first1),
                                   nano::invoke(proj2, *first2));
            ++first1;
            ++first2;
            ++result;
        }

        return {std::move(first1), std::move(first2), std::move(result)};
    }

    template <typename I1, typename S1, typename I2, typename S2, typename O,
              typename F, typename Proj1, typename Proj2>
    static constexpr binary_transform_result<I1, I2, O>
    binary_impl4(I1 first1, S1 last1, I2 first2, S2 last2, O result, F& op,
                 Proj1& proj1, Proj2& proj2)
    {
        while (first1 != last1 && first2 != last2) {
            *result = nano::invoke(op, nano::invoke(proj1, *first1),
                                   nano::invoke(proj2, *first2));
            ++first1;
            ++first2;
            ++result;
        }

        return {std::move(first1), std::move(first2), std::move(result)};
    }

public:
    // Unary op, iterators
    template <typename I, typename S, typename O, typename F,
              typename Proj = identity>
    constexpr std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> && weakly_incrementable<O> &&
            copy_constructible<F> &&
            writable<O, indirect_result_t<F&, projected<I, Proj>>>,
        unary_transform_result<I, O>>
    operator()(I first, S last, O result, F op, Proj proj = Proj{}) const
    {
        return transform_fn::unary_impl(std::move(first), std::move(last),
                                        std::move(result), op, proj);
    }

    // Unary op, range
    template <typename Rng, typename O, typename F, typename Proj = identity>
    constexpr std::enable_if_t<
        input_range<Rng> && weakly_incrementable<O> && copy_constructible<F> &&
            writable<O,
                     indirect_result_t<F&, projected<iterator_t<Rng>, Proj>>>,
        unary_transform_result<borrowed_iterator_t<Rng>, O>>
    operator()(Rng&& rng, O result, F op, Proj proj = Proj{}) const
    {
        return transform_fn::unary_impl(nano::begin(rng), nano::end(rng),
                                        std::move(result), op, proj);
    }

    // Binary op, four-legged
    template <typename I1, typename S1, typename I2, typename S2, typename O,
              typename F, typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<
        input_iterator<I1> && sentinel_for<S1, I1> && input_iterator<I2> &&
            sentinel_for<S2, I2> && weakly_incrementable<O> &&
            copy_constructible<F> &&
            writable<O, indirect_result_t<F&, projected<I1, Proj1>,
                                          projected<I2, Proj2>>>,
        binary_transform_result<I1, I2, O>>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, O result, F op,
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return transform_fn::binary_impl4(std::move(first1), std::move(last1),
                                          std::move(first2), std::move(last2),
                                          std::move(result), op, proj1, proj2);
    }

    // Binary op, two ranges
    template <typename Rng1, typename Rng2, typename O, typename F,
              typename Proj1 = identity, typename Proj2 = identity>
    constexpr std::enable_if_t<
        input_range<Rng1> && input_range<Rng2> && weakly_incrementable<O> &&
            copy_constructible<F> &&
            writable<O,
                     indirect_result_t<F&, projected<iterator_t<Rng1>, Proj1>,
                                       projected<iterator_t<Rng2>, Proj2>>>,
        binary_transform_result<borrowed_iterator_t<Rng1>,
                                borrowed_iterator_t<Rng2>, O>>
    operator()(Rng1&& rng1, Rng2&& rng2, O result, F op, Proj1 proj1 = Proj1{},
               Proj2 proj2 = Proj2{}) const
    {
        return transform_fn::binary_impl4(nano::begin(rng1), nano::end(rng1),
                                          nano::begin(rng2), nano::end(rng2),
                                          std::move(result), op, proj1, proj2);
    }

    // Binary op, three-legged
    template <typename I1, typename S1, typename I2, typename O, typename F,
              typename Proj1 = identity, typename Proj2 = identity>
    NANO_DEPRECATED constexpr std::enable_if_t<
        input_iterator<I1> && sentinel_for<S1, I1> &&
            input_iterator<std::decay_t<I2>> &&
            !input_range<I2> &&
            weakly_incrementable<O> && copy_constructible<F> &&
            writable<O, indirect_result_t<F&, projected<I1, Proj1>,
                                          projected<std::decay_t<I2>, Proj2>>>,
        binary_transform_result<I1, std::decay_t<I2>, O>>
    operator()(I1 first1, S1 last1, I2&& first2, O result, F op,
               Proj1 proj1 = Proj1{}, Proj2 proj2 = Proj2{}) const
    {
        return transform_fn::binary_impl3(std::move(first1), std::move(last1),
                                          std::forward<I2>(first2), std::move(result),
                                          op, proj1, proj2);
    }

    // binary op, range-and-a-half
    template <typename Rng1, typename I2, typename O, typename F,
              typename Proj1 = identity, typename Proj2 = identity>
    NANO_DEPRECATED constexpr std::enable_if_t<
        input_range<Rng1> && input_iterator<std::decay_t<I2>> &&
                !input_range<I2> && weakly_incrementable<O> &&
            copy_constructible<F> &&
            writable<O,
                     indirect_result_t<F&, projected<iterator_t<Rng1>, Proj1>,
                                       projected<std::decay_t<I2>, Proj2>>>,
        binary_transform_result<borrowed_iterator_t<Rng1>, std::decay_t<I2>, O>>
    operator()(Rng1&& rng1, I2&& first2, O result, F op, Proj1 proj1 = Proj1{},
               Proj2 proj2 = Proj2{}) const
    {
        return transform_fn::binary_impl3(nano::begin(rng1), nano::end(rng1),
                                          std::forward<I2>(first2), std::move(result),
                                          op, proj1, proj2);
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::transform_fn, transform)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/unique.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_UNIQUE_HPP_INCLUDED
#define NANORANGE_ALGORITHM_UNIQUE_HPP_INCLUDED






NANO_BEGIN_NAMESPACE

namespace detail {

struct unique_fn {
private:
    template <typename I, typename S, typename R, typename Proj>
    static constexpr subrange<I> impl(I first, S last, R& comp, Proj& proj)
    {
        I it = adjacent_find_fn::impl(first, last, comp, proj);

        if (it == last) {
            return {it, std::move(it)};
        }

        I n = nano::next(it, 2, last);
        for (; n != last; ++n) {
            if (!nano::invoke(comp, nano::invoke(proj, *it),
                              nano::invoke(proj, *n))) {
                *++it = iter_move(n);
            }
        }

        return {nano::next(it), std::move(n)};
    }

public:
    template <typename I, typename S, typename R = ranges::equal_to,
              typename Proj = identity>
    constexpr std::enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                                   indirect_relation<R, projected<I, Proj>> &&
                                   permutable<I>, subrange<I>>
    operator()(I first, S last, R comp = {}, Proj proj = Proj{}) const
    {
        return unique_fn::impl(std::move(first), std::move(last),
                               comp, proj);
    }

    template <typename Rng, typename R = ranges::equal_to, typename Proj = identity>
    constexpr std::enable_if_t<
        forward_range<Rng> &&
            indirect_relation<R, projected<iterator_t<Rng>, Proj>> &&
            permutable<iterator_t<Rng>>,
        borrowed_subrange_t<Rng>>
    operator()(Rng&& rng, R comp = {}, Proj proj = Proj{}) const
    {
        return unique_fn::impl(nano::begin(rng), nano::end(rng),
                               comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::unique_fn, unique)

NANO_END_NAMESPACE

#endif

// nanorange/algorithm/unique_copy.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ALGORITHM_UNIQUE_COPY_HPP_INCLUDED
#define NANORANGE_ALGORITHM_UNIQUE_COPY_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I, typename O>
using unique_copy_result = in_out_result<I, O>;

namespace detail {

struct unique_copy_fn {
private:
    template <typename I, typename S, typename O,
              typename Comp, typename Proj>
    static constexpr unique_copy_result<I, O>
    impl(I first, S last, O result, Comp& comp, Proj& proj)
    {
        if (first != last) {
            iter_value_t<I> saved = *first;
            *result = saved;
            ++result;

            while (++first != last) {
                auto&& v = *first;
                if (!nano::invoke(comp, nano::invoke(proj, v), nano::invoke(proj, saved))) {
                    saved = std::forward<decltype(v)>(v);
                    *result = saved;
                    ++result;
                }
            }
        }

        return {std::move(first), std::move(result)};
    }

    template <typename I, typename O>
    static auto constraint_helper(priority_tag<2>) -> std::enable_if_t<forward_iterator<I>, std::true_type>;

    template <typename I, typename O>
    static auto constraint_helper(priority_tag<1>) -> std::enable_if_t<input_iterator<O> && same_as<iter_value_t<I>, iter_value_t<O>>,
        std::true_type>;

    template <typename I, typename O>
    static auto constraint_helper(priority_tag<0>) -> std::enable_if_t<indirectly_copyable_storable<I, O>, std::true_type>;

public:
    template <typename I, typename S, typename O, typename Comp = ranges::equal_to,
              typename Proj = identity>
    constexpr auto operator()(I first, S last, O result,
                              Comp comp = Comp{}, Proj proj = Proj{}) const
        -> std::enable_if_t<
            input_iterator<I> && sentinel_for<S, I> &&
               weakly_incrementable<O> &&
                indirect_relation<Comp, projected<I, Proj>> &&
                indirectly_copyable<I, O> &&
               decltype(constraint_helper<I, O>(priority_tag<2>{}))::value,
        unique_copy_result<I, O>>
    {
        return unique_copy_fn::impl(std::move(first), std::move(last),
                                    std::move(result), comp, proj);
    }

    template <typename Rng, typename O, typename Comp = ranges::equal_to,
              typename Proj = identity>
    constexpr auto
    operator()(Rng&& rng, O result, Comp comp = Comp{}, Proj proj = Proj{}) const
    -> std::enable_if_t<
            input_range<Rng> &&
            weakly_incrementable<O> &&
                indirect_relation<Comp, projected<iterator_t<Rng>, Proj>> &&
                indirectly_copyable<iterator_t<Rng>, O> &&
            decltype(constraint_helper<iterator_t<Rng>, O>(priority_tag<2>{}))::value,
       unique_copy_result<borrowed_iterator_t<Rng>, O>>
    {
        return unique_copy_fn::impl(nano::begin(rng), nano::end(rng),
                                    std::move(result), comp, proj);
    }
};

}

NANO_INLINE_VAR(detail::unique_copy_fn, unique_copy)

NANO_END_NAMESPACE

#endif



#endif



// nanorange/iterator.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_HPP_INCLUDED
#define NANORANGE_ITERATOR_HPP_INCLUDED













// nanorange/iterator/front_insert_iterator.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_FRONT_INSERT_ITERATOR_HPP_INCLUDED
#define NANORANGE_ITERATOR_FRONT_INSERT_ITERATOR_HPP_INCLUDED



#include <iterator>

NANO_BEGIN_NAMESPACE

template <typename Container>
struct front_insert_iterator {
    using container_type = Container;
    using difference_type = std::ptrdiff_t;

    constexpr front_insert_iterator() = default;

    explicit front_insert_iterator(Container& x) : cont_(std::addressof(x)) {}

    front_insert_iterator& operator=(const iter_value_t<Container>& value)
    {
        cont_->push_front(value);
        return *this;
    }

    front_insert_iterator& operator=(iter_value_t<Container>&& value)
    {
        cont_->push_front(std::move(value));
        return *this;
    }

    front_insert_iterator& operator*() { return *this; }
    front_insert_iterator& operator++() { return *this; }
    front_insert_iterator& operator++(int) { return *this; }

private:
    container_type* cont_ = nullptr;
};

template <typename Container>
front_insert_iterator<Container> front_inserter(Container& x)
{
    return front_insert_iterator<Container>(x);
}

NANO_END_NAMESPACE

namespace std {

template <typename Cont>
struct iterator_traits<::nano::front_insert_iterator<Cont>> {
    using value_type = void;
    using difference_type = ptrdiff_t;
    using reference = void;
    using pointer = void;
    using iterator_category = output_iterator_tag;
};

} // namespace std

#endif

// nanorange/iterator/insert_iterator.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_INSERT_ITERATOR_HPP_INCLUDED
#define NANORANGE_ITERATOR_INSERT_ITERATOR_HPP_INCLUDED



#include <iterator>

NANO_BEGIN_NAMESPACE

template <typename Container>
struct insert_iterator {
    using container_type = Container;
    using difference_type = std::ptrdiff_t;

    constexpr insert_iterator() = default;

    explicit insert_iterator(Container& x, iterator_t<Container> i)
        : cont_(std::addressof(x)), it_(i)
    {}

    insert_iterator& operator=(const iter_value_t<Container>& value)
    {
        cont_->insert(it_, value);
        ++it_;
        return *this;
    }

    insert_iterator& operator=(iter_value_t<Container>&& value)
    {
        cont_->push_back(it_, std::move(value));
        ++it_;
        return *this;
    }

    insert_iterator& operator*() { return *this; }
    insert_iterator& operator++() { return *this; }
    insert_iterator& operator++(int) { return *this; }

private:
    container_type* cont_ = nullptr;
    iterator_t<container_type> it_{};
};

template <typename Container>
insert_iterator<Container> inserter(Container& x)
{
    return back_insert_iterator<Container>(x);
}

NANO_END_NAMESPACE

namespace std {

template <typename Container>
struct iterator_traits<::nano::insert_iterator<Container>> {
    using value_type = void;
    using difference_type = ptrdiff_t;
    using reference = void;
    using pointer = void;
    using iterator_category = output_iterator_tag;
};

} // namespace std

#endif
// nanorange/iterator/istream_iterator.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_ISTREAM_ITERATOR_HPP_INCLUDED
#define NANORANGE_ITERATOR_ISTREAM_ITERATOR_HPP_INCLUDED




#include <iosfwd>

NANO_BEGIN_NAMESPACE

template <typename T, typename CharT = char, typename Traits = std::char_traits<CharT>,
          typename Distance = std::ptrdiff_t>
class istream_iterator {
public:
    using iterator_category = input_iterator_tag;
    using difference_type = Distance;
    using value_type = T;
    using reference = const T&;
    using pointer = const T*;
    using char_type = CharT;
    using traits_type = Traits;
    using istream_type = std::basic_istream<CharT, Traits>;

    constexpr istream_iterator() = default;

    constexpr istream_iterator(default_sentinel_t) {}

    istream_iterator(istream_type& s)
        : in_stream_(std::addressof(s))
    {
        s >> value_;
    }

    istream_iterator(const istream_iterator& x) = default;

    ~istream_iterator() = default;

    const T& operator*() const { return value_; }

    const T* operator->() const { return std::addressof(value_); }

    istream_iterator& operator++()
    {
        *in_stream_ >> value_;
        if (in_stream_->fail()) {
            in_stream_ = nullptr;
        }
        return *this;
    }

    istream_iterator operator++(int)
    {
        istream_iterator tmp = *this;
        this->operator++();
        return tmp;
    }

    friend bool operator==(const istream_iterator& x, const istream_iterator& y)
    {
        return x.in_stream_ == y.in_stream_;
    }

    friend bool operator==(default_sentinel_t, const istream_iterator y)
    {
        return nullptr == y.in_stream_;
    }

    friend bool operator==(const istream_iterator& x, default_sentinel_t)
    {
        return x.in_stream_ == nullptr;
    }

    friend bool operator!=(const istream_iterator& x, const istream_iterator& y)
    {
        return !(x == y);
    }

    friend bool operator!=(default_sentinel_t x, const istream_iterator y)
    {
        return !(x == y);
    }

    friend bool operator!=(const istream_iterator& x, default_sentinel_t y)
    {
        return !(x == y);
    }

private:
    istream_type* in_stream_ = nullptr;
    T value_{};
};

NANO_END_NAMESPACE

#endif

// nanorange/iterator/istreambuf_iterator.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_ISTREAMBUF_ITERATOR_HPP_INCLUDED
#define NANORANGE_ITERATOR_ISTREAMBUF_ITERATOR_HPP_INCLUDED




#include <iosfwd>

NANO_BEGIN_NAMESPACE

template <typename CharT, typename Traits = std::char_traits<CharT>>
class istreambuf_iterator {
    class proxy {
        friend class istreambuf_iterator;
        CharT keep_;
        std::basic_streambuf<CharT, Traits>* sbuf_;

        proxy(CharT c, std::basic_streambuf<CharT, Traits>* sbuf)
            : keep_(c), sbuf_(sbuf)
        {}
    public:
        CharT operator*() const { return keep_; }
    };

public:
    using iterator_category = input_iterator_tag;
    using value_type = CharT;
    using difference_type = typename Traits::off_type;
    using reference = CharT;
    using pointer = CharT*;
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using streambuf_type = std::basic_streambuf<CharT, Traits>;
    using istream_type = std::basic_istream<CharT, Traits>;

    constexpr istreambuf_iterator() noexcept = default;

    constexpr istreambuf_iterator(default_sentinel_t) noexcept {}

    istreambuf_iterator(const istreambuf_iterator&) noexcept = default;

    ~istreambuf_iterator() = default;

    istreambuf_iterator(istream_type& s) noexcept
        : sbuf_(s.rdbuf())
    {}

    istreambuf_iterator(streambuf_type* s) noexcept
        : sbuf_(s)
    {}

    istreambuf_iterator(const proxy& p) noexcept
        : sbuf_(p.sbuf_)
    {}

    char_type operator*() const { return Traits::to_char_type(sbuf_->sgetc()); }

    istreambuf_iterator& operator++()
    {
        sbuf_->sbumpc();
        return *this;
    }

    proxy operator++(int)
    {
        return proxy(Traits::to_char_type(sbuf_->sbumpc()), sbuf_);
    }

    bool equal(const istreambuf_iterator& b) const
    {
        return  is_eof() == b.is_eof();
    }

private:
    bool is_eof() const
    {
        if (sbuf_ && sbuf_->sgetc() == Traits::eof()) {
            sbuf_ = nullptr;
            return true;
        }

        return sbuf_ == nullptr;
    }

    mutable streambuf_type* sbuf_ = nullptr;
};

template <typename CharT, typename Traits>
bool operator==(const istreambuf_iterator<CharT, Traits>& a,
                const istreambuf_iterator<CharT, Traits>& b)
{
    return a.equal(b);
}

template <typename CharT, typename Traits>
bool operator==(default_sentinel_t,
                const istreambuf_iterator<CharT, Traits>& b)
{
    return istreambuf_iterator<CharT, Traits>{}.equal(b);
}

template <typename CharT, typename Traits>
bool operator==(const istreambuf_iterator<CharT, Traits>& a,
                default_sentinel_t)
{
    return a.equal(istreambuf_iterator<CharT, Traits>{});
}

template <typename CharT, typename Traits>
bool operator!=(const istreambuf_iterator<CharT, Traits>& a,
                const istreambuf_iterator<CharT, Traits>& b)
{
    return !(a == b);
}

template <typename CharT, typename Traits>
bool operator!=(default_sentinel_t a,
                const istreambuf_iterator<CharT, Traits>& b)
{
    return !(a == b);
}

template <typename CharT, typename Traits>
bool operator!=(const istreambuf_iterator<CharT, Traits>& a,
                default_sentinel_t b)
{
    return !(a == b);
}

NANO_END_NAMESPACE

#endif



// nanorange/iterator/ostream_iterator.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_OSTREAM_ITERATOR_HPP_INCLUDED
#define NANORANGE_ITERATOR_OSTREAM_ITERATOR_HPP_INCLUDED



#include <iosfwd>
#include <iterator>
#include <memory>

NANO_BEGIN_NAMESPACE

template <typename T, typename CharT = char, typename Traits = std::char_traits<CharT>>
struct ostream_iterator
{
    using char_type = CharT;
    using traits_type = Traits;
    using ostream_type = std::basic_ostream<CharT, Traits>;
    using difference_type = std::ptrdiff_t;

    constexpr ostream_iterator() noexcept = default;

    ostream_iterator(ostream_type& os, const CharT* delim = nullptr) noexcept
            : os_(std::addressof(os)), delim_(delim)
    {}

    ostream_iterator& operator=(const T& value)
    {
        *os_ << value;
        if (delim_) {
            *os_ << delim_;
        }
        return *this;
    }

    ostream_iterator& operator*() { return *this; }
    ostream_iterator& operator++() { return *this; }
    ostream_iterator& operator++(int) { return *this; }

private:
    ostream_type* os_ = nullptr;
    const char_type* delim_ = nullptr;
};

NANO_END_NAMESPACE

namespace std {

template <typename T, typename C, typename Tr>
struct iterator_traits<::nano::ranges::ostream_iterator<T, C, Tr>> {
    using value_type = void;
    using difference_type = ptrdiff_t;
    using reference = void;
    using pointer = void;
    using iterator_category = std::output_iterator_tag;
};

}

#endif
// nanorange/iterator/ostreambuf_iterator.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_ITERATOR_OSTREAMBUF_ITERATOR_HPP_INCLUDED
#define NANORANGE_ITERATOR_OSTREAMBUF_ITERATOR_HPP_INCLUDED



#include <iosfwd> // for basic_streambuf
#include <iterator>

NANO_BEGIN_NAMESPACE

template <typename CharT, typename Traits = std::char_traits<CharT>>
struct ostreambuf_iterator {

    using char_type = CharT;
    using traits = Traits;
    using difference_type = std::ptrdiff_t;
    using streambuf_type = std::basic_streambuf<CharT, Traits>;
    using ostream_type = std::basic_ostream<CharT, Traits>;

    constexpr ostreambuf_iterator() = default;

    ostreambuf_iterator(ostream_type& s) noexcept : sbuf_(s.rdbuf()) {}

    ostreambuf_iterator(streambuf_type* s) noexcept : sbuf_(s) {}

    ostreambuf_iterator& operator=(char_type c)
    {
        if (!failed()) {
            failed_ = (sbuf_->sputc(c) == traits::eof());
        }
        return *this;
    }

    ostreambuf_iterator& operator*() { return *this; }
    ostreambuf_iterator& operator++() { return *this; }
    ostreambuf_iterator& operator++(int) { return *this; }

    bool failed() const noexcept { return failed_; }

private:
    streambuf_type* sbuf_ = nullptr;
    bool failed_ = false;
};

NANO_END_NAMESPACE

namespace std {

template <typename C, typename T>
struct iterator_traits<::nano::ranges::ostreambuf_iterator<C, T>> {
    using value_type = void;
    using difference_type = ptrdiff_t;
    using reference = void;
    using pointer = void;
    using iterator_category = output_iterator_tag;
};

} // namespace std

#endif




#endif

// nanorange/memory.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_MEMORY_HPP_INCLUDED


// nanorange/memory/uninitialized_copy.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_MEMORY_UNINITIALIZED_COPY_HPP_INCLUDED
#define NANORANGE_MEMORY_UNINITIALIZED_COPY_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I, typename O>
using uninitialized_copy_result = in_out_result<I, O>;

namespace detail {

struct uninitialized_copy_fn {
private:
    friend struct uninitialized_copy_n_fn;

    template <typename I, typename S, typename O, typename S2>
    static uninitialized_copy_result<I, O>
    impl4(I ifirst, S ilast, O ofirst, S2 olast)
    {
        O oit = ofirst;
        try {
            for (; ifirst != ilast && oit != olast; ++ifirst, (void) ++oit) {
                ::new(detail::voidify(*oit))
                        std::remove_reference_t<iter_reference_t<O>>(*ifirst);
            }
            return {std::move(ifirst), std::move(oit)};
        } catch (...) {
            nano::destroy(ofirst, ++oit);
            throw;
        }
    }

    template <typename I, typename S, typename O>
    static uninitialized_copy_result<I, O>
    impl3(I ifirst, S ilast, O ofirst)
    {
        O oit = ofirst;
        try {
            for (; ifirst != ilast; ++ifirst, (void) ++oit) {
                ::new(const_cast<void*>(static_cast<const volatile void*>(std::addressof(
                        *oit))))
                        std::remove_reference_t<iter_reference_t<O>>(*ifirst);
            }
            return {std::move(ifirst), std::move(oit)};
        } catch (...) {
            nano::destroy(ofirst, ++oit);
            throw;
        }
    }

public:
    // Four-legged
    template <typename I, typename S, typename O, typename S2>
    std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            no_throw_forward_iterator<O> &&
            no_throw_sentinel<S2, O> &&
        constructible_from<iter_value_t<O>, iter_reference_t<I>>,
        uninitialized_copy_result<I, O>>
    operator()(I ifirst, S ilast, O ofirst, S2 olast) const
    {
        return uninitialized_copy_fn::impl4(
                std::move(ifirst), std::move(ilast),
                std::move(ofirst), std::move(olast));
    }

    // Two ranges
    template <typename IRng, typename ORng>
    std::enable_if_t<
        input_range<IRng> && no_throw_forward_range<ORng> &&
        constructible_from<iter_value_t<iterator_t<ORng>>, iter_reference_t<iterator_t<IRng>>>,
        uninitialized_copy_result<borrowed_iterator_t<IRng>,
                                               borrowed_iterator_t<ORng>>>
    operator()(IRng&& irng, ORng&& orng) const
    {
        return uninitialized_copy_fn::impl4(
                nano::begin(irng), nano::end(irng),
                nano::begin(orng), nano::end(orng));
    }

    // Three-legged
    template <typename I, typename S, typename O>
    NANO_DEPRECATED
    std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            no_throw_forward_iterator<O> &&
        constructible_from<iter_value_t<O>, iter_reference_t<I>>,
        uninitialized_copy_result<I, O>>
    operator()(I ifirst, S ilast, O ofirst) const
    {
        return uninitialized_copy_fn::impl3(std::move(ifirst), std::move(ilast),
                                            std::move(ofirst));
    }

    // Range and a half
    template <typename IRng, typename O>
    NANO_DEPRECATED
    std::enable_if_t<
        input_range<IRng> && no_throw_forward_iterator<std::decay_t<O>> &&
        !no_throw_forward_range<O> &&
        constructible_from<iter_value_t<std::decay_t<O>>, iter_reference_t<iterator_t<IRng>>>,
        uninitialized_copy_result<borrowed_iterator_t<IRng>, std::decay_t<O>>>
    operator()(IRng&& irng, O&& ofirst) const
    {
        return uninitialized_copy_fn::impl3(
                nano::begin(irng), nano::end(irng), std::forward<O>(ofirst));
    }
};

}

NANO_INLINE_VAR(detail::uninitialized_copy_fn, uninitialized_copy)

template <typename I, typename O>
using uninitialized_copy_n_result = in_out_result<I, O>;

namespace detail {

struct uninitialized_copy_n_fn {
    template <typename I, typename O, typename S>
    std::enable_if_t<
        input_iterator<I> && no_throw_forward_iterator<O> &&
            no_throw_sentinel<S, O> &&
        constructible_from<iter_value_t<O>, iter_reference_t<I>>,
        uninitialized_copy_n_result<I, O>>
    operator()(I ifirst, iter_difference_t<I> n, O ofirst, S olast) const
    {
        auto t = uninitialized_copy_fn::impl4(
                    make_counted_iterator(std::move(ifirst), n),
                    default_sentinel, std::move(ofirst), std::move(olast));
        return {std::move(t).in.base(), std::move(t).out};
    }

    template <typename I, typename O>
    NANO_DEPRECATED
    std::enable_if_t<
        input_iterator<I> && no_throw_forward_iterator<O> &&
        constructible_from<iter_value_t<O>, iter_reference_t<I>>,
        uninitialized_copy_n_result<I, O>>
    operator()(I ifirst, iter_difference_t<I> n, O ofirst) const
    {
        auto t = uninitialized_copy_fn::impl3(
                make_counted_iterator(std::move(ifirst), n),
                default_sentinel, std::move(ofirst));
        return {std::move(t).in.base(), std::move(t).out};
    }

};

}

NANO_INLINE_VAR(detail::uninitialized_copy_n_fn, uninitialized_copy_n)

NANO_END_NAMESPACE

#endif

// nanorange/memory/uninitialized_default_construct.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_MEMORY_UNINITIALIZED_DEFAULT_CONSTRUCT_HPP_INCLUDED
#define NANORANGE_MEMORY_UNINITIALIZED_DEFAULT_CONSTRUCT_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct uninitialized_default_construct_fn {
private:
    template <typename I, typename S>
    static I impl(I first, S last)
    {
        I it = first;
        try {
            for (; it != last; ++it) {
                ::new(detail::voidify(*it)) std::remove_reference_t<iter_reference_t<I>>;
            }
            return it;
        } catch (...) {
            nano::destroy(first, ++it);
            throw;
        }
    }

public:
    template <typename I, typename S>
    std::enable_if_t<no_throw_forward_iterator<I> && no_throw_sentinel<S, I> &&
                         default_initializable<iter_value_t<I>>, I>
    operator()(I first, S last) const
    {
        return uninitialized_default_construct_fn::impl(
                std::move(first), std::move(last));
    }

    template <typename Rng>
    std::enable_if_t<no_throw_forward_range<Rng> &&
                         default_initializable<iter_value_t<iterator_t<Rng>>>,
                     borrowed_iterator_t<Rng>>
    operator()(Rng&& rng) const
    {
        return uninitialized_default_construct_fn::impl(
                nano::begin(rng), nano::end(rng));
    }
};

}

NANO_INLINE_VAR(detail::uninitialized_default_construct_fn,
                uninitialized_default_construct)

namespace detail {

struct uninitialized_default_construct_n_fn {
    template <typename I>
    std::enable_if_t<no_throw_forward_iterator<I> &&
                         default_initializable<iter_value_t<I>>, I>
    operator()(I first, iter_difference_t<I> n) const
    {
        return nano::uninitialized_default_construct(
                    make_counted_iterator(std::move(first), n),
                    default_sentinel).base();
    }

};

}

NANO_INLINE_VAR(detail::uninitialized_default_construct_n_fn,
                uninitialized_default_construct_n)

NANO_END_NAMESPACE

#endif

// nanorange/memory/uninitialized_fill.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_MEMORY_UNINITIALIZED_FILL_HPP_INCLUDED
#define NANORANGE_MEMORY_UNINITIALIZED_FILL_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct uninitialized_fill_fn {
private:
    friend struct uninitialized_fill_n_fn;

    template <typename I, typename S, typename T>
    static I impl(I first, S last, const T& x)
    {
        I it = first;
        try {
            for (; it != last; ++it) {
                ::new(detail::voidify(*it))
                        std::remove_reference_t<iter_reference_t<I>>(x);
            }
            return it;
        } catch (...) {
            nano::destroy(first, ++it);
            throw;
        }
    }

public:
    template <typename I, typename S, typename T>
    std::enable_if_t<no_throw_forward_iterator<I> && no_throw_sentinel<S, I> &&
        constructible_from<iter_value_t<I>, const T&>,
        I>
    operator()(I first, S last, const T& x) const
    {
        return uninitialized_fill_fn::impl(std::move(first), std::move(last), x);
    }

    template <typename Rng, typename T>
    std::enable_if_t<
        no_throw_forward_range<Rng> &&
        constructible_from<iter_value_t<iterator_t<Rng>>, const T&>,
        borrowed_iterator_t<Rng>>
    operator()(Rng&& rng, const T& x) const
    {
        return uninitialized_fill_fn::impl(nano::begin(rng), nano::end(rng), x);
    }
};

}

NANO_INLINE_VAR(detail::uninitialized_fill_fn, uninitialized_fill)

namespace detail {

struct uninitialized_fill_n_fn {
    template <typename I, typename T>
    std::enable_if_t<no_throw_forward_iterator<I> &&
        constructible_from<iter_value_t<I>, const T&>,
        I>
    operator()(I first, iter_difference_t<I> n, const T& x) const
    {
        return uninitialized_fill_fn::impl(
                    make_counted_iterator(std::move(first), n),
                    default_sentinel, x).base();
    }
};

}

NANO_INLINE_VAR(detail::uninitialized_fill_n_fn, uninitialized_fill_n)

NANO_END_NAMESPACE

#endif

// nanorange/memory/uninitialized_move.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_MEMORY_UNINITIALIZED_MOVE_HPP_INCLUDED
#define NANORANGE_MEMORY_UNINITIALIZED_MOVE_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename I, typename O>
using uninitialized_move_result = in_out_result<I, O>;

namespace detail {

struct uninitialized_move_fn {
private:
    friend struct uninitialized_move_n_fn;

    template <typename I, typename S, typename O, typename S2>
    static uninitialized_move_result<I, O>
    impl4(I ifirst, S ilast, O ofirst, S2 olast)
    {
        O oit = ofirst;
        try {
            for (; ifirst != ilast && oit != olast; ++ifirst, (void) ++oit) {
                ::new(detail::voidify(*oit))
                        std::remove_reference_t<iter_reference_t<O>>(nano::iter_move(ifirst));
            }
            return {std::move(ifirst), std::move(oit)};
        } catch (...) {
            nano::destroy(ofirst, ++oit);
            throw;
        }
    }

    template <typename I, typename S, typename O>
    static uninitialized_move_result<I, O>
    impl3(I ifirst, S ilast, O ofirst)
    {
        O oit = ofirst;
        try {
            for (; ifirst != ilast; ++ifirst, (void) ++oit) {
                ::new(detail::voidify(*oit))
                        std::remove_reference_t<iter_reference_t<O>>(nano::iter_move(ifirst));
            }
            return {std::move(ifirst), std::move(oit)};
        } catch (...) {
            nano::destroy(ofirst, ++oit);
            throw;
        }
    }

public:
    // Four-legged
    template <typename I, typename S, typename O, typename S2>
    std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            no_throw_forward_iterator<O> &&
            no_throw_sentinel<S2, O> &&
        constructible_from<iter_value_t<O>, iter_rvalue_reference_t<I>>,
        uninitialized_move_result<I, O>>
    operator()(I ifirst, S ilast, O ofirst, S2 olast) const
    {
        return uninitialized_move_fn::impl4(
                std::move(ifirst), std::move(ilast),
                std::move(ofirst), std::move(olast));
    }

    // Two ranges
    template <typename IRng, typename ORng>
    std::enable_if_t<
        input_range<IRng> && no_throw_forward_range<ORng> &&
        constructible_from<iter_value_t<iterator_t<ORng>>, iter_rvalue_reference_t<iterator_t<IRng>>>,
        uninitialized_move_result<borrowed_iterator_t<IRng>,
                                  borrowed_iterator_t<ORng>>>
    operator()(IRng&& irng, ORng&& orng) const
    {
        return uninitialized_move_fn::impl4(
                nano::begin(irng), nano::end(irng),
                nano::begin(orng), nano::end(orng));
    }

    // Three-legged
    template <typename I, typename S, typename O>
    NANO_DEPRECATED
    std::enable_if_t<
        input_iterator<I> && sentinel_for<S, I> &&
            no_throw_forward_iterator<O> &&
        constructible_from<iter_value_t<O>, iter_rvalue_reference_t<I>>,
        uninitialized_move_result<I, O>>
    operator()(I ifirst, S ilast, O ofirst) const
    {
        return uninitialized_move_fn::impl3(std::move(ifirst), std::move(ilast),
                                            std::move(ofirst));
    }

    // Range and a half
    template <typename IRng, typename O>
    NANO_DEPRECATED
    std::enable_if_t<
        input_range<IRng> && no_throw_forward_iterator<std::decay_t<O>> &&
        !no_throw_forward_range<O> &&
        constructible_from<iter_value_t<std::decay_t<O>>, iter_rvalue_reference_t<iterator_t<IRng>>>,
        uninitialized_move_result<borrowed_iterator_t<IRng>, std::decay_t<O>>>
    operator()(IRng&& irng, O&& ofirst) const
    {
        return uninitialized_move_fn::impl3(
                nano::begin(irng), nano::end(irng), std::forward<O>(ofirst));
    }
};

}

NANO_INLINE_VAR(detail::uninitialized_move_fn, uninitialized_move)

template <typename I, typename O>
using uninitialized_move_n_result = in_out_result<I, O>;

namespace detail {

struct uninitialized_move_n_fn {
    template <typename I, typename O, typename S>
    std::enable_if_t<
        input_iterator<I> && no_throw_forward_iterator<O> &&
            no_throw_sentinel<S, O> &&
        constructible_from<iter_value_t<O>, iter_rvalue_reference_t<I>>,
        uninitialized_move_n_result<I, O>>
    operator()(I ifirst, iter_difference_t<I> n, O ofirst, S olast) const
    {
        auto t = uninitialized_move_fn::impl4(
                    make_counted_iterator(std::move(ifirst), n),
                    default_sentinel, std::move(ofirst), std::move(olast));
        return {std::move(t).in.base(), std::move(t).out};
    }

    template <typename I, typename O>
    NANO_DEPRECATED
    std::enable_if_t<
        input_iterator<I> && no_throw_forward_iterator<O> &&
        constructible_from<iter_value_t<O>, iter_rvalue_reference_t<I>>,
        uninitialized_move_n_result<I, O>>
    operator()(I ifirst, iter_difference_t<I> n, O ofirst) const
    {
        auto t = uninitialized_move_fn::impl3(
                make_counted_iterator(std::move(ifirst), n),
                default_sentinel, std::move(ofirst));
        return {std::move(t).in.base(), std::move(t).out};
    }

};

}

NANO_INLINE_VAR(detail::uninitialized_move_n_fn, uninitialized_move_n)

NANO_END_NAMESPACE

#endif

// nanorange/memory/uninitialized_value_construct.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_MEMORY_UNINITIALIZED_VALUE_CONSTRUCT_HPP_INCLUDED
#define NANORANGE_MEMORY_UNINITIALIZED_VALUE_CONSTRUCT_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

struct uninitialized_value_construct_fn {
private:
    template <typename I, typename S>
    static I impl(I first, S last)
    {
        I it = first;
        try {
            for (; it != last; ++it) {
                ::new(detail::voidify(*it)) std::remove_reference_t<iter_reference_t<I>>();
            }
            return it;
        } catch (...) {
            nano::destroy(first, ++it);
            throw;
        }
    }

public:
    template <typename I, typename S>
    std::enable_if_t<no_throw_forward_iterator<I> && no_throw_sentinel<S, I> &&
                         default_initializable<iter_value_t<I>>, I>
    operator()(I first, S last) const
    {
        return uninitialized_value_construct_fn::impl(
                std::move(first), std::move(last));
    }

    template <typename Rng>
    std::enable_if_t<no_throw_forward_range<Rng> &&
                         default_initializable<iter_value_t<iterator_t<Rng>>>,
                     borrowed_iterator_t<Rng>>
    operator()(Rng&& rng) const
    {
        return uninitialized_value_construct_fn::impl(
                nano::begin(rng), nano::end(rng));
    }
};

}

NANO_INLINE_VAR(detail::uninitialized_value_construct_fn,
                uninitialized_value_construct)

namespace detail {

struct uninitialized_value_construct_n_fn {
    template <typename I>
    std::enable_if_t<no_throw_forward_iterator<I> &&
                         default_initializable<iter_value_t<I>>, I>
    operator()(I first, iter_difference_t<I> n) const
    {
        return nano::uninitialized_value_construct(
                    make_counted_iterator(std::move(first), n),
                    default_sentinel).base();
    }

};

}

NANO_INLINE_VAR(detail::uninitialized_value_construct_n_fn,
                uninitialized_value_construct_n)

NANO_END_NAMESPACE

#endif


#endif




// nanorange/utility.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_UTILITY_HPP_INCLUDED
#define NANORANGE_UTILITY_HPP_INCLUDED



#endif

// nanorange/range.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_HPP_INCLUDED
#define NANORANGE_VIEWS_HPP_INCLUDED

// nanorange/views/all.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_ALL_HPP_INCLUDED
#define NANORANGE_VIEWS_ALL_HPP_INCLUDED

// nanorange/detail/views/range_adaptors.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_VIEW_RANGE_ADAPTORS_HPP_INCLUDED
#define NANORANGE_DETAIL_VIEW_RANGE_ADAPTORS_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace detail {

template <typename>
inline constexpr bool is_raco = false;

template <typename R, typename C,
          typename = std::enable_if_t<
              viewable_range<R> &&
              !is_raco<remove_cvref_t<R>> && is_raco<remove_cvref_t<C>>>>
constexpr auto operator|(R&& lhs, C&& rhs)
    -> decltype(std::forward<C>(rhs)(std::forward<R>(lhs)))
{
    return std::forward<C>(rhs)(std::forward<R>(lhs));
}

template <typename LHS, typename RHS>
struct raco_pipe {
private:
    LHS lhs_;
    RHS rhs_;

public:
    constexpr raco_pipe(LHS&& lhs, RHS&& rhs)
        : lhs_(std::move(lhs)),
          rhs_(std::move(rhs))
    {}

    // FIXME: Do I need to do ref-qualified overloads of these too?

    template <typename R, std::enable_if_t<viewable_range<R>, int> = 0>
    constexpr auto operator()(R&& r)
        -> decltype(rhs_(lhs_(std::forward<R>(r))))
    {
        return rhs_(lhs_(std::forward<R>(r)));
    }


    template <typename R, std::enable_if_t<viewable_range<R>, int> = 0>
    constexpr auto operator()(R&& r) const
        -> decltype(rhs_(lhs_(std::forward<R>(r))))
    {
        return rhs_(lhs_(std::forward<R>(r)));
    }
};

template <typename LHS, typename RHS>
inline constexpr bool is_raco<raco_pipe<LHS, RHS>> = true;

template <typename LHS, typename RHS>
constexpr auto operator|(LHS&& lhs, RHS&& rhs)
    -> std::enable_if_t<
        is_raco<remove_cvref_t<LHS>> && is_raco<remove_cvref_t<RHS>>,
        raco_pipe<LHS, RHS>>
{
    return raco_pipe<LHS, RHS>{std::forward<LHS>(lhs), std::forward<RHS>(rhs)};
}

template <typename Lambda>
struct rao_proxy : Lambda {
    constexpr explicit rao_proxy(Lambda&& l) : Lambda(std::move(l)) {}
};

template <typename L>
inline constexpr bool is_raco<rao_proxy<L>> = true;

} // namespace detail

NANO_END_NAMESPACE

#endif

// nanorange/detail/views/ref.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_REF_HPP_INCLUDED
#define NANORANGE_VIEWS_REF_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace ref_view_ {

template <typename R>
class ref_view : public view_interface<ref_view<R>> {

    static_assert(range<R> && std::is_object<R>::value, "");

    R* r_ = nullptr;

    struct constructor_req {
        static void FUN(R&);
        static void FUN(R&&) = delete;

        template <typename T>
        auto requires_() -> decltype(FUN(std::declval<T>()));
    };

public:
    constexpr ref_view() noexcept = default;

    template <typename T,
              std::enable_if_t<detail::not_same_as<T, ref_view>, int> = 0,
              std::enable_if_t<detail::requires_<constructor_req, T>, int> = 0,
              std::enable_if_t<convertible_to<T, R&>, int> = 0>
    constexpr ref_view(T&& t)
        : r_(std::addressof(static_cast<R&>(std::forward<T>(t))))
    {}

    constexpr R& base() const { return *r_; }

    constexpr iterator_t<R> begin() const { return ranges::begin(*r_); }

    constexpr sentinel_t<R> end() const { return ranges::end(*r_); }

    template <typename RR = R,
              typename = decltype(ranges::empty(std::declval<RR&>()))>
    constexpr bool empty() const
    {
        return ranges::empty(*r_);
    }

    template <typename RR = R, std::enable_if_t<sized_range<RR>, int> = 0>
    constexpr auto size() const
    {
        return ranges::size(*r_);
    }

    template <typename RR = R, std::enable_if_t<contiguous_range<RR>, int> = 0>
    constexpr auto data() const
    {
        return ranges::data(*r_);
    }
};

template <typename R, std::enable_if_t<range<R> && std::is_object_v<R>, int> = 0>
ref_view(R&) -> ref_view<R>;

} // namespace ref_view_

using ref_view_::ref_view;

template <typename R>
inline constexpr bool enable_borrowed_range<ref_view<R>> = true;

NANO_END_NAMESPACE

#endif


NANO_BEGIN_NAMESPACE

namespace detail {

struct all_view_fn {
private:
    template <typename T>
    static constexpr auto impl(T&& t, priority_tag<2>)
        noexcept(noexcept(detail::decay_copy(std::forward<T>(t))))
        -> std::enable_if_t<view<std::decay_t<T>>,
                            decltype(detail::decay_copy(std::forward<T>(t)))>
    {
        return std::forward<T>(t);
    }

    template <typename T>
    static constexpr auto impl(T&& t, priority_tag<1>) noexcept
        -> decltype(ref_view(std::forward<T>(t)))
    {
        return ref_view(std::forward<T>(t));
    }

    template <typename T>
    static constexpr auto impl(T&& t, priority_tag<0>)
        noexcept(noexcept(nano::subrange{std::forward<T>(t)}))
        -> decltype(nano::subrange{std::forward<T>(t)})
    {
        return nano::subrange{std::forward<T>(t)};
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(all_view_fn::impl(std::forward<T>(t), priority_tag<2>{})))
        -> decltype(all_view_fn::impl(std::forward<T>(t), priority_tag<2>{}))
    {
        return all_view_fn::impl(std::forward<T>(t), priority_tag<2>{});
    }
};

template <>
inline constexpr bool is_raco<all_view_fn> = true;

} // namespace detail

namespace views {

NANO_INLINE_VAR(nano::detail::all_view_fn, all)

}

template <typename R>
using all_view = std::enable_if_t<viewable_range<R>,
                                  decltype(views::all(std::declval<R>()))>;

NANO_END_NAMESPACE

#endif

// nanorange/views/common.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_COMMON_HPP_INCLUDED
#define NANORANGE_VIEWS_COMMON_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename V>
class common_view : public view_interface<common_view<V>> {

    static_assert(view<V> && !common_range<V>, "");

    template <typename VV>
    using random_and_sized_t =
            std::integral_constant<bool,
                               random_access_range<VV> && sized_range<VV>>;


    V base_ = V();

    template <typename VV>
    static constexpr auto do_begin(VV& base, std::true_type)
    {
        return ranges::begin(base);
    }

    template <typename VV>
    static constexpr auto do_begin(VV& base, std::false_type)
    {
        return common_iterator<iterator_t<VV>, sentinel_t<VV>>(
                ranges::begin(base));
    }

    template <typename VV>
    static constexpr auto do_end(VV& base, std::true_type)
    {
        return ranges::begin(base) + ranges::size(base);
    }

    template <typename VV>
    static constexpr auto do_end(VV& base, std::false_type)
    {
        return common_iterator<iterator_t<VV>, sentinel_t<VV>>(
                ranges::end(base));
    }


public:
    common_view() = default;

    constexpr explicit common_view(V r)
        : base_(std::move(r))
    {}

    template <typename R,
        std::enable_if_t<detail::not_same_as<R, common_view>, int> = 0,
        std::enable_if_t<viewable_range<R> &&
                !common_range<R> &&
                constructible_from<V, all_view<R>>, int> = 0>
    constexpr explicit common_view(R&& r)
        : base_(views::all(std::forward<R>(r)))
    {}

    constexpr V base() const { return base_; }

    template <typename VV = V, std::enable_if_t<sized_range<VV>, int> = 0>
    constexpr auto size() { return ranges::size(base_); }

    template <typename VV = V, std::enable_if_t<sized_range<const VV>, int> = 0>
    constexpr auto size() const { return ranges::size(base_); }

    constexpr auto begin()
    {
        return do_begin<V>(base_, random_and_sized_t<V>{});
    }

    template <typename VV = V, std::enable_if_t<range<const VV>, int> = 0>
    constexpr auto begin() const
    {
        return do_begin<const V>(base_, random_and_sized_t<const V>{});
    }

    constexpr auto end()
    {
        return do_end<V>(base_, random_and_sized_t<V>{});
    }

    template <typename VV = V, std::enable_if_t<range<const VV>, int> = 0>
    constexpr auto end() const
    {
        return do_end<const V>(base_, random_and_sized_t<const V>{});
    }

};

template <typename R>
common_view(R&&) -> common_view<all_view<R>>;

namespace detail {

struct common_view_fn {
private:
    template <typename T>
    static constexpr auto impl(T&& t, nano::detail::priority_tag<1>)
        noexcept(noexcept(views::all(std::forward<T>(t))))
        -> std::enable_if_t<common_range<T>,
            decltype(views::all(std::forward<T>(t)))>
    {
        return views::all(std::forward<T>(t));
    }

    template <typename T>
    static constexpr auto impl(T&& t, nano::detail::priority_tag<0>)
        -> common_view<all_view<T>>
    {
        return common_view<all_view<T>>{std::forward<T>(t)};
    }

public:
    template <typename T>
    constexpr auto operator()(T&& t) const
        -> std::enable_if_t<
        viewable_range<T>,
        decltype(common_view_fn::impl(std::forward<T>(t),
                                    nano::detail::priority_tag<1>{}))>
    {
        return common_view_fn::impl(std::forward<T>(t),
                               nano::detail::priority_tag<1>{});
    }
};

template <>
inline constexpr bool is_raco<common_view_fn> = true;

} // namespace detail

namespace views {

NANO_INLINE_VAR(::nano::detail::common_view_fn, common)

} // namespace views

NANO_END_NAMESPACE

#endif

// nanorange/views/counted.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_COUNTED_HPP_INCLUDED
#define NANORANGE_VIEWS_COUNTED_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

namespace views {

namespace detail {

struct counted_fn {
private:
    template <typename I>
    static constexpr auto impl(I i, iter_difference_t<I> n, nano::detail::priority_tag<1>)
        noexcept(noexcept(nano::subrange{i, i + n}))
        -> std::enable_if_t<random_access_iterator<I>, decltype(nano::subrange{i, i + n})>
    {
        return nano::subrange{i, i + n};
    }

    template <typename I>
    static constexpr auto impl(I i, iter_difference_t<I> n, nano::detail::priority_tag<0>)
        noexcept(noexcept(nano::subrange{
                nano::make_counted_iterator(std::move(i), n),
                default_sentinel}))
        -> decltype(nano::subrange{
            nano::make_counted_iterator(std::move(i), n), default_sentinel})
    {
        return nano::subrange{nano::make_counted_iterator(std::move(i), n),
                                   default_sentinel};
    }

public:
    template <typename E, typename F, typename T = std::decay_t<E>>
    constexpr auto operator()(E&& e, F&& f) const
        noexcept(noexcept(impl(std::forward<E>(e),
                               static_cast<iter_difference_t<T>>(std::forward<F>(f)),
                               nano::detail::priority_tag<1>{})))
        -> std::enable_if_t<
            input_or_output_iterator<T> &&
            convertible_to<F, iter_difference_t<T>>,
            decltype(impl(std::forward<E>(e),
                          static_cast<iter_difference_t<T>>(std::forward<F>(f)),
                          nano::detail::priority_tag<1>{}))>
    {
        return impl(std::forward<E>(e),
                    static_cast<iter_difference_t<T>>(std::forward<F>(f)),
                    nano::detail::priority_tag<1>{});
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::counted_fn, counted)

} // namespace views

NANO_END_NAMESPACE

#endif

// nanorange/views/drop.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_DROP_HPP_INCLUDED
#define NANORANGE_VIEWS_DROP_HPP_INCLUDED





#include <optional>

NANO_BEGIN_NAMESPACE

namespace detail {

template <bool IsRandomAccess, typename>
struct drop_view_cache {};

template <typename I>
struct drop_view_cache<false, I> {
    std::optional<I> cached{};
};

}

template <typename R>
struct drop_view
    : view_interface<drop_view<R>>,
      private detail::drop_view_cache<random_access_range<R>, iterator_t<R>> {

    static_assert(view<R>);

    drop_view() = default;

    constexpr drop_view(R base, range_difference_t<R> count)
        : base_(std::move(base)),
          count_(count)
    {}

    constexpr R base() const { return base_; }

    template <typename RR = R, std::enable_if_t<
        !(detail::simple_view<RR> && random_access_range<RR>), int> = 0>
    constexpr auto begin()
    {
        if constexpr (random_access_range<R>) {
            return ranges::next(ranges::begin(base_), count_, ranges::end(base_));
        } else {
            auto& c = this->cached;
            if (!c.has_value()) {
                c = ranges::next(ranges::begin(base_), count_, ranges::end(base_));
            }
            return *c;
        }
    }

    template <typename RR = R, std::enable_if_t<random_access_range<const RR>, int> = 0>
    constexpr auto begin() const
    {
        return ranges::next(ranges::begin(base_), count_, ranges::end(base_));
    }

    template <typename RR = R, std::enable_if_t<!detail::simple_view<RR>, int> = 0>
    constexpr auto end()
    {
        return ranges::end(base_);
    }

    template <typename RR = R, std::enable_if_t<range<const RR>, int> = 0>
    constexpr auto end()
    {
        return ranges::end(base_);
    }

    template <typename RR = R, std::enable_if_t<sized_range<RR>, int> = 0>
    constexpr auto size()
    {
        const auto s = ranges::size(base_);
        const auto c = static_cast<decltype(s)>(count_);
        return s < c ? 0 : s - c;
    }

    template <typename RR = R, std::enable_if_t<sized_range<const RR>, int> = 0>
    constexpr auto size() const
    {
        const auto s = ranges::size(base_);
        const auto c = static_cast<decltype(s)>(count_);
        return s < c ? 0 : s - c;
    }

private:
    R base_ = R();
    range_difference_t<R> count_ = 0;
};

template <typename R>
drop_view(R&&, range_difference_t<R>) -> drop_view<all_view<R>>;

namespace detail {

struct drop_view_fn {

    template <typename E, typename F>
    constexpr auto operator()(E&& e, F&& f) const
        -> decltype(drop_view{std::forward<E>(e), std::forward<F>(f)})
    {
        return drop_view{std::forward<E>(e), std::forward<F>(f)};
    }

    template <typename C>
    constexpr auto operator()(C c) const
    {
        return detail::rao_proxy{[c = std::move(c)](auto&& r) mutable
#ifndef NANO_MSVC_LAMBDA_PIPE_WORKAROUND
            -> decltype(drop_view{std::forward<decltype(r)>(r), std::declval<C&&>()})
#endif
        {
            return drop_view{std::forward<decltype(r)>(r), std::move(c)};
        }};
    }

};

}

namespace views {

NANO_INLINE_VAR(nano::detail::drop_view_fn, drop)

}

NANO_END_NAMESPACE

#endif

// nanorange/views/drop_while.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_DROP_WHILE_HPP_INCLUDED
#define NANORANGE_VIEWS_DROP_WHILE_HPP_INCLUDED



// nanorange/detail/views/semiregular_box.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_DETAIL_VIEW_SEMIREGULAR_BOX_HPP_INCLUDED
#define NANORANGE_DETAIL_VIEW_SEMIREGULAR_BOX_HPP_INCLUDED



#include <optional>

NANO_BEGIN_NAMESPACE

namespace detail {

template <typename T>
struct semiregular_box : std::optional<T>
{
    static_assert(copy_constructible<T>);
    static_assert(std::is_object_v<T>);

private:
    std::optional<T>& base() { return *this; }
    std::optional<T> const& base() const { return *this; }

public:
    template <typename U = T,
              std::enable_if_t<default_initializable<U>, int> = 0>
    constexpr semiregular_box()
        noexcept(std::is_nothrow_default_constructible_v<T>)
        : semiregular_box{std::in_place}
    {}

    template <typename U = T,
              std::enable_if_t<!default_initializable<U>, int> = 0>
    constexpr semiregular_box() {}

    // All other constructors get forwarded to optional -- but don't hijack
    // copy/move construct
    template <typename Arg0, typename... Args,
              std::enable_if_t<
                  constructible_from<std::optional<T>, Arg0, Args...> &&
                  !same_as<remove_cvref_t<Arg0>, semiregular_box>, int> = 0>
    constexpr semiregular_box(Arg0&& arg0, Args&&... args)
        : std::optional<T>{std::forward<Arg0>(arg0), std::forward<Args>(args)...}
    {}

    constexpr semiregular_box(const semiregular_box&) = default;
    constexpr semiregular_box(semiregular_box&&) = default;

    semiregular_box& operator=(const semiregular_box& other)
    {
        if constexpr (assignable_from<T&, const T&>) {
            base() = other.base();
        } else {
            if (other) {
                this->emplace(*other);
            } else {
                this->reset();
            }
        }

        return *this;
    }

    semiregular_box& operator=(semiregular_box&& other) noexcept
    {
        if constexpr (assignable_from<T&, T>) {
            base() = std::move(other.base());
        } else {
            if (other) {
                this->emplace(std::move(*other));
            } else {
                this->reset();
            }
        }

        return *this;
    }
};

} // namespace detail

NANO_END_NAMESPACE

#endif




NANO_BEGIN_NAMESPACE

template <typename R, typename Pred>
struct drop_while_view : view_interface<drop_while_view<R, Pred>> {

    static_assert(view<R>);
    static_assert(input_range<R>);
    static_assert(std::is_object_v<Pred>);
    static_assert(indirect_unary_predicate<const Pred, iterator_t<R>>);

    drop_while_view() = default;

    constexpr drop_while_view(R base, Pred pred)
        : base_(std::move(base)),
          pred_(std::move(pred))
    {}

    constexpr R base() const { return base_; }

    constexpr const Pred& pred() const { return *pred_; }

    constexpr auto begin()
    {
        if (!cached_.has_value()) {
            cached_ = ranges::find_if(base_,
                [&p = pred()](auto&& arg)
                {
                    return !nano::invoke(p, std::forward<decltype(arg)>(arg));
                });
        }

        return *cached_;
    }

    constexpr auto end()
    {
        return ranges::end(base_);
    }

private:
    R base_;
    detail::semiregular_box<Pred> pred_;
    std::optional<iterator_t<R>> cached_;
};

template <typename R, typename Pred>
drop_while_view(R&& r, Pred pred) -> drop_while_view<all_view<R>, Pred>;

namespace detail {

struct drop_while_view_fn {

    template <typename E, typename F>
    constexpr auto operator()(E&& e, F&& f) const
        -> decltype(drop_while_view{std::forward<E>(e), std::forward<F>(f)})
    {
        return drop_while_view{std::forward<E>(e), std::forward<F>(f)};
    }

    template <typename Pred>
    constexpr auto operator()(Pred&& pred) const
    {
        return detail::rao_proxy{[p = std::forward<Pred>(pred)](auto&& r) mutable
#ifndef NANO_MSVC_LAMBDA_PIPE_WORKAROUND
            -> decltype(drop_while_view{std::forward<decltype(r)>(r), std::declval<Pred&&>()})
#endif
        {
            return drop_while_view{std::forward<decltype(r)>(r), std::move(p)};
        }};
    }

};

} // namespace detail

namespace views {

NANO_INLINE_VAR(nano::detail::drop_while_view_fn, drop_while)

}

NANO_END_NAMESPACE

#endif

// nanorange/views/elements.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_ELEMENTS_HPP_INCLUDED
#define NANORANGE_VIEWS_ELEMENTS_HPP_INCLUDED





NANO_BEGIN_NAMESPACE

namespace detail {

struct has_tuple_element_concept {
    template <typename T, typename I,
              std::size_t N = I::value,
              typename = typename std::tuple_size<T>::type>
    auto requires_(T t) -> decltype(
        requires_expr<(N < std::tuple_size_v<T>)>{},
        std::declval<std::tuple_element_t<N, T>>(),
        requires_expr<convertible_to<decltype(std::get<N>(t)), std::tuple_element_t<N, T>>>{}
    );
};

template <typename T, std::size_t N>
NANO_CONCEPT has_tuple_element =
    detail::requires_<has_tuple_element_concept, T,
                      std::integral_constant<std::size_t, N>>;

} // namespace detail

template <typename R, std::size_t N>
struct elements_view : view_interface<elements_view<R, N>> {

    static_assert(input_range<R>);
    static_assert(view<R>);
    static_assert(detail::has_tuple_element<range_value_t<R>, N>);
    static_assert(detail::has_tuple_element<
        std::remove_reference_t<range_reference_t<R>>, N>);

    elements_view() = default;

    constexpr explicit elements_view(R base)
        : base_(std::move(base))
    {}

    template <typename RR = R, std::enable_if_t<!detail::simple_view<RR>, int> = 0>
    constexpr auto begin()
    {
        return iterator<false>(ranges::begin(base_));
    }

    template <typename RR = R, std::enable_if_t<detail::simple_view<RR>, int> = 0>
    constexpr auto begin() const
    {
        return iterator<true>(ranges::begin(base_));
    }

    template <typename RR = R, std::enable_if_t<!detail::simple_view<RR>, int> = 0>
    constexpr auto end()
    {
        return ranges::end(base_);
    }

    template <typename RR = R, std::enable_if_t<detail::simple_view<RR>, int> = 0>
    constexpr auto end() const
    {
        return ranges::end(base_);
    }

    template <typename RR = R, std::enable_if_t<sized_range<RR>, int> = 0>
    constexpr auto size()
    {
        return ranges::size(base_);
    }

    template <typename RR = R, std::enable_if_t<sized_range<const RR>, int> = 0>
    constexpr auto size() const
    {
        return ranges::size(base_);
    }

private:
    template <bool Const>
    struct iterator
    {
    private:
        using base_t = detail::conditional_t<Const, const R, R>;
        friend iterator<!Const>;

        iterator_t<base_t> current_;

    public:
        using iterator_category = iterator_category_t<iterator_t<base_t>>;
        using value_type =
            remove_cvref_t<std::tuple_element_t<N, range_value_t<base_t>>>;
        using difference_type = range_difference_t<base_t>;

        iterator() = default;

        constexpr explicit iterator(iterator_t<base_t> current)
            : current_(std::move(current))
        {}

        template <typename I,
                  std::enable_if_t<same_as<I, iterator<!Const>>, int> = 0,
                  bool C = Const, typename B = base_t,
                  std::enable_if_t<C &&
                      convertible_to<iterator_t<R>, iterator_t<B>>, int> = 0>
        constexpr iterator(I i)
            : current_(std::move(i.current_))
        {}

        constexpr iterator_t<base_t> base() const { return current_; }

        constexpr decltype(auto) operator*() const
        {
            return std::get<N>(*current_);
        }

        constexpr iterator& operator++() { ++current_; return *this; }

        constexpr auto operator++(int)
        {
            if constexpr (forward_range<base_t>) {
                auto temp = *this;
                ++*this;
                return temp;
            } else {
                ++*this;
            }
        }

        template <typename B = base_t>
        constexpr auto operator--()
            -> std::enable_if_t<bidirectional_range<B>, iterator&>
        {
            --current_;
            return *this;
        }

        template <typename B = base_t>
        constexpr auto operator--(int)
            -> std::enable_if_t<bidirectional_range<B>, iterator>
        {
            auto temp = *this;
            ++*this;
            return temp;
        }

        template <typename B = base_t>
        constexpr auto operator+=(difference_type x)
            -> std::enable_if_t<random_access_range<B>, iterator&>
        {
            current_ += x;
            return *this;
        }

        template <typename B = base_t>
        constexpr auto operator-=(difference_type x)
            -> std::enable_if_t<random_access_range<B>, iterator&>
        {
            current_ -= x;
            return *this;
        }

        template <typename B = base_t,
                  std::enable_if_t<random_access_range<B>, int> = 0>
        constexpr decltype(auto) operator[](difference_type n) const
        {
            return std::get<N>(*(current_ + n));
        }

        template <typename B = base_t>
        friend constexpr auto operator==(const iterator& x, const iterator& y)
            -> std::enable_if_t<equality_comparable<iterator_t<B>>, bool>
        {
            return x.current_ == y.current_;
        }

        template <typename B = base_t>
        friend constexpr auto operator!=(const iterator& x, const iterator& y)
            -> std::enable_if_t<equality_comparable<iterator_t<B>>, bool>
        {
            return !(x == y);
        }

        // Make these friend functions templates to keep MSVC happy
#if (defined(_MSC_VER) && _MSC_VER < 1922)
        template <typename = void>
#endif
        friend constexpr bool operator==(const iterator& x, const sentinel_t<base_t>& y)
        {
            return x.current_ == y;
        }

#if (defined(_MSC_VER) && _MSC_VER < 1922)
        template <typename = void>
#endif
        friend constexpr bool operator==(const sentinel_t<base_t>& y, const iterator& x)
        {
            return x.current_ == y;
        }

#if (defined(_MSC_VER) && _MSC_VER < 1922)
        template <typename = void>
#endif
        friend constexpr bool operator!=(const iterator& x, const sentinel_t<base_t>& y)
        {
            return !(x == y);
        }

#if (defined(_MSC_VER) && _MSC_VER < 1922)
        template <typename = void>
#endif
        friend constexpr bool operator!=(const sentinel_t<base_t>& y, const iterator& x)
        {
            return !(x == y);
        }

        template <typename B = base_t>
        friend constexpr auto operator<(const iterator& x, const iterator& y)
            -> std::enable_if_t<random_access_range<B>, bool>
        {
            return x.current_ < y.current_;
        }

        template <typename B = base_t>
        friend constexpr auto operator>(const iterator& x, const iterator& y)
            -> std::enable_if_t<random_access_range<B>, bool>
        {
            return (y < x);
        }

        template <typename B = base_t>
        friend constexpr auto operator<=(const iterator& x, const iterator& y)
            -> std::enable_if_t<random_access_range<B>, bool>
        {
            return !(y < x);
        }

        template <typename B = base_t>
        friend constexpr auto operator>=(const iterator& x, const iterator& y)
            -> std::enable_if_t<random_access_range<B>, bool>
        {
            return !(x < y);
        }

        template <typename B = base_t>
        friend constexpr auto operator+(const iterator& x, difference_type y)
            -> std::enable_if_t<random_access_range<B>, iterator>
        {
            return iterator{x} += y;
        }

        template <typename B = base_t>
        friend constexpr auto operator+(difference_type x, const iterator& y)
            -> std::enable_if_t<random_access_range<B>, iterator>
        {
            return y + x;
        }

        template <typename B = base_t>
        friend constexpr auto operator-(const iterator& x, difference_type y)
            -> std::enable_if_t<random_access_range<B>, iterator>
        {
            return iterator{x} -= y;
        }


        template <typename B = base_t>
        friend constexpr auto operator-(const iterator& x, const iterator& y)
            -> std::enable_if_t<random_access_range<B>, difference_type>
        {
            return x.current_ - y.current_;
        }

        template <typename B = base_t>
        friend constexpr auto operator-(const iterator& x, const sentinel_t<base_t>& y)
        -> std::enable_if_t<sized_sentinel_for<sentinel_t<B>, iterator_t<B>>,
            difference_type>
        {
            return x.current_ - y;
        }

        template <typename B = base_t>
        friend constexpr auto operator-(const sentinel_t<base_t>& x, const iterator& y)
            -> std::enable_if_t<sized_sentinel_for<sentinel_t<B>, iterator_t<B>>,
                                difference_type>
        {
            return -(y - x);
        }
    };

    R base_ = R();
};


template <typename R>
using keys_view = elements_view<all_view<R>, 0>;

template <typename R>
using values_view = elements_view<all_view<R>, 1>;

namespace detail {

template <std::size_t N>
struct elements_view_fn {
    template <typename E>
    constexpr auto operator()(E&& e) const
        -> decltype(elements_view<all_view<decltype(std::forward<E>(e))>, N>{std::forward<E>(e)})
    {
        return elements_view<all_view<decltype(std::forward<E>(e))>, N>{std::forward<E>(e)};
    }
};

template <std::size_t N>
inline constexpr bool is_raco<elements_view_fn<N>> = true;

} // namespace detail

namespace views {

inline namespace function_objects {

template <std::size_t N>
inline constexpr nano::detail::elements_view_fn<N> elements{};

inline constexpr nano::detail::elements_view_fn<0> keys{};

inline constexpr nano::detail::elements_view_fn<1> values{};

}

}


NANO_END_NAMESPACE

#endif

// nanorange/views/empty.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_EMPTY_HPP_INCLUDED
#define NANORANGE_VIEWS_EMPTY_HPP_INCLUDED



NANO_BEGIN_NAMESPACE

namespace empty_view_ {

template <typename T>
class empty_view : view_interface<empty_view<T>> {
    static_assert(std::is_object<T>::value, "");

public:
    static constexpr T* begin() noexcept { return nullptr; }
    static constexpr T* end() noexcept { return nullptr; }
    static constexpr std::ptrdiff_t size() noexcept { return 0; }
    static constexpr T* data() noexcept { return nullptr; }

    static constexpr bool empty() noexcept { return true; }
};

}

using empty_view_::empty_view;

template <typename T>
inline constexpr bool enable_borrowed_range<empty_view<T>> = true;

namespace views {

template <typename T, typename = std::enable_if_t<std::is_object<T>::value>>
inline constexpr empty_view<T> empty{};

}


NANO_END_NAMESPACE

#endif

// nanorange/views/filter.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_FILTER_HPP_INCLUDED
#define NANORANGE_VIEWS_FILTER_HPP_INCLUDED





#include <cassert>

NANO_BEGIN_NAMESPACE

namespace detail {

template <typename V>
constexpr auto filter_view_iter_cat_helper()
{
    using C = iterator_category_t<iterator_t<V>>;
    if constexpr (derived_from<C, bidirectional_iterator_tag>) {
        return bidirectional_iterator_tag{};
    } else if constexpr (derived_from<C, forward_iterator_tag>) {
        return forward_iterator_tag{};
    } else {
        return input_iterator_tag{};
    }
}

constexpr inline auto as_ref = [](auto& pred) {
    return [&p = pred] (auto&& arg) {
        return nano::invoke(p, std::forward<decltype(arg)>(arg));
    };
};

}

namespace filter_view_ {

template <typename V, typename Pred>
struct filter_view : view_interface<filter_view<V, Pred>> {

    // FIXME: GCC9 recursive constraint (?) problems again
    // static_assert(input_range<V>);
    static_assert(input_iterator<iterator_t<V>>);
    static_assert(indirect_unary_predicate<Pred, iterator_t<V>>);
    static_assert(view<V>);
    static_assert(std::is_object_v<Pred>);

private:
    V base_ = V();
    detail::semiregular_box<Pred> pred_;

    struct iterator {
    private:
        iterator_t<V> current_ = iterator_t<V>();
        filter_view* parent_ = nullptr;

    public:
        // using iterator_concept = ...
        using iterator_category =
            decltype(detail::filter_view_iter_cat_helper<V>());
        using value_type = iter_value_t<iterator_t<V>>;
        using difference_type = iter_difference_t<iterator_t<V>>;
        // Extension: legacy typedefs
        using pointer = iterator_t<V>;
        using reference = iter_reference_t<iterator_t<V>>;

        iterator() = default;
        constexpr iterator(filter_view& parent, iterator_t<V> current)
            : current_(std::move(current)), parent_(std::addressof(parent))
        {}

        constexpr iterator_t<V> base() const { return current_; }

        constexpr iter_reference_t<iterator_t<V>> operator*() const
        {
            return *current_;
        }

        template <typename VV = V>
        constexpr auto operator->() const
            -> std::enable_if_t<detail::has_arrow<iterator_t<VV>>, iterator_t<V>>
        {
            return current_;
        }

        constexpr iterator& operator++()
        {
            current_ = ranges::find_if(++current_,
                                       ranges::end(parent_->base_),
                                       detail::as_ref(*parent_->pred_));
            return *this;
        }

        constexpr auto operator++(int)
        {
            if constexpr (forward_range<V>) {
                auto tmp = *this;
                ++*this;
                return tmp;
            } else {
                ++*this;
            }
        }

        template <typename VV = V>
        constexpr auto operator--()
            -> std::enable_if_t<bidirectional_range<VV>, iterator&>
        {
            do {
                --current_;
            } while (!nano::invoke(*parent_->pred_, *current_));
            return *this;
        }

        template <typename VV = V>
        constexpr auto operator--(int)
            -> std::enable_if_t<bidirectional_range<VV>, iterator>
        {
            auto tmp = *this;
            --*this;
            return tmp;
        }

        template <typename VV = V>
        friend constexpr auto operator==(const iterator& x, const iterator& y)
            -> std::enable_if_t<equality_comparable<iterator_t<VV>>, bool>
        {
            return x.current_ == y.current_;
        }

        template <typename VV = V>
        friend constexpr auto operator!=(const iterator& x, const iterator& y)
            -> std::enable_if_t<equality_comparable<iterator_t<VV>>, bool>
        {
            return !(x == y);
        }

        friend constexpr iter_rvalue_reference_t<iterator_t<V>>
        iter_move(const iterator& i) noexcept(
            noexcept(ranges::iter_move(i.current_)))
        {
            return ranges::iter_move(i.current_);
        }

        template <typename VV = V>
        friend constexpr auto
        iter_swap(const iterator& x, const iterator& y) noexcept(
            noexcept(ranges::iter_swap(x.current_, y.current_)))
            -> std::enable_if_t<indirectly_swappable<iterator_t<VV>>>
        {
            ranges::iter_swap(x.current_, y.current_);
        }
    };

    struct sentinel {
    private:
        sentinel_t<V> end_ = sentinel_t<V>();

    public:
        sentinel() = default;

        constexpr explicit sentinel(filter_view& parent)
            : end_(ranges::end(parent.base_))
        {}

        constexpr sentinel_t<V> base() const { return end_; }

        // Make these friend functions templates to keep MSVC happy
#if (defined(_MSC_VER) && _MSC_VER < 1922)
        template <typename = void>
#endif
        friend constexpr bool operator==(const iterator& i, const sentinel& s)
        {
            return i.base() == s.end_;
        }

#if (defined(_MSC_VER) && _MSC_VER < 1922)
        template <typename = void>
#endif
        friend constexpr bool operator==(const sentinel& s, const iterator& i)
        {
            return i == s;
        }

#if (defined(_MSC_VER) && _MSC_VER < 1922)
        template <typename = void>
#endif
        friend constexpr bool operator!=(const iterator& i, const sentinel& s)
        {
            return !(i == s);
        }

#if (defined(_MSC_VER) && _MSC_VER < 1922)
        template <typename = void>
#endif
        friend constexpr bool operator!=(const sentinel& s, const iterator& i)
        {
            return !(i == s);
        }
    };

    std::optional<iterator> begin_cache_{};

public:
    filter_view() = default;

    constexpr filter_view(V base, Pred pred)
        : base_(std::move(base)), pred_(std::move(pred))
    {}

    constexpr V base() const { return base_; }

    constexpr iterator begin()
    {
        if (begin_cache_) {
            return *begin_cache_;
        }

        assert(pred_.has_value());
        begin_cache_ = std::optional<iterator>{
            iterator{*this, nano::find_if(base_, detail::as_ref(*pred_))}};
        return *begin_cache_;
    }

    constexpr auto end()
    {
        if constexpr (common_range<V>) {
            return iterator{*this, ranges::end(base_)};
        } else {
            return sentinel{*this};
        }
    }
};

template <typename R, typename Pred>
filter_view(R&&, Pred)->filter_view<all_view<R>, Pred>;

}

using filter_view_::filter_view;

namespace detail {

struct filter_view_fn {
    template <typename Pred>
    constexpr auto operator()(Pred pred) const
    {
        return detail::rao_proxy{[p = std::move(pred)] (auto&& r) mutable
#ifndef NANO_MSVC_LAMBDA_PIPE_WORKAROUND
            -> decltype(filter_view{std::forward<decltype(r)>(r), std::declval<Pred&&>()})
#endif
        {
            return filter_view{std::forward<decltype(r)>(r), std::move(p)};
        }};
    }

    template <typename R, typename Pred>
    constexpr auto operator()(R&& r, Pred pred) const
        noexcept(noexcept(filter_view{std::forward<R>(r), std::move(pred)}))
        -> decltype(filter_view{std::forward<R>(r), std::move(pred)})
    {
        return filter_view{std::forward<R>(r), std::move(pred)};
    }

};

}

namespace views {

NANO_INLINE_VAR(nano::detail::filter_view_fn, filter)

}

NANO_END_NAMESPACE

#endif


// nanorange/views/iota.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_IOTA_HPP_INCLUDED
#define NANORANGE_VIEWS_IOTA_HPP_INCLUDED





NANO_BEGIN_NAMESPACE

namespace detail {

struct decrementable_concept {
    template <typename I>
    auto requires_(I i) -> decltype(
        requires_expr<same_as<decltype(--i), I&>>{},
        requires_expr<same_as<decltype(i--), I>>{}
    );
};

template <typename I>
NANO_CONCEPT decrementable = incrementable<I> && requires_<decrementable_concept, I>;

template <typename W>
using iota_diff_t = iter_difference_t<W>;

struct advanceable_concept {
    template <typename I>
    auto requires_(I i, const I j, const iota_diff_t<I> n) -> decltype(
        requires_expr<same_as<decltype(i += n), I&>>{},
        requires_expr<same_as<decltype(i -= n), I&>>{},
        I(j + n),
        I(n + j),
        I(j - n),
        requires_expr<convertible_to<decltype(j - j), iota_diff_t<I>>>{}
    );
};

template <typename I>
NANO_CONCEPT advanceable =
    decrementable<I> && totally_ordered<I> &&
    requires_<advanceable_concept, I>;

template <typename W>
constexpr auto iota_view_iter_cat_helper()
{
    if constexpr (detail::advanceable<W>) {
        return random_access_iterator_tag{};
    } else if constexpr (detail::decrementable<W>) {
        return bidirectional_iterator_tag{};
    } else if constexpr (incrementable<W>) {
        return forward_iterator_tag{};
    } else {
        return input_iterator_tag{};
    }
}

} // namespace detail

template <typename W, typename Bound = unreachable_sentinel_t>
struct iota_view : view_interface<iota_view<W, Bound>> {
    static_assert(weakly_incrementable<W>);
    static_assert(semiregular<Bound>);
    static_assert(detail::weakly_equality_comparable_with<W, Bound>);

private:
    struct sentinel;

    struct iterator {
    private:
        friend struct sentinel;
        W value_ = W();

    public:
        using iterator_category = decltype(detail::iota_view_iter_cat_helper<W>());
        using value_type = W;
        using difference_type = detail::iota_diff_t<W>;
        // Extension: legacy typedefs
        using pointer = void;
        using reference = W;

        iterator() = default;

        constexpr explicit iterator(W value) : value_(value) {}

        constexpr W operator*() const
            noexcept(std::is_nothrow_copy_constructible_v<W>)
        {
            return value_;
        }

        constexpr iterator& operator++()
        {
            ++value_;
            return *this;
        }

        constexpr auto operator++(int)
        {
            if constexpr (incrementable<W>) {
                auto tmp = *this;
                ++*this;
                return tmp;
            } else {
                ++*this;
            }
        }

        template <typename WW = W>
        constexpr auto operator--()
            -> std::enable_if_t<detail::decrementable<WW>, iterator&>
        {
            --value_;
            return *this;
        }

        constexpr iterator operator--(int)
        {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        template <typename WW = W>
        constexpr auto operator+=(difference_type n)
            -> std::enable_if_t<detail::advanceable<WW>, iterator&>
        {
            if constexpr (integral<W> && !signed_integral<W>) {
                if (n >= difference_type(0)) {
                    value_ += static_cast<W>(n);
                } else {
                    value_ -= static_cast<W>(-n);
                }
            } else {
                value_ += n;
            }
            return *this;
        }

        template <typename WW = W>
        constexpr auto operator-=(difference_type n)
            -> std::enable_if_t<detail::advanceable<WW>, iterator&>
        {
            if constexpr (integral<W> && !signed_integral<W>) {
                if (n >= difference_type(0)) {
                    value_ -= static_cast<W>(n);
                } else {
                    value_ += static_cast<W>(-n);
                }
            } else {
                value_ -= n;
            }
            return *this;
        }

        template <typename WW = W>
        constexpr auto operator[](difference_type n) const
            -> std::enable_if_t<detail::advanceable<WW>, W>
        {
            return W(value_ + n);
        }

        template <typename WW = W>
        friend constexpr auto operator==(const iterator& x, const iterator& y)
            -> std::enable_if_t<equality_comparable<WW>, bool>
        {
            return x.value_ == y.value_;
        }

        template <typename WW = W>
        friend constexpr auto operator!=(const iterator& x, const iterator& y)
            -> std::enable_if_t<equality_comparable<WW>, bool>
        {
            return !(x == y);
        }

        template <typename WW = W>
        friend constexpr auto operator<(const iterator& x, const iterator& y)
            -> std::enable_if_t<totally_ordered<WW>, bool>
        {
            return x.value_ < y.value_;
        }

        template <typename WW = W>
        friend constexpr auto operator>(const iterator& x, const iterator& y)
            -> std::enable_if_t<totally_ordered<WW>, bool>
        {
            return y < x;
        }

        template <typename WW = W>
        friend constexpr auto operator<=(const iterator& x, const iterator& y)
            -> std::enable_if_t<totally_ordered<WW>, bool>
        {
            return !(y < x);
        }

        template <typename WW = W>
        friend constexpr auto operator>=(const iterator& x, const iterator& y)
            -> std::enable_if_t<totally_ordered<WW>, bool>
        {
            return !(x < y);
        }

        template <typename WW = W>
        friend constexpr auto operator+(iterator i, difference_type n)
            -> std::enable_if_t<detail::advanceable<WW>, iterator>
        {
            return i += n;
        }

        template <typename WW = W>
        friend constexpr auto operator+(difference_type n, iterator i)
            -> std::enable_if_t<detail::advanceable<WW>, iterator>
        {
            return i + n;
        }

        template <typename WW = W>
        friend constexpr auto operator-(iterator i, difference_type n)
            -> std::enable_if_t<detail::advanceable<WW>, iterator>
        {
            return i -= n;
        }

        template <typename WW = W>
        friend constexpr auto operator-(const iterator& x, const iterator& y)
            -> std::enable_if_t<detail::advanceable<WW>, difference_type>
        {
            using D = difference_type;
            if constexpr (integral<D>) {
                if constexpr (signed_integral<D>) {
                    return D(D(x.value_) - D(y.value_));
                } else {
                    return (y.value_ > x.value)
                        ? D(-D(y.value_ - x.value_))
                        : D(x.value_ - y.value_);
                }
            } else {
                return x.value_ - y.value_;
            }
        }
    };

    struct sentinel {
    private:
        Bound bound_ = Bound();

    public:
        sentinel() = default;
        constexpr explicit sentinel(Bound bound) : bound_(bound) {}

        friend constexpr bool operator==(const iterator& i, const sentinel& s)
        {
            return i.value_ == s.bound_;
        }

        friend constexpr bool operator==(const sentinel& s, const iterator& i)
        {
            return i == s;
        }

        friend constexpr bool operator!=(const iterator& i, const sentinel& s)
        {
            return !(i == s);
        }

        friend constexpr bool operator!=(const sentinel& s, const iterator& i)
        {
            return !(i == s);
        }

        template <typename WW = W>
        friend constexpr auto operator-(const iterator& i, const sentinel& s)
            -> std::enable_if_t<sized_sentinel_for<Bound, WW>, iter_difference_t<WW>>
        {
            return i.value_ - s.bound_;
        }

        template <typename WW = W>
        friend constexpr auto operator-(const sentinel& s, const iterator& i)
            -> std::enable_if_t<sized_sentinel_for<Bound, WW>, iter_difference_t<WW>>
        {
            return -(i - s);
        }
    };

    W value_ = W();
    Bound bound_ = Bound();

public:
    iota_view() = default;

    constexpr explicit iota_view(W value) : value_(value) {}

    constexpr iota_view(type_identity_t<W> value, type_identity_t<Bound> bound)
        : value_(value),
          bound_(bound)
    {}

    constexpr iota_view(iterator first, sentinel last)
        : iota_view(*first, last.bound_) {}

    constexpr iterator begin() const
    {
        return iterator{value_};
    }

    template <typename WW = W, std::enable_if_t<!same_as<WW, Bound>, int> = 0>
    constexpr auto end() const
    {
        if constexpr (same_as<Bound, unreachable_sentinel_t>) {
            return unreachable_sentinel;
        } else {
            return sentinel{bound_};
        }
    }

    template <typename WW = W, std::enable_if_t<same_as<WW, Bound>, int> = 0>
    constexpr iterator end() const
    {
        return iterator{bound_};
    }

    template <typename WW = W, typename BB = Bound, std::enable_if_t<
              (same_as<WW, BB> && detail::advanceable<W>) ||
              (integral<WW> && integral<BB>) ||
                                   sized_sentinel_for<BB, WW>, int> = 0>
    constexpr auto size() const
    {
        constexpr auto make_unsigned_like = [](auto i) {
            return std::make_unsigned_t<decltype(i)>(i);
        };

        if constexpr (integral<W> && integral<Bound>) {
            return (value_ < 0)
                ? ((bound_ < 0)
                  ? make_unsigned_like(-value_) - make_unsigned_like(-bound_)
                  : make_unsigned_like(bound_) - make_unsigned_like(-value_))
                : make_unsigned_like(bound_) - make_unsigned_like(value_);
        } else {
            make_unsigned_like(bound_ - value_);
        }
    }
};

template <typename W, typename Bound, std::enable_if_t<
    !integral<W> || !integral<Bound> ||
        (signed_integral<W> == signed_integral<Bound>), int> = 0>
iota_view(W, Bound) -> iota_view<W, Bound>;

template <typename W, typename Bound>
inline constexpr bool enable_borrowed_range<iota_view<W, Bound>> = true;

namespace views {

namespace detail {

struct iota_view_fn {
    template <typename W>
    constexpr auto operator()(W&& value) const
        noexcept(noexcept(iota_view{std::forward<W>(value)}))
        -> decltype(iota_view(std::forward<W>(value)))
    {
        return iota_view(std::forward<W>(value));
    }

    template <typename W, typename Bound>
    constexpr auto operator()(W&& value, Bound&& bound) const
        noexcept(noexcept(iota_view{std::forward<W>(value), std::forward<Bound>(bound)}))
        -> decltype(iota_view(std::forward<W>(value), std::forward<Bound>(bound)))
    {
        return iota_view{std::forward<W>(value), std::forward<Bound>(bound)};
    }
};

} // namespace detail

NANO_INLINE_VAR(detail::iota_view_fn, iota)

}

NANO_END_NAMESPACE

#endif

// nanorange/views/istream.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_ISTREAM_HPP_INCLUDED
#define NANORANGE_VIEWS_ISTREAM_HPP_INCLUDED





#include <iosfwd>

NANO_BEGIN_NAMESPACE

namespace detail {

struct StreamExtractable_req {
    template <typename Val, typename CharT, typename Traits>
    auto requires_(std::basic_istream<CharT, Traits>& is, Val& t)
        -> decltype(is >> t);
};

template <typename Val, typename CharT, typename Traits>
NANO_CONCEPT StreamExtractable =
    requires_<StreamExtractable_req, Val, CharT, Traits>;

} // namespace detail

template <typename Val, typename CharT, typename Traits = std::char_traits<CharT>>
struct basic_istream_view : view_interface<basic_istream_view<Val, CharT, Traits>> {

    static_assert(movable<Val>);
    static_assert(default_initializable<Val>);
    static_assert(detail::StreamExtractable<Val, CharT, Traits>);

    basic_istream_view() = default;

    constexpr explicit basic_istream_view(std::basic_istream<CharT, Traits>& stream)
        : stream_(std::addressof(stream))
    {}

    constexpr auto begin()
    {
        if (stream_) {
            *stream_ >> object_;
        }
        return iterator{*this};
    }

    constexpr default_sentinel_t end() const noexcept
    {
        return default_sentinel;
    }

private:
    struct iterator {
        using iterator_category = input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = Val;

        iterator() = default;

        constexpr explicit iterator(basic_istream_view& parent) noexcept
            : parent_(std::addressof(parent))
        {}

        // Disable move-only iterator until views support them properly
#if 0
        iterator(const iterator&) = delete;
        iterator(iterator&&) = default;

        iterator& operator=(const iterator&) = delete;
        iterator& operator=(iterator&&) = default;
#endif
        iterator& operator++()
        {
            *parent_->stream_ >> parent_->object_;
            return *this;
        }

        void operator++(int)
        {
            ++*this;
        }

        Val& operator*() const { return parent_->object_; }

        friend bool operator==(const iterator& x, default_sentinel_t)
        {
            return x.done();
        }

        friend bool operator==(default_sentinel_t s, const iterator& x)
        {
            return x == s;
        }

        friend bool operator!=(const iterator& x, default_sentinel_t s)
        {
            return !(x == s);
        }

        friend bool operator!=(default_sentinel_t s, const iterator& x)
        {
            return !(x == s);
        }

    private:
        [[nodiscard]] bool done() const
        {
            return parent_ == nullptr
                  || parent_->stream_ == nullptr
                  || !*parent_->stream_;
        }

        basic_istream_view* parent_{};
    };

    std::basic_istream<CharT, Traits>* stream_{};
    Val object_ = Val();
};

template <typename Val, typename CharT, typename Traits>
auto istream_view(std::basic_istream<CharT, Traits>& s)
    -> basic_istream_view<Val, CharT, Traits>
{
    return basic_istream_view<Val, CharT, Traits>{s};
}

NANO_END_NAMESPACE

#endif

// nanorange/views/join.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_JOIN_HPP_INCLUDED
#define NANORANGE_VIEWS_JOIN_HPP_INCLUDED





NANO_BEGIN_NAMESPACE

namespace detail {

template <typename V, bool InnerIsReference>
struct join_view_data {
    V base_ = V();
};

template <typename V>
struct join_view_data<V, false> {
    V base_ = V();
    all_view<range_reference_t<V>> inner_ = all_view<range_reference_t<V>>();
};

template <typename B>
constexpr auto join_view_iter_cat_helper()
{
    constexpr bool ref_is_glvalue = std::is_reference_v<range_reference_t<B>>;

    if constexpr (ref_is_glvalue) {
        using OuterCat = iterator_category_t<iterator_t<B>>;
        using InnerCat = iterator_category_t<iterator_t<range_reference_t<B>>>;

        if constexpr (derived_from<OuterCat, bidirectional_iterator_tag> &&
                      derived_from<InnerCat, bidirectional_iterator_tag>) {
            return bidirectional_iterator_tag{};
        } else if constexpr(derived_from<OuterCat, forward_iterator_tag> &&
                            derived_from<InnerCat, forward_iterator_tag>) {
            return forward_iterator_tag{};
        } else {
            return input_iterator_tag{};
        }
    } else {
        return input_iterator_tag{};
    }
}

}

namespace join_view_ {

template <typename V>
struct join_view : view_interface<join_view<V>> {
private:

    static_assert(input_range<V>);
    static_assert(view<V>);
    static_assert(input_range<range_reference_t<V>>);
    static_assert(std::is_reference_v<range_reference_t<V>> ||
                  view<range_value_t<V>>);

    using InnerRng = range_reference_t<V>;

    template<bool> struct sentinel;

    template <bool Const>
    struct iterator {

        friend struct sentinel<Const>;

        template <typename B>
        static constexpr bool ref_is_glvalue =
            std::is_reference_v<range_reference_t<B>>;

        using Base = detail::conditional_t<Const, const V, V>;

        // https://github.com/ericniebler/stl2/issues/604
        using Parent = detail::conditional_t<Const && ref_is_glvalue<Base>,
            const join_view, join_view>;

        iterator_t<Base> outer_ = iterator_t<Base>();
        iterator_t<range_reference_t<Base>> inner_
            = iterator_t<range_reference_t<Base>>();
        Parent* parent_ = nullptr;

        template <typename B = Base>
        constexpr decltype(auto) update_inner(range_reference_t<Base> x)
        {
            if constexpr (ref_is_glvalue<B>) {
                return (x);
            } else {
                return (parent_->data_.inner_ = views::all(x));
            }
        }

        constexpr void satisfy()
        {
            for (; outer_ !=  ranges::end(parent_->data_.base_); ++outer_) {
                auto& inner = update_inner(*outer_);
                inner_ = ranges::begin(inner);
                if (inner_ != ranges::end(inner)) {
                    return;
                }
            }

            if constexpr (ref_is_glvalue<Base>) {
                inner_ = iterator_t<range_reference_t<Base>>();
            }
        }

    public:
        //using iterator_concept = ...
        using iterator_category = decltype(detail::join_view_iter_cat_helper<Base>());
        using value_type = range_value_t<range_reference_t<Base>>;
        using difference_type = nano::common_type_t<
            range_difference_t<Base>,
            range_difference_t<range_reference_t<Base>>>;
        // Extension: legacy typedefs
        using pointer = iterator_t<Base>;
        using reference = range_reference_t<range_reference_t<Base>>;

        iterator() = default;

        constexpr iterator(Parent& parent, iterator_t<Base> outer)
            : outer_(std::move(outer)),
              parent_(std::addressof(parent))
        {
            satisfy();
        }

        // constexpr iterator(iterator<!Const>);

        constexpr decltype(auto) operator*() const { return *inner_; }

        template <typename B = Base>
        constexpr auto operator->() const
            -> std::enable_if_t<detail::has_arrow<B>, iterator_t<Base>>
        {
            return inner_;
        }

    private:
        template <typename B = Base>
        constexpr decltype(auto) get_inner_rng()
        {
            if constexpr (ref_is_glvalue<B>) {
                return (*outer_);
            } else {
                return (parent_->data_.inner_);
            }
        }

    public:
        template <typename B = Base>
        constexpr iterator& operator++()
        {
            auto&& inner_rng = get_inner_rng();

            if (++inner_ == ranges::end(inner_rng)) {
                ++outer_;
                satisfy();
            }

            return *this;
        }

        template <typename B = Base>
        constexpr auto operator++(int)
            -> std::enable_if_t<!(ref_is_glvalue<B> &&
                                  forward_range<B> &&
                                  forward_range<range_reference_t<B>>)>
        {
            ++*this;
        }

        template <typename B = Base>
        constexpr auto operator++(int)
            -> std::enable_if_t<ref_is_glvalue<B> &&
                                forward_range<B> &&
                                forward_range<range_reference_t<B>>,
                                iterator>
        {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        template <typename B = Base>
        constexpr auto operator--()
            -> std::enable_if_t<ref_is_glvalue<B> &&
                                bidirectional_range<B> &&
                                bidirectional_range<range_reference_t<B>>,
                                iterator&>
        {
            if (outer_ == ranges::end(parent_->data_.base_)) {
                inner_ = ranges::end(*outer_);
            }

            while (inner_ == ranges::begin(*outer_)) {
                inner_ = ranges::end(*--outer_);
            }
            --inner_;
            return *this;
        }

        template <typename B = Base>
        constexpr auto operator--(int)
            -> std::enable_if_t<ref_is_glvalue<B> &&
                                bidirectional_range<B> &&
                                bidirectional_range<range_reference_t<B>>,
                                iterator>
        {
            auto tmp = *this;
            --*this;
            return *tmp;
        }

        template <typename B = Base>
        friend constexpr auto operator==(const iterator& x, const iterator& y)
            -> std::enable_if_t<ref_is_glvalue<B> &&
                                equality_comparable<iterator_t<B>> &&
                                equality_comparable<iterator_t<range_reference_t<B>>>,
                                bool>
        {
            return x.outer_ == y.outer_ && x.inner_ == y.inner_;
        }

        template <typename B = Base>
        friend constexpr auto operator!=(const iterator& x, const iterator& y)
            -> std::enable_if_t<ref_is_glvalue<B> &&
                                equality_comparable<iterator_t<B>> &&
                                equality_comparable<iterator_t<range_reference_t<B>>>,
                                bool>
        {
            return !(x == y);
        }

        friend constexpr decltype(auto) iter_move(const iterator& i)
            noexcept(noexcept(ranges::iter_move(i.inner_)))
        {
            return ranges::iter_move(i.inner_);
        }

        friend constexpr void iter_swap(const iterator& x, const iterator& y)
            noexcept(noexcept(ranges::iter_swap(x.inner_, y.inner_)))
        {
            ranges::iter_swap(x.inner_, y.inner_);
        }
    };

    template <bool Const>
    struct sentinel {
    private:
        using Parent = detail::conditional_t<Const, const join_view, join_view>;
        using Base = detail::conditional_t<Const, const V, V>;

        sentinel_t<Base> end_ = sentinel_t<Base>();

    public:
        sentinel() = default;

        constexpr explicit sentinel(Parent& parent)
            : end_(ranges::end(parent.data_.base_))
        {}

        // constexpr sentinel(sentinel<!Const> s);

        friend constexpr bool operator==(const iterator<Const>& x, const sentinel& y)
        {
            return x.outer_ == y.end_;
        }

        friend constexpr bool operator!=(const iterator<Const>& x, const sentinel& y)
        {
            return !(x == y);
        }

        friend constexpr bool operator==(const sentinel& x, const iterator<Const>& y)
        {
            return y == x;
        }

        friend constexpr bool operator!=(const sentinel& x, const iterator<Const>& y)
        {
            return !(y == x);
        }
    };

    detail::join_view_data<V, std::is_reference_v<InnerRng>> data_{};

public:

    join_view() = default;

    constexpr explicit join_view(V base)
        : data_{std::move(base)}
    {}

    constexpr auto begin()
    {
        return iterator<detail::simple_view<V>>{*this, ranges::begin(data_.base_)};
    }

    template <typename VV = V,
              std::enable_if_t<input_range<const VV> &&
                               std::is_reference_v<range_reference_t<const VV>>, int> = 0>
    constexpr auto begin() const
    {
        return iterator<true>{*this, ranges::begin(data_.base_)};
    }

    constexpr auto end()
    {
        if constexpr (forward_range<V> &&
                      std::is_reference_v<InnerRng> && forward_range<InnerRng> &&
                      common_range<V> && common_range<InnerRng>) {
            return iterator<detail::simple_view<V>>{*this, ranges::end(data_.base_)};
        } else {
            return sentinel<detail::simple_view<V>>{*this};
        }
    }

    template <typename VV = V,
              std::enable_if_t<input_range<const VV> &&
                               std::is_reference_v<range_reference_t<const VV>>, int> = 0>
    constexpr auto end() const
    {
        if constexpr (forward_range<const V> &&
                      std::is_reference_v<range_reference_t<const V>> &&
                      forward_range<range_reference_t<const V>> &&
                      common_range<const V> &&
                      common_range<range_reference_t<const V>>) {
            return iterator<true>{*this, ranges::end(data_.base_)};
        } else {
            return sentinel<true>{*this};
        }
    }
};

template <typename R>
explicit join_view(R&&) -> join_view<all_view<R>>;

} // namespace join_view_

using join_view_::join_view;

namespace detail {

struct join_view_fn {

    template <typename E>
    constexpr auto operator()(E&& e) const
        -> decltype(join_view{std::forward<E>(e)})
    {
        return join_view{std::forward<E>(e)};
    }

};

template <>
inline constexpr bool is_raco<join_view_fn> = true;

}

namespace views {

NANO_INLINE_VAR(nano::detail::join_view_fn, join)

}

NANO_END_NAMESPACE

#endif


// nanorange/views/reverse.hpp
//
// Copyright (c) 2018 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_REVERSE_HPP_INCLUDED
#define NANORANGE_VIEWS_REVERSE_HPP_INCLUDED




#include <optional>

NANO_BEGIN_NAMESPACE

namespace detail {

template <bool IsCommonRange, typename>
struct reverse_view_cache {};

template <typename I>
struct reverse_view_cache<false, I> {
    std::optional<I> cached{};
};

}

template <typename V>
struct reverse_view
    : view_interface<reverse_view<V>>,
      private detail::reverse_view_cache<common_range<V>, iterator_t<V>> {

    static_assert(view<V>);
    static_assert(bidirectional_range<V>);

    reverse_view() = default;

    constexpr explicit reverse_view(V r)
        : base_(std::move(r))
    {}

    constexpr V base() const { return base_; }

    constexpr reverse_iterator<iterator_t<V>> begin()
    {
        if constexpr (common_range<V>) {
            return nano::make_reverse_iterator(ranges::end(base_));
        } else {
            auto& c = this->cached;
            if (!c.has_value()) {
                c = ranges::next(ranges::begin(base_), ranges::end(base_));
            }
            return nano::make_reverse_iterator(*c);
        }
    }

    template <typename VV = V>
    constexpr auto begin() const
        -> std::enable_if_t<common_range<const VV>,
                            reverse_iterator<iterator_t<const VV>>>
    {
        return nano::make_reverse_iterator(ranges::end(base_));
    }

    constexpr reverse_iterator<iterator_t<V>> end()
    {
        return nano::make_reverse_iterator(ranges::begin(base_));
    }

    template <typename VV = V>
    constexpr auto end() const
        -> std::enable_if_t<common_range<const VV>, reverse_iterator<iterator_t<const VV>>>
    {
        return nano::make_reverse_iterator(ranges::begin(base_));
    }

    template <typename VV = V, std::enable_if_t<sized_range<VV>, int> = 0>
    constexpr auto size()
    {
        return ranges::size(base_);
    }

    template <typename VV = V, std::enable_if_t<sized_range<const VV>, int> = 0>
    constexpr auto size() const
    {
        return ranges::size(base_);
    }

private:
    V base_ = V{};
};

template <typename R>
reverse_view(R&&) -> reverse_view<all_view<R>>;

namespace detail {

struct reverse_view_fn {
private:
    template <typename R>
    static constexpr auto impl(reverse_view<R> r, detail::priority_tag<1>)
    {
        return r.base();
    }

    template <typename I, subrange_kind K>
    static constexpr auto impl(subrange<reverse_iterator<I>, reverse_iterator<I>, K> s,
                               detail::priority_tag<1>)
    {
        if constexpr (K == subrange_kind::sized) {
            return subrange<I, I, K>(s.end().base(), s.begin().base(), s.size());
        } else {
            return subrange<I, I, K>(s.end().base(), s.begin().base());
        }
    }

    template <typename R>
    static constexpr auto impl(R&& r, detail::priority_tag<0>)
        -> decltype(reverse_view{std::forward<R>(r)})
    {
        return reverse_view{std::forward<R>(r)};
    }

public:
    template <typename R>
    constexpr auto operator()(R&& r) const
        -> decltype(impl(std::forward<R>(r), priority_tag<1>{}))
    {
        return impl(std::forward<R>(r), priority_tag<1>{});
    }
};

template <>
inline constexpr bool is_raco<reverse_view_fn> = true;

} // namespace detail

namespace views {

NANO_INLINE_VAR(nano::detail::reverse_view_fn, reverse)

} // namespace views

NANO_END_NAMESPACE

#endif

// nanorange/views/single.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_SINGLE_HPP_INCLUDED
#define NANORANGE_VIEWS_SINGLE_HPP_INCLUDED




NANO_BEGIN_NAMESPACE

template <typename T>
struct single_view : view_interface<single_view<T>> {
    static_assert(copy_constructible<T>);
    static_assert(std::is_object<T>::value);

    single_view() = default;

    constexpr explicit single_view(const T& t)
        : value_(t)
    {}

    constexpr explicit single_view(T&& t)
        : value_(std::move(t))
    {}

    template <typename... Args,
              std::enable_if_t<constructible_from<T, Args...>, int> = 0>
    constexpr single_view(std::in_place_t, Args&&... args)
        : value_{std::in_place, std::forward<Args>(args)...}
    {}

    constexpr T* begin() noexcept { return data(); }
    constexpr const T* begin() const noexcept { return data(); }

    constexpr T* end() noexcept { return data() + 1; }
    constexpr const T* end() const noexcept { return data() + 1; }

    static constexpr std::size_t size() { return 1; }

    constexpr T* data() noexcept { return value_.operator->(); }
    constexpr const T* data() const noexcept { return value_.operator->(); }

private:
    detail::semiregular_box<T> value_;
};

namespace views {

namespace detail {

struct single_view_fn {
    template <typename T>
    constexpr auto operator()(T&& t) const
        noexcept(noexcept(single_view{std::forward<T>(t)}))
        -> decltype(single_view{std::forward<T>(t)})
    {
        return single_view{std::forward<T>(t)};
    }
};

}

NANO_INLINE_VAR(detail::single_view_fn, single)

}


NANO_END_NAMESPACE

#endif
// nanorange/views/split.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_SPLIT_HPP_INCLUDED
#define NANORANGE_VIEWS_SPLIT_HPP_INCLUDED








NANO_BEGIN_NAMESPACE

namespace detail {

template <auto> struct require_constant;

struct tiny_range_concept {

    template <typename R>
    auto requires_() -> decltype(
        std::declval<require_constant<std::remove_reference_t<R>::size()>>()
    );

    template <typename R>
    static auto test(long) -> std::false_type;

    template <typename R>
    static auto test(int) -> std::enable_if_t<
        sized_range<R> &&
        detail::requires_<tiny_range_concept, R> &&
        (std::remove_reference_t<R>::size() <= 1),
        std::true_type>;
};

template <typename R>
NANO_CONCEPT tiny_range = decltype(tiny_range_concept::test<R>(0))::value;

template <typename V, typename P, bool NotForwardRange>
struct split_view_data {
    V base_ = V();
    P pattern_ = P();
};

template <typename V, typename P>
struct split_view_data<V, P, true> {
    V base_ = V();
    P pattern_ = P();
    iterator_t<V> current_ = iterator_t<V>();
};

} // namespace detail

namespace split_view_ {

template <typename V, typename Pattern>
struct split_view : view_interface<split_view<V, Pattern>> {
private:

    static_assert(input_range<V>);
    static_assert(forward_range<Pattern>);
    static_assert(view<V>);
    static_assert(view<Pattern>);
    static_assert(indirectly_comparable<iterator_t<V>, iterator_t<Pattern>,
                                        ranges::equal_to>);
    static_assert(forward_range<V> || detail::tiny_range<Pattern>);

    detail::split_view_data<V, Pattern, !forward_range<V>> data_{};



    template <bool>
    struct inner_iterator;

    template <bool Const>
    struct outer_iterator {
    private:
        friend struct outer_iterator<!Const>;
        friend struct inner_iterator<Const>;

        using Parent = detail::conditional_t<Const, const split_view, split_view>;
        using Base = detail::conditional_t<Const, const V, V>;
        Parent* parent_ = nullptr;
        iterator_t<Base> current_ = iterator_t<Base>();

        constexpr decltype(auto) get_current()
        {
            if constexpr (forward_range<V>) {
                return (current_);
            } else {
                return (parent_->data_.current_);
            }
        }

        constexpr decltype(auto) get_current() const
        {
            if constexpr (forward_range<V>) {
                return (current_);
            } else {
                return (parent_->data_.current_);
            }
        }

        constexpr bool done() const
        {
            return get_current() == ranges::end(parent_->data_.base_);
        }

    public:
        // FIXME: iterator_concept
        using iterator_category = detail::conditional_t<
            forward_range<Base>, forward_iterator_tag, input_iterator_tag>;

        struct value_type {
        private:
            outer_iterator i_ = outer_iterator();

        public:
            value_type() = default;

            constexpr explicit value_type(outer_iterator i)
                : i_(std::move(i))
            {}

            constexpr inner_iterator<Const> begin() const
            {
                return inner_iterator<Const>{i_};
            }

            constexpr default_sentinel_t end() const
            {
                return default_sentinel;
            }
        };

        using difference_type = range_difference_t<Base>;
        // Extension: legacy typedefs
        using pointer = void;
        using reference = value_type;

        outer_iterator() = default;

        template <typename B = Base, std::enable_if_t<!forward_range<B>, int> = 0>
        constexpr explicit outer_iterator(Parent& parent)
            : parent_(std::addressof(parent))
        {}

        template <typename B = Base, std::enable_if_t<forward_range<B>, int> = 0>
        constexpr outer_iterator(Parent& parent, iterator_t<Base> current)
            : parent_(std::addressof(parent)),
              current_(std::move(current))
        {}

        template <typename I,
                  std::enable_if_t<same_as<I, outer_iterator<!Const>>, int> = 0,
                  bool C = Const, typename VV = V,
                  std::enable_if_t<C &&
                      convertible_to<iterator_t<VV>, iterator_t<const VV>>, int> = 0>
        constexpr outer_iterator(I i)
            : parent_(i.parent_),
              current_(std::move(i.current_))
        {}

        constexpr value_type operator*() const { return value_type{*this}; }

        constexpr outer_iterator& operator++()
        {
            const auto end = ranges::end(parent_->data_.base_);
            if (get_current() == end) {
                return *this;
            }
            const auto [pbegin, pend] = subrange{parent_->data_.pattern_};
            if (pbegin == pend) {
                ++get_current();
            } else {
                do {
                    const auto [b, p] = ranges::mismatch(get_current(), end,
                                                         pbegin, pend);
                    if (p == pend) {
                        get_current() = b;
                        break;
                    }
                } while (++get_current() != end);
            }
            return *this;
        }

        constexpr decltype(auto) operator++(int)
        {
            if constexpr (forward_range<Base>) {
                auto tmp = *this;
                ++*this;
                return tmp;
            } else {
                ++*this;
            }
        }

        template <typename B = Base>
        friend constexpr auto operator==(const outer_iterator& x, const outer_iterator& y)
            -> std::enable_if_t<forward_range<B>, bool>
        {
            return x.current_ == y.current_;
        }

        template <typename B = Base>
        friend constexpr auto operator!=(const outer_iterator& x, const outer_iterator& y)
            -> std::enable_if_t<forward_range<B>, bool>
        {
            return !(x == y);
        }

        friend constexpr bool operator==(const outer_iterator& x, default_sentinel_t)
        {
            return x.done();
        }

        friend constexpr bool operator==(default_sentinel_t d, const outer_iterator& x)
        {
            return x == d;
        }

        friend constexpr bool operator!=(const outer_iterator& x, default_sentinel_t d)
        {
            return !(x == d);
        }

        friend constexpr bool operator!=(default_sentinel_t d, const outer_iterator& x)
        {
            return !(x == d);
        }
    };

    template <bool Const>
    struct inner_iterator {
    private:
        using Base = detail::conditional_t<Const, const V, V>;
        static constexpr bool NoReallyGccConst = Const;
        outer_iterator<Const> i_ = outer_iterator<NoReallyGccConst>();
        bool incremented_ = false;

        constexpr bool done() const
        {
            auto cur = i_.get_current();
            auto end = ranges::end(i_.parent_->data_.base_);
            if (cur == end) {
                return true;
            }
            auto [pcur, pend] = subrange{i_.parent_->data_.pattern_};
            if (pcur == pend) {
                return incremented_;
            }
            do {
                if (*cur != *pcur) {
                    return false;
                }
                if (++pcur == pend) {
                    return true;
                }
            } while (++cur != end);
            return false;
        }

        constexpr decltype(auto) get_outer_current() const { return i_.get_current(); }

    public:
        using iterator_category = detail::conditional_t<
            derived_from<iterator_category_t<iterator_t<Base>>, forward_iterator_tag>,
                forward_iterator_tag,
                input_iterator_tag>;
        using value_type = range_value_t<Base>;
        using difference_type = range_difference_t<Base>;

        inner_iterator() = default;

        constexpr explicit inner_iterator(outer_iterator<Const> i)
            : i_(std::move(i))
        {}

        constexpr decltype(auto) operator*() const { return *i_.get_current(); }

        constexpr inner_iterator& operator++()
        {
            incremented_ = true;
            if constexpr (!forward_range<Base>) {
                if constexpr (Pattern::size() == 0) {
                    return *this;
                }
            }
            ++i_.get_current();
            return *this;
        }


        constexpr decltype(auto) operator++(int)
        {
            if constexpr (forward_range<V>) {
                auto tmp = *this;
                ++*this;
                return tmp;
            } else {
                ++*this;
            }
        }

        template <typename B = Base>
        friend constexpr auto operator==(const inner_iterator& x, const inner_iterator& y)
            -> std::enable_if_t<forward_range<B>, bool>
        {
            return x.get_outer_current() == y.get_outer_current();
        }

        template <typename B = Base>
        friend constexpr auto operator!=(const inner_iterator& x, const inner_iterator& y)
            -> std::enable_if_t<forward_range<B>, bool>
        {
            return !(x == y);
        }

        friend constexpr bool operator==(const inner_iterator& x, default_sentinel_t)
        {
            return x.done();
        }

        friend constexpr bool operator==(default_sentinel_t d, const inner_iterator& x)
        {
            return x == d;
        }

        friend constexpr bool operator!=(const inner_iterator& x, default_sentinel_t d)
        {
            return !(x == d);
        }

        friend constexpr bool operator!=(default_sentinel_t d, const inner_iterator& x)
        {
            return !(x == d);
        }

        friend constexpr decltype(auto) iter_move(const inner_iterator& i)
            noexcept(noexcept(ranges::iter_move(i.get_outer_current())))
        {
            return ranges::iter_move(i.get_outer_current());
        }

        template <typename B = Base>
        friend constexpr auto iter_swap(const inner_iterator& x, const inner_iterator& y)
            noexcept(noexcept(ranges::iter_swap(x.get_outer_current(), y.get_outer_current())))
            -> std::enable_if_t<indirectly_swappable<B>>
        {
            ranges::iter_swap(x.get_outer_current(), y.get_outer_current());
        }
    };

public:
    split_view() = default;

    constexpr split_view(V base, Pattern pattern)
        : data_{std::move(base), std::move(pattern)}
    {}

    template <typename R,
              std::enable_if_t<constructible_from<V, all_view<R>>, int> = 0,
              std::enable_if_t<
                  constructible_from<Pattern, single_view<range_value_t<R>>>, int> = 0,
              std::enable_if_t<input_range<R>, int> = 0>
    constexpr split_view(R&& r, range_value_t<R> e)
        : data_{views::all(std::forward<R>(r)), single_view{std::move(e)}}
    {}

    constexpr auto begin()
    {
        if constexpr (forward_range<V>) {
            return outer_iterator<detail::simple_view<V>>{*this, ranges::begin(data_.base_)};
        } else {
            data_.current_ = ranges::begin(data_.base_);
            return outer_iterator<false>{*this};
        }
    }

    template <typename VV = V,
              std::enable_if_t<forward_range<VV> && forward_range<const VV>, int> = 0>
    constexpr auto begin() const
    {
        return outer_iterator<true>{*this, ranges::begin(data_.base_)};
    }

    template <typename VV = V,
              std::enable_if_t<forward_range<VV> && common_range<VV>, int> = 0>
    constexpr auto end()
    {
        return outer_iterator<detail::simple_view<V>>{*this, ranges::end(data_.base_)};
    }

    constexpr auto end() const
    {
        if constexpr (forward_range<V> &&
                      forward_range<const V> &&
                      common_range<const V>) {
            return outer_iterator<true>{*this, ranges::end(data_.base_)};
        } else {
            return default_sentinel;
        }
    }
};

template <typename R, typename P>
split_view(R&&, P&&) -> split_view<all_view<R>, all_view<P>>;

template <typename R, std::enable_if_t<input_range<R>, int> = 0>
split_view(R&&, range_value_t<R>)
    -> split_view<all_view<R>, single_view<range_value_t<R>>>;

} // namespace split_view_

using split_view_::split_view;

namespace detail {

struct split_view_fn {

    template <typename E, typename F>
    constexpr auto operator()(E&& e, F&& f) const
        -> decltype(split_view{std::forward<E>(e), std::forward<F>(f)})
    {
        return split_view{std::forward<E>(e), std::forward<F>(f)};
    }

    template <typename P>
    constexpr auto operator()(P&& p) const
    {
        return detail::rao_proxy{
            [p = std::forward<P>(p)](auto&& r) mutable
#ifndef NANO_MSVC_LAMBDA_PIPE_WORKAROUND
                 -> decltype(split_view{std::forward<decltype(r)>(r), std::declval<P&&>()})
#endif
            {
                return split_view{std::forward<decltype(r)>(r), std::move(p)};
            }};
    }
};

} // namespace detail

namespace views {

NANO_INLINE_VAR(nano::detail::split_view_fn, split)

}

NANO_END_NAMESPACE

#endif


// nanorange/views/take.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_TAKE_HPP_INCLUDED
#define NANORANGE_VIEWS_TAKE_HPP_INCLUDED







NANO_BEGIN_NAMESPACE

template <typename V>
struct take_view : view_interface<take_view<V>> {
private:
    static_assert(view<V>);

    V base_ = V();
    range_difference_t<V> count_ = 0;

    template <bool Const>
    struct sentinel {
    private:
        friend struct sentinel<!Const>;
        using Base = detail::conditional_t<Const, const V, V>;
        using CI = counted_iterator<iterator_t<Base>>;

        sentinel_t<Base> end_ = sentinel_t<Base>();

    public:
        sentinel() = default;

        constexpr explicit sentinel(sentinel_t<Base> end)
            : end_(std::move(end))
        {}

        // Use deduced type to avoid constraint recursion in GCC8
        template <typename S,
            std::enable_if_t<same_as<S, sentinel<!Const>>, int> = 0,
            bool C = Const, typename VV = V,
            std::enable_if_t<C &&
                convertible_to<sentinel_t<VV>, sentinel_t<Base>>, int> = 0>
        constexpr explicit sentinel(S s)
            : end_(std::move(s.end_))
        {}

        constexpr sentinel_t<Base> base() const { return end_; }

        friend constexpr bool operator==(const CI& y, const sentinel& x)
        {
            return y.count() == 0 || y.base() == x.end_;
        }

        friend constexpr bool operator==(const sentinel& x, const CI& y)
        {
            return y == x;
        }

        friend constexpr bool operator!=(const CI& y, const sentinel& x)
        {
            return !(y == x);
        }

        friend constexpr bool operator!=(const sentinel& x, const CI& y)
        {
            return !(y == x);
        }
    };

public:
    take_view() = default;

    constexpr take_view(V base, range_difference_t<V> count)
        : base_(std::move(base)),
          count_(count)
    {}

    constexpr V base() const { return base_; }

    template <typename VV = V, std::enable_if_t<!detail::simple_view<VV>, int> = 0>
    constexpr auto begin()
    {
        if constexpr (sized_range<V>) {
            if constexpr (random_access_range<V>) {
                return ranges::begin(base_);
            } else {
                // N.B spec doesn't static_cast here, but I'm pretty
                // sure it should
                return counted_iterator{ranges::begin(base_),
                                        static_cast<range_difference_t<V>>(size())};
            }
        } else {
            return counted_iterator{ranges::begin(base_), count_};
        }
    }

    template <typename VV = V, std::enable_if_t<range<const VV>, int> = 0>
    constexpr auto begin() const
    {
        if constexpr (sized_range<const V>) {
            if constexpr (random_access_range<V>) {
                return ranges::begin(base_);
            } else {
                // N.B spec doesn't static_cast here, but I'm pretty
                // sure it should
                return counted_iterator{ranges::begin(base_),
                                        static_cast<range_difference_t<V>>(size())};
            }
        } else {
            return counted_iterator{ranges::begin(base_), count_};
        }
    }

    template <typename VV = V, std::enable_if_t<!detail::simple_view<VV>, int> = 0>
    constexpr auto end()
    {
        if constexpr (sized_range<V>) {
            if constexpr (random_access_range<V>) {
                return ranges::begin(base_) + size();
            } else {
                return default_sentinel;
            }
        } else {
            return sentinel<false>{ranges::end(base_)};
        }
    }

    template <typename VV = V, std::enable_if_t<range<const VV>, int> = 0>
    constexpr auto end() const
    {
        if constexpr (sized_range<const V>) {
            if constexpr (random_access_range<const V>) {
                return ranges::begin(base_) + size();
            } else {
                return default_sentinel;
            }
        } else {
            return sentinel<true>{ranges::end(base_)};
        }
    }

    template <typename VV = V, std::enable_if_t<sized_range<VV>, int> = 0>
    constexpr auto size()
    {
        auto n = ranges::size(base_);
        return ranges::min(n, static_cast<decltype(n)>(count_));
    }

    template <typename VV = V, std::enable_if_t<sized_range<const VV>, int> = 0>
    constexpr auto size() const
    {
        auto n = ranges::size(base_);
        return ranges::min(n, static_cast<decltype(n)>(count_));
    }
};

template <typename R, std::enable_if_t<range<R>, int> = 0>
take_view(R&&, range_difference_t<R>) -> take_view<all_view<R>>;

namespace detail {

#ifdef NANO_MSVC_LAMBDA_PIPE_WORKAROUND
template <typename R>
using take_view_helper_t = take_view<all_view<R>>;
#endif

struct take_view_fn {

    template <typename C>
    constexpr auto operator()(C c) const
    {

        return detail::rao_proxy{[c = std::move(c)](auto&& r) mutable
#ifdef NANO_MSVC_LAMBDA_PIPE_WORKAROUND
            -> take_view_helper_t<decltype(r)>
#else
            -> decltype(take_view{std::forward<decltype(r)>(r), std::declval<C&&>()})
#endif
        {
            return take_view{std::forward<decltype(r)>(r), std::move(c)};
        }};
    }

    template <typename E, typename F>
    constexpr auto operator()(E&& e, F&& f) const
        -> decltype(take_view{std::forward<E>(e), std::forward<F>(f)})
    {
        return take_view{std::forward<E>(e), std::forward<F>(f)};
    }

};

}

namespace views {

NANO_INLINE_VAR(nano::detail::take_view_fn, take)

}

NANO_END_NAMESPACE

#endif

// nanorange/views/take_while.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_TAKE_WHILE_HPP_INCLUDED
#define NANORANGE_VIEWS_TAKE_WHILE_HPP_INCLUDED






NANO_BEGIN_NAMESPACE

template <typename R, typename Pred>
struct take_while_view : view_interface<take_while_view<R, Pred>> {
private:
    static_assert(view<R>);
    // FIXME: Should be input_range (GCC9)
    static_assert(input_iterator<iterator_t<R>>);
    static_assert(std::is_object_v<Pred>);
    static_assert(indirect_unary_predicate<const Pred, iterator_t<R>>);

    template <bool Const>
    struct sentinel {
    private:
        friend struct sentinel<!Const>;
        using base_t = detail::conditional_t<Const, const R, R>;
        sentinel_t<base_t> end_ = sentinel_t<base_t>();
        const Pred* pred_{};

    public:
        sentinel() = default;

        constexpr explicit sentinel(sentinel_t<base_t>(end), const Pred* pred)
            : end_(std::move(end)),
              pred_(pred)
        {}

        // Use deduced type to avoid constraint recursion in GCC8
        template <typename S,
                  std::enable_if_t<same_as<S, sentinel<!Const>>, int> = 0,
                  bool C = Const, typename VV = R,
                  std::enable_if_t<C &&
                      convertible_to<sentinel_t<VV>, sentinel_t<base_t>>, int> = 0>
        constexpr sentinel(S s)
            : end_(std::move(s.end_)),
              pred_(s.pred_)
        {}

        constexpr sentinel_t<base_t> base() const { return end_; }

        // Make these friend functions templates to keep MSVC happy
#if (defined(_MSC_VER) && _MSC_VER < 1922)
        template <typename = void>
#endif
        friend constexpr bool operator==(const iterator_t<base_t>& x, const sentinel& y)
        {
            return y.end_ == x || !nano::invoke(*y.pred_, *x);
        }

#if (defined(_MSC_VER) && _MSC_VER < 1922)
        template <typename = void>
#endif
        friend constexpr bool operator==(const sentinel& y, const iterator_t<base_t>& x)
        {
            return x == y;
        }

#if (defined(_MSC_VER) && _MSC_VER < 1922)
        template <typename = void>
#endif
        friend constexpr bool operator!=(const iterator_t<base_t>& x, const sentinel& y)
        {
            return !(x == y);
        }

#if (defined(_MSC_VER) && _MSC_VER < 1922)
        template <typename = void>
#endif
        friend constexpr bool operator!=(const sentinel& y, const iterator_t<base_t>& x)
        {
            return !(x == y);
        }
    };

    R base_;
    detail::semiregular_box<Pred> pred_;

public:
    take_while_view() = default;

    constexpr take_while_view(R base, Pred pred)
        : base_(std::move(base)),
          pred_(std::move(pred))
    {}

    constexpr R base() const { return base_; }

    constexpr const Pred& pred() const { return *pred_; }

    template <typename RR = R, std::enable_if_t<!detail::simple_view<RR>, int> = 0>
    constexpr auto begin() { return ranges::begin(base_); }

    template <typename RR = R, std::enable_if_t<range<const RR>, int> = 0>
    constexpr auto begin() const { return ranges::begin(base_); }

    template <typename RR = R, std::enable_if_t<!detail::simple_view<RR>, int> = 0>
    constexpr auto end()
    {
        return sentinel<false>{ranges::end(base_), std::addressof(*pred_)};
    }

    template <typename RR = R, std::enable_if_t<range<const RR>, int> = 0>
    constexpr auto end() const
    {
        return sentinel<true>{ranges::end(base_), std::addressof(*pred_)};
    }
};

template <typename R, typename Pred>
take_while_view(R&&, Pred) -> take_while_view<all_view<R>, Pred>;

namespace detail {

struct take_while_view_fn {

    template <typename E, typename F>
    constexpr auto operator()(E&& e, F&& f) const
    -> decltype(take_while_view{std::forward<E>(e), std::forward<F>(f)})
    {
        return take_while_view{std::forward<E>(e), std::forward<F>(f)};
    }

    template <typename Pred>
    constexpr auto operator()(Pred&& pred) const
    {
        return detail::rao_proxy{[p = std::forward<Pred>(pred)](auto&& r) mutable
#ifndef NANO_MSVC_LAMBDA_PIPE_WORKAROUND
            -> decltype(take_while_view{std::forward<decltype(r)>(r), std::declval<Pred&&>()})
#endif
        {
            return take_while_view{std::forward<decltype(r)>(r), std::move(p)};
        }};
    }

};

}

namespace views {

NANO_INLINE_VAR(nano::detail::take_while_view_fn, take_while)

}

NANO_END_NAMESPACE

#endif

// nanorange/views/transform.hpp
//
// Copyright (c) 2019 Tristan Brindle (tcbrindle at gmail dot com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NANORANGE_VIEWS_TRANSFORM_HPP_INCLUDED
#define NANORANGE_VIEWS_TRANSFORM_HPP_INCLUDED






NANO_BEGIN_NAMESPACE

namespace transform_view_ {

template <typename V, typename F>
struct transform_view : view_interface<transform_view<V, F>>
{
private:
    // FIXME: Weird GCC9 bug strikes again!
    // These two conditions are equivalent to input_range<V>, but if we test
    // that directly with GCC9 then it fails?!
    static_assert(range<V>);
    static_assert(input_iterator<iterator_t<V>>);
    static_assert(view<V>);
    static_assert(copy_constructible<F>);
    static_assert(std::is_object_v<F>);
    static_assert(regular_invocable<F&, range_reference_t<V>>);

    template <bool> struct sentinel;

    template <bool Const>
    struct iterator {
    private:
        friend struct iterator<!Const>;
        friend struct sentinel<Const>;

        using Parent =
            detail::conditional_t<Const, const transform_view, transform_view>;
        using Base = detail::conditional_t<Const, const V, V>;

        iterator_t<Base> current_ = iterator_t<Base>();
        Parent* parent_ = nullptr;

        static constexpr bool iter_move_noexcept_helper =
            noexcept(nano::invoke(*parent_->fun_, *current_));

    public:
        using iterator_category = detail::conditional_t<
            derived_from<iterator_category_t<iterator_t<Base>>, contiguous_iterator_tag>,
            random_access_iterator_tag,
            iterator_category_t<iterator_t<Base>>>;

        using value_type =
            remove_cvref_t<invoke_result_t<F&, range_reference_t<Base>>>;
        using difference_type = range_difference_t<Base>;
        // Extension: legacy typedefs
        using pointer = void;
        using reference = invoke_result_t<F&, range_reference_t<Base>>;

        iterator() = default;

        constexpr iterator(Parent& parent, iterator_t<Base> current)
            : current_(std::move(current)),
              parent_(std::addressof(parent))
        {}

        template <typename I,
            std::enable_if_t<same_as<I, iterator<!Const>>, int> = 0,
            bool C = Const, typename VV = V, std::enable_if_t<
            C && convertible_to<iterator_t<VV>, iterator_t<Base>>, int> = 0>
        constexpr iterator(I i)
            : current_(std::move(i.current_)),
              parent_(i.parent_)
        {}

        constexpr iterator_t<Base> base() const { return current_; }

        constexpr decltype(auto) operator*() const
        {
            return nano::invoke(*parent_->fun_, *current_);
        }

        constexpr iterator& operator++() { ++current_; return *this; }

        constexpr auto operator++(int)
        {
            if constexpr (forward_range<Base>) {
                auto tmp = *this;
                ++*this;
                return tmp;
            } else {
                ++current_;
            }
        }

        template <typename B = Base>
        constexpr auto operator--()
            -> std::enable_if_t<bidirectional_range<B>, iterator&>
        {
            --current_;
            return *this;
        }

        template <typename B = Base>
        constexpr auto operator--(int)
            -> std::enable_if_t<bidirectional_range<B>, iterator>
        {
            auto tmp = *this;
            --*this;
            return tmp;
        }

        template <typename B = Base>
        constexpr auto operator+=(difference_type n)
            -> std::enable_if_t<random_access_range<B>, iterator&>
        {
            current_ += n;
            return *this;
        }

        template <typename B = Base>
        constexpr auto operator-=(difference_type n)
            -> std::enable_if_t<random_access_range<B>, iterator&>
        {
            current_ -= n;
            return *this;
        }

        template <typename B = Base, typename = std::enable_if_t<random_access_range<B>>>
        constexpr decltype(auto) operator[](difference_type n) const
        {
            return nano::invoke(*parent_->fun_, current_[n]);
        }

        template <typename B = Base>
        friend constexpr auto operator==(const iterator& x, const iterator& y)
            -> std::enable_if_t<equality_comparable<iterator_t<B>>, bool>
        {
            return x.current_ == y.current_;
        }

        template <typename B = Base>
        friend constexpr auto operator!=(const iterator& x, const iterator& y)
            -> std::enable_if_t<equality_comparable<iterator_t<B>>, bool>
        {
            return !(x == y);
        }

        template <typename B = Base>
        friend constexpr auto operator<(const iterator& x, const iterator& y)
            -> std::enable_if_t<random_access_range<B>, bool>
        {
            return x.current_ < y.current_;
        }

        template <typename B = Base>
        friend constexpr auto operator>(const iterator& x, const iterator& y)
            -> std::enable_if_t<random_access_range<B>, bool>
        {
            return y < x;
        }

        template <typename B = Base>
        friend constexpr auto operator<=(const iterator& x, const iterator& y)
            -> std::enable_if_t<random_access_range<B>, bool>
        {
            return !(y < x);
        }

        template <typename B = Base>
        friend constexpr auto operator>=(const iterator& x, const iterator& y)
            -> std::enable_if_t<random_access_range<B>, bool>
        {
            return !(x < y);
        }

        template <typename B = Base>
        friend constexpr auto operator+(iterator i, difference_type n)
            -> std::enable_if_t<random_access_range<B>, iterator>
        {
            return iterator{*i.parent_, i.current_ + n};
        }

        template <typename B = Base>
        friend constexpr auto operator+(difference_type n, iterator i)
            -> std::enable_if_t<random_access_range<B>, iterator>
        {
            return iterator{*i.parent_, i.current_ + n};
        }

        template <typename B = Base>
        friend constexpr auto operator-(iterator i, difference_type n)
            -> std::enable_if_t<random_access_range<B>, iterator>
        {
            return iterator{*i.parent_, i.current_ - n};
        }

        template <typename B = Base>
        friend constexpr auto operator-(const iterator& x, const iterator& y)
            -> std::enable_if_t<random_access_range<B>, difference_type>
        {
            return x.current_ - y.current_;
        }

        friend constexpr decltype(auto) iter_move(const iterator& i)
            noexcept(iter_move_noexcept_helper)
        {
            if constexpr (std::is_lvalue_reference_v<decltype(*i)>) {
                return std::move(*i);
            } else {
                return *i;
            }
        }

        template <typename B = Base>
        friend constexpr auto iter_swap(const iterator& x, const iterator& y)
            noexcept(noexcept(ranges::iter_swap(x.current_, y.current_)))
            -> std::enable_if_t<indirectly_swappable<iterator_t<B>>>
        {
            return ranges::iter_swap(x.current_, y.current_);
        }
    };

    template <bool Const>
    struct sentinel
    {
    private:
        friend struct sentinel<!Const>;

        using Parent =
            detail::conditional_t<Const, const transform_view, transform_view>;
        using Base = detail::conditional_t<Const, const V, V>;
        sentinel_t<Base> end_ = sentinel_t<Base>();

    public:
        sentinel() = default;

        constexpr explicit sentinel(sentinel_t<Base> end)
            : end_(std::move(end))
        {}

        template <typename S,
            std::enable_if_t<same_as<S, sentinel<!Const>>, int> = 0,
            bool C = Const, typename VV = V, std::enable_if_t<
            C && convertible_to<sentinel_t<VV>, sentinel_t<Base>>, int> = 0>
        constexpr sentinel(S i)
            : end_(std::move(i.end_))
        {}

        constexpr sentinel_t<Base> base() { return end_; }

        friend constexpr bool operator==(const iterator<Const>& x, const sentinel& y)
        {
            return x.base() == y.end_;
        }

        friend constexpr bool operator==(const sentinel& x, const iterator<Const>& y)
        {
            return x.end_ == y.base();
        }

        friend constexpr bool operator!=(const iterator<Const>& x, const sentinel& y)
        {
            return !(x == y);
        }

        friend constexpr bool operator!=(const sentinel& x, const iterator<Const>& y)
        {
            return !(x == y);
        }

        template <typename B = Base>
        friend constexpr auto operator-(const iterator<Const>& x, const sentinel& y)
            -> std::enable_if_t<sized_sentinel_for<sentinel_t<B>, iterator_t<B>>,
                                range_difference_t<B>>
        {
            return x.current_ - y.end_;
        }

        template <typename B = Base>
        friend constexpr auto operator-(const sentinel& x, const iterator<Const>& y)
            -> std::enable_if_t<sized_sentinel_for<sentinel_t<B>, iterator_t<B>>,
                                range_difference_t<B>>
        {
            x.end_ - y.current_;
        }
    };

    V base_ = V();
    detail::semiregular_box<F> fun_;

public:
    transform_view() = default;

    constexpr transform_view(V base, F fun)
        : base_(std::move(base)),
          fun_(std::move(fun))
    {}

    constexpr V base() const { return base_; }

    constexpr iterator<false> begin()
    {
        return iterator<false>{*this, ranges::begin(base_)};
    }

    template <typename VV = V, std::enable_if_t<
        range<const VV> &&
        regular_invocable<const F&, range_reference_t<const VV>>, int> = 0>
    constexpr iterator<true> begin() const
    {
        return iterator<true>{*this, ranges::begin(base_)};
    }

    constexpr auto end()
    {
        if constexpr (common_range<V>) {
            return iterator<false>{*this, ranges::end(base_)};
        } else {
            return sentinel<false>{ranges::end(base_)};
        }
    }

    template <typename VV = V, std::enable_if_t<
        range<const VV> &&
        regular_invocable<const F&, range_reference_t<const VV>>, int> = 0>
    constexpr auto end() const
    {
        if constexpr (common_range<V>) {
            return iterator<true>{*this, ranges::end(base_)};
        } else {
            return sentinel<true>{ranges::end(base_)};
        }
    }

    template <typename VV = V, std::enable_if_t<sized_range<VV>, int> = 0>
    constexpr auto size() { return ranges::size(base_); }

    template <typename VV = V, std::enable_if_t<sized_range<const VV>, int> = 0>
    constexpr auto size() const { return ranges::size(base_); }

};

template <typename R, typename F, std::enable_if_t<
    input_range<all_view<R>> &&
    copy_constructible<F> &&
    std::is_object_v<F> &&
    regular_invocable<F&, range_reference_t<all_view<R>>>, int> = 0>
transform_view(R&&, F) -> transform_view<all_view<R>, F>;

} // namespace transform_view_

using transform_view_::transform_view;

namespace detail {

struct transform_view_fn {
    template <typename E, typename F>
    constexpr auto operator()(E&& e, F&& f) const
        -> decltype(transform_view{std::forward<E>(e), std::forward<F>(f)})
    {
        return transform_view{std::forward<E>(e), std::forward<F>(f)};
    }

    template <typename F>
    constexpr auto operator()(F f) const
    {
        return detail::rao_proxy{[f = std::move(f)](auto&& r) mutable
#ifndef NANO_MSVC_LAMBDA_PIPE_WORKAROUND
            -> decltype(transform_view{std::forward<decltype(r)>(r), std::declval<F&&>()})
#endif
        {
            return transform_view{std::forward<decltype(r)>(r), std::move(f)};
        }};
    }
};

}

namespace views {

NANO_INLINE_VAR(nano::detail::transform_view_fn, transform)

}

NANO_END_NAMESPACE

#endif


#endif


#endif // NANORANGE_HPP_INCLUDED
