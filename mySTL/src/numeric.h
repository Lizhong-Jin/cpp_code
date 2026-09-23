#pragma once

#ifndef NUMERIC_H
#define NUMERIC_H

#include "iterator.h"

namespace mystl {
    // *******************************************************************
    // accumulate
    template <typename Input_Iterator, typename T>
    constexpr T accumulate(Input_Iterator first, Input_Iterator last, T init) {
        while (first != last) {init += *first; ++first;}
        return init;
    }

    template <typename Input_Iterator, typename T, typename Binary_operator>
    constexpr T accumulate(Input_Iterator first, Input_Iterator last, T init, Binary_operator binary_op) {
        while (first != last) {init = binary_op(init, *first); ++first;}
        return init;
    }

    // *******************************************************************
    // reduce (not accomplished yet)

    // *******************************************************************
    // transform_reduce (not accomplished yet)

    // *******************************************************************
    // adjacent_difference
    template <typename Input_Iterator, typename Output_Iterator>
    constexpr Output_Iterator adjacent_difference(Input_Iterator first, Input_Iterator last, Output_Iterator result) {
        if (first == last) return result;
        typedef iter_value_type<Input_Iterator> value_t;
        value_t value = *first;
        *result = value;
        while (++first != last) {
            value_t temp = *first;
            *++result = temp - value;
            value = move(temp);
        }
        return ++result;
    }

    template <typename Input_Iterator, typename Output_Iterator, typename Binary_operator>
    constexpr Output_Iterator adjacent_difference(Input_Iterator first, Input_Iterator last,
                                                    Output_Iterator result, Binary_operator binary_op) {
        if (first == last) return result;
        typedef iter_value_type<Input_Iterator> value_t;
        value_t value = *first;
        *result = value;
        while (++first != last) {
            value_t temp = *first;
            *++result = binary_op(temp, value);
            value = move(temp);
        }
        return ++result;
    }

    // *******************************************************************
    // inner_product
    template <typename Input_Iterator1, typename Input_Iterator2, typename T>
    T inner_product(Input_Iterator1 first1, Input_Iterator1 last1, Input_Iterator2 first2, T init) {
        while (first1 != last1) {init += (*first1 * *first2); ++first1; ++first2;}
        return init;
    }

    template <typename Input_Iterator1, typename Input_Iterator2, typename T, typename Binary_operator1, typename Binary_operator2>
    T inner_product(Input_Iterator1 first1, Input_Iterator1 last1, Input_Iterator2 first2, T init,
                    Binary_operator1 binary_op1, Binary_operator2 binary_op2) {
        while (first1 != last1) {
            init = binary_op1(init, binary_op2(*first1, *first2));
            ++first1; ++first2;
        }
        return init;
    }

    // *******************************************************************
    // iota
    template <typename Forward_Iterator, typename T>
    void iota(Forward_Iterator first, Forward_Iterator last, T value) {
        while (first != last) {*first = value; ++first; ++value;}
    }

    // *******************************************************************
    // partial_sum
    template <typename Input_Iterator, typename Output_Iterator>
    Output_Iterator partial_sum(Input_Iterator first, Input_Iterator last, Output_Iterator result) {
        if (first == last) return result;
        typedef iter_value_type<Input_Iterator> value_t;
        value_t value = *first;
        *result = value;
        while (++first != last) {
            value += *first;
            *++result = value;
        }
        return ++result;
    }

    template <typename Input_Iterator, typename Output_Iterator, typename Binary_operator>
    Output_Iterator partial_sum(Input_Iterator first, Input_Iterator last, Output_Iterator result, Binary_operator binary_op) {
        if (first == last) return result;
        typedef iter_value_type<Input_Iterator> value_t;
        value_t value = *first;
        *result = value;
        while (++first != last) {
            value = binary_op(value, *first);
            *++result = value;
        }
        return ++result;
    }

    // *******************************************************************
    // inclusive_scan (not accomplished yet)

    // *******************************************************************
    // exclusive_scan (not accomplished yet)

    // *******************************************************************
    // transform_inclusive_scan (not accomplished yet)

    // *******************************************************************
    // transform_exclusive_scan (not accomplished yet)

    // *******************************************************************
    // gcd && lcm
    namespace detail {
        template <typename T>
        constexpr enable_if_t<is_integral_v<T> && is_signed_v<T>> _get_abs(T val) {
            return val < 0 ? -val : val;
        }
        template <typename T>
        constexpr enable_if_t<is_integral_v<T> && is_unsigned_v<T>> _get_abs(T val) {
            return val;
        }
        template <> void _get_abs(bool) = delete;

        template <typename M, typename N> constexpr common_type_t<M, N> get_gcd(M m, N n) {
            return m==0 ? _get_abs(n) : (n==0 ? _get_abs(m) : get_gcd(n, m % n));
        }

        template <typename M, typename N> constexpr common_type_t<M, N> get_lcm(M m, N n) {
            return (m==0 || n==0) ? 0 : ((m / get_gcd(m, n)) * n);
        }
    }

    template <typename M, typename N>
    constexpr common_type_t<M, N> gcd(M m, N n) {
        static_assert(is_integral_v<M> || is_integral_v<N>, "gcd argument must be an integral type");
        static_assert(!is_same_v<M, bool> || !is_same_v<N, bool>, "gcd argument must not be a boolean");
        return detail::get_gcd(m, n);
    }

    template <typename M, typename N>
    constexpr common_type_t<M, N> lcm(M m, N n) {
        static_assert(is_integral_v<M> || is_integral_v<N>, "lcm argument must be an integral type");
        static_assert(!is_same_v<M, bool> || !is_same_v<N, bool>, "lcm argument must not be a boolean");
        return detail::get_lcm(m, n);
    }

    // *******************************************************************
    // midpoint (not accomplished yet)

    // *******************************************************************
    // add_sat (not accomplished yet)

    // *******************************************************************
    // sub_sat (not accomplished yet)

    // *******************************************************************
    // mul_sat (not accomplished yet)

    // *******************************************************************
    // div_sat (not accomplished yet)

    // *******************************************************************
    // saturate_cast (not accomplished yet)


} // namespace mystl

#endif //NUMERIC_H
