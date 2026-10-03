#pragma once

#ifndef UTILITY_H
#define UTILITY_H

#include "impl/type_traits_normal.h"

namespace mystl {
    // **************************************************************************
    // move
    template <typename T>
    constexpr remove_reference_t<T>&& move(T&& arg) noexcept {
        return static_cast<remove_reference_t<T>&&>(arg);
    }

    // forward
    template <typename T>
    constexpr T&& forward(remove_reference_t<T>& arg) noexcept {
        return static_cast<T&&>(arg);
    }
    template <typename T>
    constexpr T&& forward(remove_reference_t<T>&& arg) noexcept {
        static_assert(!is_lvalue_reference_v<T>, "bad forward");
        return static_cast<T&&>(arg);
    }

    // swap
    template <typename T>
        requires is_move_constructible_v<T> && is_move_assignable_v<T>
    constexpr void swap(T& lhs, T& rhs) 
        noexcept(is_nothrow_move_constructible_v<T> && is_nothrow_move_assignable_v<T>) 
    {
        T temp(mystl::move(lhs));
        lhs = mystl::move(rhs);
        rhs = mystl::move(temp);
    };
    template <class T, size_t N>
    void swap(T(&lhs)[N], T(&rhs)[N]) noexcept(noexcept(mystl::swap(*lhs, *rhs))) {
        for (int i=0; i<N; i++) {
            mystl::swap(lhs[i], rhs[i]);
        }
    }

    // **************************************************************************
    // pair
    template <typename T1, typename T2>
    struct pair {
        typedef T1 first_type;
        typedef T2 second_type;
        first_type first;
        second_type second;

        // default constructible
        template <typename O1 = T1, typename O2 = T2, typename = enable_if_t<
            is_default_constructible_v<O1> && is_default_constructible_v<O2>, void>>
        constexpr pair()
        noexcept(is_nothrow_default_constructible_v<O1> && is_nothrow_default_constructible_v<O2>)
        : first(), second() {}

        // implicit constructible
        template <typename U1 = T1, typename U2 = T2, enable_if_t<
            is_copy_constructible_v<U1> && is_copy_constructible_v<U2>
            && is_convertible_v<const T1&, U1> && is_convertible_v<const T2&, U2>, int> = 0>
        constexpr pair(const T1& a, const T2& b): first(a), second(b) {}

        // explicit constructible
        template <typename U1 = T1, typename U2 = T2, enable_if_t<
            is_copy_constructible_v<U1> && is_copy_constructible_v<U2>
            && (!is_convertible_v<const U1&, T1> || !is_convertible_v<const U2&, T2>), int> = 0>
        constexpr pair(const T1& a, const T2& b): first(a), second(b) {}

        pair(const pair& p) = default;
        pair(pair&& p) = default;

        // implicit constructible for other type
        template <typename Other1, typename Other2, enable_if_t<
            is_constructible_v<T1, Other1&&> && is_constructible_v<T2, Other2&&>
            && is_convertible_v<Other1&&, T1> && is_convertible_v<Other2&&, T2>, int> = 0>
        constexpr pair(Other1&& a, Other2&& b): first(forward<Other1>(a)), second(forward<Other2>(b)) {}

        // explicit constructible for other type
        template <typename Other1, typename Other2, enable_if_t<
            is_constructible_v<T1, Other1&&> && is_constructible_v<T2, Other2&&>
            && (!is_convertible_v<Other1&&, T1> || !is_convertible_v<Other2&&, T2>), int> = 0>
        explicit constexpr pair(Other1&& a, Other2&& b): first(forward<Other1>(a)), second(forward<Other2>(b)) {}

        // implicit constructible for other l_value pair
        template <typename Other1, typename Other2, enable_if_t<
            is_constructible_v<T1, const Other1&> && is_constructible_v<T2, const Other2&>
            && is_convertible_v<const Other1&, T1> && is_convertible_v<const Other2&, T2>, int> = 0>
        constexpr pair(const pair<Other1, Other2>& p): first(p.first), second(p.second) {}

