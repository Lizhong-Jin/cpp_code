#pragma once

#ifndef POINTER_TRAITS_H
#define POINTER_TRAITS_H

#include "../type_traits.h"

namespace mystl {
    template <typename Ptr>
    struct pointer_traits;

    namespace detail {
        // detect element type
        template <typename, typename = void> struct has_element_type : false_type {};
        template <typename T> struct has_element_type<T, void_t<typename T::element_type>> : true_type {};

        template <typename, typename = void> struct has_value_type : false_type {};
        template <typename T> struct has_value_type<T, void_t<typename T::value_type>> : true_type {};

        template <typename Ptr, bool = has_element_type<Ptr>::value>
        struct element_type_detect {
            static_assert(has_value_type<Ptr>::value, "pointer_traits: neither element_type nor value_type present");
            using type = typename Ptr::value_type;
        };
        template <typename Ptr>
        struct element_type_detect<Ptr, true> {
            using type = typename Ptr::element_type;
        };

        // detect difference type
        template <typename, typename = void> struct has_difference_type : false_type {};
        template <typename T> struct has_difference_type<T, void_t<typename T::difference_type>> : true_type {};

        template <typename Ptr, bool = has_difference_type<Ptr>::value>
        struct difference_type_detect {using type = ptrdiff_t;};
        template <typename Ptr>
        struct difference_type_detect<Ptr, true> {
            using type = typename Ptr::difference_type;
        };

        // detect rebind
        template <typename, typename U, typename = void>
        struct has_rebind : false_type {};
        template <typename T, typename U>
        struct has_rebind<T, U, void_t<typename T::template rebind<U>>> : true_type {};

        template <typename Ptr, typename U, bool = has_rebind<Ptr, U>::value>
        struct rebind_impl {
            using type = typename pointer_traits<Ptr>::template rebind_default<U>;
        };
        template <typename Ptr, typename U>
        struct rebind_impl<Ptr, U, true> {
            using type = typename Ptr::template rebind<U>;
        };

        // pointer_to_impl
        template <typename Ptr, typename T>
        constexpr auto pointer_to_impl(T& x, int) -> decltype(Ptr::pointer_to(x)) {return Ptr::pointer_to(x);}

        template <typename Ptr, typename T>
        constexpr Ptr pointer_to_impl(T& x, long) {return Ptr(mystl::addressof(x));}

        // to_address_impl
        template <typename T>
        constexpr T* to_address_impl(T* ptr, int) noexcept {return ptr;}

        template <typename Ptr>
        constexpr auto to_address_impl(const Ptr& ptr, int) -> decltype(Ptr::to_address(ptr)) {
            return Ptr::to_address(ptr);
        }

        template <typename Ptr>
        constexpr auto to_address_impl(const Ptr& ptr, long) -> decltype(to_address_impl(ptr.operator->(), 0)) {
            return to_address_impl(ptr.operator->(), 0);
        }

    }

    template <typename T>
    constexpr T* to_address(T* ptr) noexcept {return ptr;}

    template <typename Ptr>
    constexpr auto to_address(const Ptr& ptr) noexcept -> decltype(detail::to_address_impl(ptr, 0)) {
        return detail::to_address_impl(ptr, 0);
    }

    template <typename Ptr>
    struct pointer_traits {
        using pointer = Ptr;
        using element_type = typename detail::element_type_detect<Ptr>::type;
        using difference_type = typename detail::difference_type_detect<Ptr>::type;
        template <typename U> using rebind = typename detail::rebind_impl<Ptr, U>::type;
        template <typename U> using rebind_default = U*;

        static constexpr pointer pointer_to(element_type& x) noexcept {
            return detail::pointer_to_impl<Ptr>(x, 0);
        }

        static constexpr element_type* to_address(pointer ptr) noexcept {
            return mystl::to_address(ptr);
        }
    };

    template <typename T>
    struct pointer_traits<T*> {
        using pointer = T*;
        using element_type = T;
        using difference_type = long;
        template <typename U> using rebind = U*;
        template <typename U> using rebind_default = U*;

        static constexpr pointer pointer_to(element_type& x) noexcept {return &x;}

        static constexpr element_type* to_address(pointer ptr) noexcept {return ptr;}
    };
} // namespace mystl


#endif //POINTER_TRAITS_H
