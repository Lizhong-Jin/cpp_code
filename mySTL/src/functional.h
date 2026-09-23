#pragma once

#ifndef FUNCTIONAL_H
#define FUNCTIONAL_H

#include "impl/functions.h"
#include "impl/wrappers.h"
#include "type_traits.h"

namespace mystl {

    // *************************************************************************************
    // Arithmetic operations
    // plus
    template <typename = void> struct plus;

    template <typename T> struct plus {
        constexpr T operator()(const T x, const T y) const { return x + y; }
    };

    template <> struct plus<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) + forward<U>(y))) -> decltype(forward<T>(x) + forward<U>(y))
        {return forward<T>(x) + forward<U>(y);}
        using is_transparent = void;
    };

    // minus
    template <typename = void> struct minus;

    template <typename T> struct minus {
        constexpr T operator()(const T x, const T y) const { return x - y; }
    };

    template <> struct minus<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) - forward<U>(y))) -> decltype(forward<T>(x) - forward<U>(y))
        {return forward<T>(x) - forward<U>(y);}
        using is_transparent = void;
    };

    // multiplies
    template <typename = void> struct multiplies;

    template <typename T> struct multiplies {
        constexpr T operator()(const T x, const T y) const { return x * y; }
    };

    template <> struct multiplies<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) * forward<U>(y))) -> decltype(forward<T>(x) * forward<U>(y))
        {return forward<T>(x) * forward<U>(y);}
        using is_transparent = void;
    };

    // divides
    template <typename = void> struct divides;

    template <typename T> struct divides {
        constexpr T operator()(const T x, const T y) const { return x / y; }
    };

    template <> struct divides<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) / forward<U>(y))) -> decltype(forward<T>(x) / forward<U>(y))
        {return forward<T>(x) / forward<U>(y);}
        using is_transparent = void;
    };

    // modulus
    template <typename = void> struct modulus;

    template <typename T> struct modulus {
        constexpr T operator()(const T x, const T y) const { return x % y; }
    };

    template <> struct modulus<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) % forward<U>(y))) -> decltype(forward<T>(x) % forward<U>(y))
        {return forward<T>(x) % forward<U>(y);}
        using is_transparent = void;
    };

    // negate
    template <typename = void> struct negate;

    template <typename T> struct negate {
        constexpr T operator()(const T& x) const { return -x; }
    };

    template <> struct negate<void> {
        template <typename T>
        constexpr auto operator()(T&& x) const
        noexcept(noexcept(-forward<T>(x))) -> decltype(-forward<T>(x))
        {return -forward<T>(x);}
        using is_transparent = void;
    };

    // *************************************************************************************
    // Comparisons
    // equal_to
    template <typename = void> struct equal_to;

    template <typename T> struct equal_to {
        constexpr bool operator()(const T& x, const T& y) const { return x == y; }
    };

    template <> struct equal_to<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) == forward<U>(y))) -> decltype(forward<T>(x) == forward<U>(y))
        {return forward<T>(x) == forward<U>(y);}
        using is_transparent = void;
    };

    // not_equal_to
    template <typename = void> struct not_equal_to;

    template <typename T> struct not_equal_to {
        constexpr bool operator()(const T& x, const T& y) const { return x != y; }
    };

    template <> struct not_equal_to<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) != forward<U>(y))) -> decltype(forward<T>(x) != forward<U>(y))
        {return forward<T>(x) != forward<U>(y);}
        using is_transparent = void;
    };

    // greater
    template <typename = void> struct greater;

    template <typename T> struct greater {
        constexpr bool operator()(const T& x, const T& y) const { return x > y; }
    };

    template <> struct greater<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) > forward<U>(y))) -> decltype(forward<T>(x) > forward<U>(y))
        {return forward<T>(x) > forward<U>(y);}
        using is_transparent = void;
    };

    // less
    template <typename = void> struct less;

    template <typename T> struct less {
        constexpr bool operator()(const T& x, const T& y) const { return x < y; }
    };

    template <> struct less<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) < forward<U>(y))) -> decltype(forward<T>(x) < forward<U>(y))
        {return forward<T>(x) < forward<U>(y);}
        using is_transparent = void;
    };

    // greater_equal
    template <typename = void> struct greater_equal;

    template <typename T> struct greater_equal {
        constexpr bool operator()(const T& x, const T& y) const { return x >= y; }
    };

    template <> struct greater_equal<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) >= forward<U>(y))) -> decltype(forward<T>(x) >= forward<U>(y))
        {return forward<T>(x) >= forward<U>(y);}
        using is_transparent = void;
    };

    // less_equal
    template <typename = void> struct less_equal;

    template <typename T> struct less_equal {
        constexpr bool operator()(const T& x, const T& y) const { return x <= y; }
    };

    template <> struct less_equal<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) <= forward<U>(y))) -> decltype(forward<T>(x) <= forward<U>(y))
        {return forward<T>(x) <= forward<U>(y);}
        using is_transparent = void;
    };

    // *************************************************************************************
    // Logical operations
    // logical_add
    template <typename = void> struct logical_and;

    template <typename T> struct logical_and {
        constexpr bool operator()(const T& x, const T& y) const { return x && y; }
    };

    template <> struct logical_and<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) && forward<U>(y))) -> decltype(forward<T>(x) && forward<U>(y))
        {return forward<T>(x) && forward<U>(y);}
        using is_transparent = void;
    };

    // logical_or
    template <typename = void> struct logical_or;

    template <typename T> struct logical_or {
        constexpr bool operator()(const T& x, const T& y) const { return x || y; }
    };

    template <> struct logical_or<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) || forward<U>(y))) -> decltype(forward<T>(x) || forward<U>(y))
        {return forward<T>(x) || forward<U>(y);}
        using is_transparent = void;
    };

    // logical_not
    template <typename = void> struct logical_not;

    template <typename T> struct logical_not {
        constexpr bool operator()(const T& x) const { return !x; }
    };

    template <> struct logical_not<void> {
        template <typename T>
        constexpr auto operator()(T&& x) const
        noexcept(noexcept(!forward<T>(x))) -> decltype(!forward<T>(x))
        {return !forward<T>(x);}
        using is_transparent = void;
    };

    // *************************************************************************************
    // Bitwise operations
    // bit_and
    template <typename = void> struct bit_and;

    template <typename T> struct bit_and {
        constexpr bool operator()(const T& x, const T& y) const { return x & y; }
    };

    template <> struct bit_and<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) & forward<U>(y))) -> decltype(forward<T>(x) & forward<U>(y))
        {return forward<T>(x) & forward<U>(y);}
        using is_transparent = void;
    };

    // bit_or
    template <typename = void> struct bit_or;

    template <typename T> struct bit_or {
        constexpr bool operator()(const T& x, const T& y) const { return x | y; }
    };

    template <> struct bit_or<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) | forward<U>(y))) -> decltype(forward<T>(x) | forward<U>(y))
        {return forward<T>(x) | forward<U>(y);}
        using is_transparent = void;
    };

    // bit_xor
    template <typename = void> struct bit_xor;

    template <typename T> struct bit_xor {
        constexpr bool operator()(const T& x, const T& y) const { return x ^ y; }
    };

    template <> struct bit_xor<void> {
        template <typename T, typename U>
        constexpr auto operator()(T&& x, U&& y) const
        noexcept(noexcept(forward<T>(x) ^ forward<U>(y))) -> decltype(forward<T>(x) ^ forward<U>(y))
        {return forward<T>(x) ^ forward<U>(y);}
        using is_transparent = void;
    };

    // bit_not
    template <typename = void> struct bit_not;

    template <typename T> struct bit_not {
        constexpr bool operator()(const T& x) const { return ~x; }
    };

    template <> struct bit_not<void> {
        template <typename T>
        constexpr auto operator()(T&& x) const
        noexcept(noexcept(~forward<T>(x))) -> decltype(~forward<T>(x))
        {return ~forward<T>(x);}
        using is_transparent = void;
    };

} // namespace mystl

#endif //FUNCTIONAL_H