        // explicit constructible for other l_value pair
        template <typename Other1, typename Other2, enable_if_t<
            is_constructible_v<T1, const Other1&> && is_constructible_v<T2, const Other2&>
            && (!is_convertible_v<const Other1&, T1> || !is_convertible_v<const Other2&, T2>), int> = 0>
        explicit constexpr pair(const pair<Other1, Other2>& p): first(p.first), second(p.second) {}

        // implicit constructible for other r_value pair
        template <typename Other1, typename Other2, enable_if_t<
            is_constructible_v<T1, Other1> && is_constructible_v<T2, Other2>
            && is_convertible_v<Other1, T1> && is_convertible_v<Other2, T2>, int> = 0>
        constexpr pair(pair<Other1, Other2>&& p): first(forward<Other1>(p.first)), second(forward<Other2>(p.second)) {}

        // explicit constructible for other r_value pair
        template <typename Other1, typename Other2, enable_if_t<
            is_constructible_v<T1, Other1> && is_constructible_v<T2, Other2>
            && (!is_convertible_v<Other1, T1> || !is_convertible_v<Other2, T2>), int> = 0>
        explicit constexpr pair(pair<Other1, Other2>&& p): first(forward<Other1>(p.first)), second(forward<Other2>(p.second)) {}

        // copy assign for this pair
        pair& operator=(const pair& rhs) {
            if (this != &rhs) {
                first = rhs.first;
                second = rhs.second;
            }
            return *this;
        }
        // move assign for this pair
        pair& operator=(pair&& rhs) noexcept(noexcept(first = move(rhs.first)) && noexcept(second = move(rhs.second))) {
            if (this != &rhs) {
                first = move(rhs.first);
                second = move(rhs.second);
            }
            return *this;
        }

        // copy assign for other pair
        template <typename Other1, typename Other2>
        pair& operator=(const pair<Other1, Other2>& p) {
            first = p.first;
            second = p.second;
            return *this;
        }
        // move assign for other pair
        template <typename Other1, typename Other2>
        pair& operator=(pair<Other1, Other2>&& p) {
            first = forward<Other1>(p.first);
            second = forward<Other2>(p.second);
            return *this;
        }

        // destructible
        ~pair() = default;

        void swap(pair& rhs) noexcept(noexcept(mystl::swap(first, rhs.first)) && noexcept(mystl::swap(second, rhs.second))) {
            if (this != &rhs) {
                mystl::swap(first, rhs.first);
                mystl::swap(second, rhs.second);
            }
        }
    };

    // operator overloading for pair
    // equal ==
    template <typename T1, typename T2>
    bool operator==(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }
    // not equal !=
    template <typename T1, typename T2>
    bool operator!=(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
        return lhs.first != rhs.first || lhs.second != rhs.second;
    }
    // greater than >
    template <typename T1, typename T2>
    bool operator>(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
        return lhs.first > rhs.first || (lhs.first == rhs.first && lhs.second > rhs.second);
    }
    // less than <
    template <typename T1, typename T2>
    bool operator<(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
        return lhs.first < rhs.first || (lhs.first == rhs.first && lhs.second < rhs.second);
    }
    // not less than >=
    template <typename T1, typename T2>
    bool operator>=(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
        return !(lhs < rhs);
    }
    // not greater than <=
    template <typename T1, typename T2>
    bool operator<=(const pair<T1, T2>& lhs, const pair<T1, T2>& rhs) {
        return !(lhs > rhs);
    }

    // swap for pair
    template <typename T1, typename T2>
    void swap(pair<T1, T2>& lhs, pair<T1, T2>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    // make_pair
    template <typename T1, typename T2>
    pair<T1, T2> make_pair(T1&& first, T2&& second) {
        return mystl::pair<T1, T2>(mystl::forward<T1>(first), mystl::forward<T2>(second));
    }

} // namespace mystl

#endif //UTILITY_H
