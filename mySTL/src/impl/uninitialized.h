#pragma once

#ifndef UNINITIALIZED_H
#define UNINITIALIZED_H

#include "construct.h"
#include "../iterator.h"
#include "../utility.h"
#include "../cstring.h"
#include "allocator.h"
#include "algobase.h"
#include "pointer_traits.h"

namespace mystl {

    namespace detail {
        template <typename Input_Iterator, typename Forward_Iterator>
        struct use_trivial_copy {
        private:
            using in_ptr = iter_pointer<Input_Iterator>;
            using out_ptr = iter_pointer<Forward_Iterator>;
            using in_value_type = iter_value_type<Input_Iterator>;
            using out_value_type = iter_value_type<Forward_Iterator>;

            // The byte-copy path returns a raw pointer and must not erase volatile access.
            static constexpr bool _is_contiguous = is_pointer_v<Input_Iterator> && is_pointer_v<Forward_Iterator>
                && !is_volatile_v<remove_pointer_t<Input_Iterator>>
                && !is_volatile_v<remove_pointer_t<Forward_Iterator>>;
            static constexpr bool _value_match = is_same_v<remove_cv_t<in_value_type>, out_value_type>;
            static constexpr bool _trivially_copy = is_trivially_copy_constructible_v<out_value_type>
                && is_trivially_copyable_v<out_value_type>;
        public:
            static constexpr bool value = _is_contiguous && _value_match && _trivially_copy;
            using type = bool_constant<value>;
        };

        template <typename Input_Iterator, typename Forward_Iterator>
        struct use_trivial_move {
        private:
            using in_ptr = iter_pointer<Input_Iterator>;
            using out_ptr = iter_pointer<Forward_Iterator>;
            using in_value_type = iter_value_type<Input_Iterator>;
            using out_value_type = iter_value_type<Forward_Iterator>;

            // The byte-copy path returns a raw pointer and must not erase volatile access.
            static constexpr bool _is_contiguous = is_pointer_v<Input_Iterator> && is_pointer_v<Forward_Iterator>
                && !is_volatile_v<remove_pointer_t<Input_Iterator>>
                && !is_volatile_v<remove_pointer_t<Forward_Iterator>>;
            static constexpr bool _value_match = is_same_v<remove_cv_t<in_value_type>, out_value_type>;
            static constexpr bool _trivially_move = is_trivially_move_constructible_v<out_value_type>
                && is_trivially_copyable_v<out_value_type>;
        public:
            static constexpr bool value = _is_contiguous && _value_match && _trivially_move;
            using type = bool_constant<value>;
        };
    }

    // uninitialized_copy implement for generic situation
    template <typename Input_Iterator, typename Forward_Iterator>
    constexpr Forward_Iterator _uninitialized_copy_generic_impl(Input_Iterator first, Input_Iterator last,
                                                Forward_Iterator result, input_iterator_tag) {
        auto current = result;
        try {
            for (; first != last; ++first, ++current) {
                mystl::construct(mystl::addressof(*current), *first);
            }
        }catch (...) {
            mystl::destroy(result, current);
            throw;
        }
        return current;
    }

    template <typename Random_Iterator, typename Forward_Iterator>
    constexpr Forward_Iterator _uninitialized_copy_generic_impl(Random_Iterator first, Random_Iterator last,
                                                Forward_Iterator result, random_access_iterator_tag) {
        using diff_type = iter_difference_type<Random_Iterator>;
        diff_type n = last - first;
        auto current = result;
        try {
            for (; n > 0; --n, ++first, ++current) {
                mystl::construct(mystl::addressof(*current), *first);
            }
        }catch (...) {
            mystl::destroy(result, current);
            throw;
        }
        return current;
    }

    template <typename Input_Iterator, typename Forward_Iterator>
    constexpr Forward_Iterator _uninitialized_copy_generic(Input_Iterator first, Input_Iterator last,
                                                Forward_Iterator result) {
        return _uninitialized_copy_generic_impl(first, last, result, iter_category<Input_Iterator>{});
    }

    // uninitialized_copy_n implement for trivially copy type and contiguous iterator, using memory copy
    template <typename T, typename Size>
    constexpr T* _uninitialized_copy_n_trivial(const T* first, Size n, T* result) {
        using value_type = T;
        if (n > 0) {
            mystl::memcpy(static_cast<void*>(result), static_cast<const void*>(first), sizeof(value_type) * n);
        }
        return n > 0 ? result + n : result;
    }

    // uninitialized_copy_a implement for generic situation
    template<typename Input_Iterator, typename Forward_Iterator, typename Allocator>
    constexpr Forward_Iterator _uninitialized_copy_a_generic_impl(Input_Iterator first, Input_Iterator last,
                                    Forward_Iterator result, Allocator& alloc, input_iterator_tag) {
        using traits = allocator_traits<Allocator>;
        auto current = result;
        try {
            for (; first != last; ++first, ++current) {
                traits::construct(alloc, mystl::addressof(*current), *first);
            }
        } catch (...) {
            for (; result != current; ++result) {
                traits::destroy(alloc, mystl::addressof(*result));
            }
            throw;
        }
        return current;
    }

    template<typename Random_Iterator, typename Forward_Iterator, typename Allocator>
    constexpr Forward_Iterator _uninitialized_copy_a_generic_impl(Random_Iterator first, Random_Iterator last,
                                    Forward_Iterator result, Allocator& alloc, random_access_iterator_tag) {
        using traits = allocator_traits<Allocator>;
        using diff_type = iter_difference_type<Random_Iterator>;
        diff_type n = last - first;
        auto current = result;
        try {
            for (; n > 0; --n, ++first, ++current) {
                traits::construct(alloc, mystl::addressof(*current), *first);
            }
        } catch (...) {
            for (; result != current; ++result) {
                traits::destroy(alloc, mystl::addressof(*result));
            }
            throw;
        }
        return current;
    }

    template<typename Input_Iterator, typename Forward_Iterator, typename Allocator>
    constexpr Forward_Iterator _uninitialized_copy_a_generic(Input_Iterator first, Input_Iterator last,
                                    Forward_Iterator result, Allocator& alloc) {
        return _uninitialized_copy_a_generic_impl(first, last, result, alloc, iter_category<Input_Iterator>{});
    }

    // uninitialized_copy_n implement for generic situation
    template<typename Input_Iterator, typename Size, typename Forward_Iterator>
    constexpr Forward_Iterator _uninitialized_copy_n_generic(Input_Iterator first, Size n, Forward_Iterator result) {
        auto current = result;
        try {
            for (; n > 0; --n, ++first, ++current) {
                mystl::construct(mystl::addressof(*current), *first);
            }
        }catch (...) {
            mystl::destroy(result, current);
            throw;
        }
        return current;
    }

    // uninitialized_copy_n_a implement for generic situation
    template<typename Input_Iterator, typename Size, typename Forward_Iterator, typename Allocator>
    constexpr Forward_Iterator _uninitialized_copy_n_a_generic(Input_Iterator first, Size n,
                                    Forward_Iterator result, Allocator& alloc) {
        using traits = allocator_traits<Allocator>;
        auto current = result;
        try {
            for (; n > 0; --n, ++first, ++current) {
                traits::construct(alloc, mystl::addressof(*current), *first);
            }
        }catch (...) {
            for (; result != current; ++result) {
                traits::destroy(alloc, mystl::addressof(*result));
            }
            throw;
        }
        return current;
    }

    // uninitialized_move implement for generic situation
    template <typename Input_Iterator, typename Forward_Iterator>
    constexpr Forward_Iterator _uninitialized_move_generic_impl(Input_Iterator first, Input_Iterator last,
                                                Forward_Iterator result, input_iterator_tag) {
        auto current = result;
        try {
            for (; first != last; ++first, ++current) {
                mystl::construct(mystl::addressof(*current), mystl::move(*first));
            }
        }catch (...) {
            mystl::destroy(result, current);
            throw;
        }
        return current;
    }

    template <typename Random_Iterator, typename Forward_Iterator>
    constexpr Forward_Iterator _uninitialized_move_generic_impl(Random_Iterator first, Random_Iterator last,
                                                Forward_Iterator result, random_access_iterator_tag) {
        using diff_type = iter_difference_type<Random_Iterator>;
        diff_type n = last - first;
        auto current = result;
        try {
            for (; n > 0; --n, ++first, ++current) {
                mystl::construct(mystl::addressof(*current), mystl::move(*first));
            }
        }catch (...) {
            mystl::destroy(result, current);
            throw;
        }
        return current;
    }

    template <typename Input_Iterator, typename Forward_Iterator>
    constexpr Forward_Iterator _uninitialized_move_generic(Input_Iterator first, Input_Iterator last,
                                                Forward_Iterator result) {
        return _uninitialized_move_generic_impl(first, last, result, iter_category<Input_Iterator>{});
    }

    // uninitialized_move_n implement for trivially move type and contiguous iterator, using memory move
    template <typename T, typename Size>
    constexpr T* _uninitialized_move_n_trivial(const T* first, Size n, T* result) {
        using value_type = T;
        if (n > 0) {
            mystl::memmove(static_cast<void*>(result), static_cast<const void*>(first), sizeof(value_type) * n);
        }
        return n > 0 ? result + n : result;
    }

    // uninitialized_move_a implement for generic situation
    template<typename Input_Iterator, typename Forward_Iterator, typename Allocator>
    constexpr Forward_Iterator _uninitialized_move_a_generic_impl(Input_Iterator first, Input_Iterator last,
                                    Forward_Iterator result, Allocator& alloc, input_iterator_tag) {
        using traits = allocator_traits<Allocator>;
        auto current = result;
        try {
            for (; first != last; ++first, ++current) {
                traits::construct(alloc, mystl::addressof(*current), mystl::move(*first));
            }
        } catch (...) {
            for (; result != current; ++result) {
                traits::destroy(alloc, mystl::addressof(*result));
            }
            throw;
        }
        return current;
    }

    template<typename Random_Iterator, typename Forward_Iterator, typename Allocator>
    constexpr Forward_Iterator _uninitialized_move_a_generic_impl(Random_Iterator first, Random_Iterator last,
                                    Forward_Iterator result, Allocator& alloc, random_access_iterator_tag) {
        using traits = allocator_traits<Allocator>;
        using diff_type = iter_difference_type<Random_Iterator>;
        diff_type n = last - first;
        auto current = result;
        try {
            for (; n > 0; --n, ++first, ++current) {
                traits::construct(alloc, mystl::addressof(*current), mystl::move(*first));
            }
        } catch (...) {
            for (; result != current; ++result) {
                traits::destroy(alloc, mystl::addressof(*result));
            }
            throw;
        }
        return current;
    }

    template<typename Input_Iterator, typename Forward_Iterator, typename Allocator>
    constexpr Forward_Iterator _uninitialized_move_a_generic(Input_Iterator first, Input_Iterator last,
                                    Forward_Iterator result, Allocator& alloc) {
        return _uninitialized_move_a_generic_impl(first, last, result, alloc, iter_category<Input_Iterator>{});
    }

    // uninitialized_move_n implement for generic situation
    template<typename Input_Iterator, typename Size, typename Forward_Iterator>
    constexpr Forward_Iterator _uninitialized_move_n_generic(Input_Iterator first, Size n, Forward_Iterator result) {
        auto current = result;
        try {
            for (; n > 0; --n, ++first, ++current) {
                mystl::construct(mystl::addressof(*current), mystl::move(*first));
            }
        }catch (...) {
            mystl::destroy(result, current);
            throw;
        }
        return current;
    }

    // uninitialized_move_n_a implement for generic situation
    template<typename Input_Iterator, typename Size, typename Forward_Iterator, typename Allocator>
    constexpr Forward_Iterator _uninitialized_move_n_a_generic(Input_Iterator first, Size n,
                                    Forward_Iterator result, Allocator& alloc) {
        using traits = allocator_traits<Allocator>;
        auto current = result;
        try {
            for (; n > 0; --n, ++first, ++current) {
                traits::construct(alloc, mystl::addressof(*current), mystl::move(*first));
            }
        }catch (...) {
            for (; result != current; ++result) {
                traits::destroy(alloc, mystl::addressof(*result));
            }
            throw;
        }
        return current;
    }

    // uninitialized_fill_n implement for trivially copy type and contiguous iterator, using memory copy
    template <typename Contiguous_Iterator, typename Size, typename T>
    constexpr Contiguous_Iterator _uninitialized_fill_n_trivial(Contiguous_Iterator first, Size n, const T& value) {
        for (; n > 0; --n, ++first) {*first = value;}
        return first + n;
    }

    // specialization for 1-bite type
    template <typename Contiguous_Iterator, typename Size, typename T,
                    typename U = iter_value_type<Contiguous_Iterator>>
    constexpr enable_if_t<is_integral_v<U> && sizeof(U) == 1 && !is_same_v<U, bool>
                && is_integral_v<T> && sizeof(T) == 1, Contiguous_Iterator>
    _uninitialized_fill_n_trivial(Contiguous_Iterator first, Size n, const T& value) {
        if (n > 0) mystl::memset(static_cast<void*>(mystl::addressof(*first)), static_cast<unsigned char>(value), n);
        return first + n;
    }

    // uninitialized_fill_n implement for generic situation
    template <typename Forward_Iterator, typename Size, typename T>
    constexpr Forward_Iterator _uninitialized_fill_n_generic(Forward_Iterator first, Size n, const T& value) {
        auto current = first;
        try {
            for (; n > 0; --n, ++current) {
                mystl::construct(mystl::addressof(*current), value);
            }
        }catch (...) {
            mystl::destroy(first, current);
            throw;
        }
        return current;
    }

    // uninitialized_fill implement for generic situation
    template <typename Forward_Iterator, typename T>
    constexpr void _uninitialized_fill_generic_impl(Forward_Iterator first, Forward_Iterator last, const T& value) {
        auto current = first;
        try {
            for (; current != last; ++current) {
                mystl::construct(mystl::addressof(*current), value);
            }
        }catch (...) {
            mystl::destroy(first, current);
            throw;
        }
    }

    template <typename Forward_Iterator, typename T>
    constexpr void _uninitialized_fill_generic(Forward_Iterator first, Forward_Iterator last, const T& value) {
        if constexpr (is_random_access_iterator_v<Forward_Iterator>) {
            _uninitialized_fill_n_generic(first, last - first, value);
        }else {
            _uninitialized_fill_generic_impl(first, last, value);
        }
    }

    // uninitialized_fill_n_a implement for generic situation
    template <typename Forward_Iterator, typename Size, typename T, typename Allocator>
    constexpr Forward_Iterator _uninitialized_fill_n_a_generic(Forward_Iterator first, Size n, const T& value,
                                                Allocator& alloc) {
        using traits = allocator_traits<Allocator>;
        auto current = first;
        try {
            for (; n > 0; --n, ++current) {
                traits::construct(alloc, mystl::addressof(*current), value);
            }
        }catch (...) {
            for (; first != current; ++first) {
                traits::destroy(alloc, mystl::addressof(*first));
            }
            throw;
        }
        return current;
    }

    // uninitialized_fill_a implement for generic situation
    template <typename Forward_Iterator, typename T, typename Allocator>
    constexpr void _uninitialized_fill_a_generic_impl(Forward_Iterator first, Forward_Iterator last, const T& value,
                                                    Allocator& alloc) {
        using traits = allocator_traits<Allocator>;
        auto current = first;
        try {
            for (; current != last; ++current) {
                traits::construct(alloc, mystl::addressof(*current), value);
            }
        }catch (...) {
            for (; first != current; ++first) {
                traits::destroy(alloc, mystl::addressof(*first));
            }
            throw;
        }
    }

    template <typename Forward_Iterator, typename T, typename Allocator>
    constexpr void _uninitialized_fill_a_generic(Forward_Iterator first, Forward_Iterator last, const T& value,
                                                    Allocator& alloc) {
        if constexpr (is_random_access_iterator_v<Forward_Iterator>) {
            _uninitialized_fill_n_a_generic(first, last - first, value, alloc);
        }else {
            _uninitialized_fill_a_generic_impl(first, last, value, alloc);
        }
    }

    // *************************************************************************************
    // uninitialized_copy
    template<typename Input_Iterator, typename Forward_Iterator>
    constexpr Forward_Iterator uninitialized_copy(Input_Iterator first, Input_Iterator last, Forward_Iterator result) {
        if (first == last) return result;
        static constexpr bool use_trivial = detail::use_trivial_copy<Input_Iterator, Forward_Iterator>::value;
        if constexpr (use_trivial) {
            using T = iter_value_type<Forward_Iterator>;
            const T *src_first = mystl::to_address(first);
            const T *src_last = mystl::to_address(last);
            T *dest = mystl::to_address(result);
            return _uninitialized_copy_n_trivial(src_first, src_last - src_first, dest);
        } else {
            return _uninitialized_copy_generic(first, last, result);
        }
    }

    // *************************************************************************************
    // uninitialized_copy_a
    template<typename Input_Iterator, typename Forward_Iterator, typename Allocator>
    constexpr Forward_Iterator uninitialized_copy_a(Input_Iterator first, Input_Iterator last, Forward_Iterator result,
                                                    Allocator& alloc) {
        if (first == last) return result;
        return _uninitialized_copy_a_generic(first, last, result, alloc);
    }

    // *************************************************************************************
    // uninitialized_copy_n
    template <typename Input_Iterator, typename Size, typename Forward_Iterator>
    constexpr Forward_Iterator uninitialized_copy_n(Input_Iterator first, Size n, Forward_Iterator result) {
        static constexpr bool use_trivial = detail::use_trivial_copy<Input_Iterator, Forward_Iterator>::value;
        if constexpr (use_trivial) {
            using T = iter_value_type<Forward_Iterator>;
            const T *src_first = mystl::to_address(first);
            T *dest = mystl::to_address(result);
            return _uninitialized_copy_n_trivial(src_first, n, dest);
        }else {
            return _uninitialized_copy_n_generic(first, n, result);
        }
    }

    // *************************************************************************************
    // uninitialized_copy_n_a
    template <typename Input_Iterator, typename Size, typename Forward_Iterator, typename Allocator>
    constexpr Forward_Iterator uninitialized_copy_n_a(Input_Iterator first, Size n, Forward_Iterator result,
                                                Allocator& alloc) {
        return _uninitialized_copy_n_a_generic(first, n, result, alloc);
    }

    // *************************************************************************************
    // uninitialized_fill
    template <typename Forward_Iterator, typename T>
    constexpr void uninitialized_fill(Forward_Iterator first, Forward_Iterator last, const T& value) {
        if (first == last) return;
        _uninitialized_fill_generic(first, last, value);
    }

    // *************************************************************************************
    // uninitialized_fill_a
    template <typename Forward_Iterator, typename T, typename Allocator>
    constexpr void uninitialized_fill_a(Forward_Iterator first, Forward_Iterator last, const T& value, Allocator& alloc) {
        if (first == last) return;
        _uninitialized_fill_a_generic(first, last, value, alloc);
    }

    // *************************************************************************************
    // uninitialized_fill_n
    template <typename Forward_Iterator, typename Size, typename T>
    constexpr Forward_Iterator uninitialized_fill_n(Forward_Iterator first, Size n, const T& value) {
        return _uninitialized_fill_n_generic(first, n, value);
    }

    // *************************************************************************************
    // uninitialized_fill_n_a
    template <typename Forward_Iterator, typename Size, typename T, typename Allocator>
    constexpr Forward_Iterator uninitialized_fill_n_a(Forward_Iterator first, Size n, const T& value, Allocator& alloc) {
        return _uninitialized_fill_n_a_generic(first, n, value, alloc);
    }

    // *************************************************************************************
    // uninitialized_move
    template <typename Input_Iterator, typename Forward_Iterator>
    constexpr Forward_Iterator uninitialized_move(Input_Iterator first, Input_Iterator last, Forward_Iterator result) {
        if (first == last) return result;
        static constexpr bool use_trivial = detail::use_trivial_move<Input_Iterator, Forward_Iterator>::value;
        if constexpr (use_trivial) {
            using T = iter_value_type<Forward_Iterator>;
            const T *src_first = mystl::to_address(first);
            const T *src_last = mystl::to_address(last);
            T *dest = mystl::to_address(result);
            return _uninitialized_move_n_trivial(src_first, src_last - src_first, dest);
        }else {
            return _uninitialized_move_generic(first, last, result);
        }
    }

    // *************************************************************************************
    // uninitialized_move_a
    template <typename Input_Iterator, typename Forward_Iterator, typename Allocator>
    constexpr Forward_Iterator uninitialized_move_a(Input_Iterator first, Input_Iterator last, Forward_Iterator result,
                                                Allocator& alloc) {
        if (first == last) return result;
        return _uninitialized_move_a_generic(first, last, result, alloc);
    }

    // *************************************************************************************
    // uninitialized_move_n
    template <typename Input_Iterator, typename Size, typename Forward_Iterator>
    constexpr Forward_Iterator uninitialized_move_n(Input_Iterator first, Size n, Forward_Iterator result) {
        static constexpr bool use_trivial = detail::use_trivial_move<Input_Iterator, Forward_Iterator>::value;
        if constexpr (use_trivial) {
            using T = iter_value_type<Forward_Iterator>;
            const T *src_first = mystl::to_address(first);
            T *dest = mystl::to_address(result);
            return _uninitialized_move_n_trivial(src_first, n, dest);
        }else {
            return _uninitialized_move_n_generic(first, n, result);
        }
    }

    // *************************************************************************************
    // uninitialized_move_n_a
    template <typename Input_Iterator, typename Size, typename Forward_Iterator, typename Allocator>
    constexpr Forward_Iterator uninitialized_move_n_a(Input_Iterator first, Size n, Forward_Iterator result,
                                                Allocator& alloc) {
        return _uninitialized_move_n_a_generic(first, n, result, alloc);
    }

    // *************************************************************************************
    // uninitialized_default_construct_n
    template <typename Forward_Iterator, typename Size>
    constexpr Forward_Iterator _uninitialized_default_construct_n_generic(Forward_Iterator first, Size n) {
        auto current = first;
        try {
            for (; n > 0; --n, ++current) {
                mystl::construct(mystl::addressof(*current));
            }
        }catch (...) {
            mystl::destroy(first, current);
            throw;
        }
        return current;
    }

    template <typename Forward_Iterator, typename Size>
    constexpr Forward_Iterator uninitialized_default_construct_n(Forward_Iterator first, Size n) {
        if constexpr (is_trivially_default_constructible_v<iter_value_type<Forward_Iterator>>) {
            mystl::advance(first, n);
            return first;
        }else {
            return _uninitialized_default_construct_n_generic(first, n);
        }
    }

    // *************************************************************************************
    // uninitialized_default_construct_n_a
    template <typename Forward_Iterator, typename Size, typename Allocator>
    constexpr Forward_Iterator _uninitialized_default_construct_n_a_generic(Forward_Iterator first, Size n, Allocator& alloc) {
        using traits = allocator_traits<Allocator>;
        auto current = first;
        try {
            for (; n > 0; --n, ++current) {
                traits::construct(alloc, mystl::addressof(*current));
            }
        }catch (...) {
            for (; first != current; ++first) {
                traits::destroy(alloc, mystl::addressof(*first));
            }
            throw;
        }
        return current;
    }

    template <typename Forward_Iterator, typename Size, typename Allocator>
    constexpr Forward_Iterator uninitialized_default_construct_n_a(Forward_Iterator first, Size n, Allocator& alloc) {
        return _uninitialized_default_construct_n_a_generic(first, n, alloc);
    }

    // *************************************************************************************
    // uninitialized_default_construct
    template <typename Forward_Iterator>
    constexpr void _uninitialized_default_construct_generic_impl(Forward_Iterator first, Forward_Iterator last,
                                                forward_iterator_tag) {
        auto current = first;
        try {
            for (; current != last; ++current) {
                mystl::construct(mystl::addressof(*current));
            }
        }catch (...) {
            mystl::destroy(first, current);
            throw;
        }
    }

    template <typename Random_Iterator>
    constexpr void _uninitialized_default_construct_generic_impl(Random_Iterator first, Random_Iterator last,
                                                random_access_iterator_tag) {
        using diff_type = iter_difference_type<Random_Iterator>;
        diff_type n = last - first;
        _uninitialized_default_construct_n_generic(first, n);
    }

    template <typename Forward_Iterator>
    constexpr void _uninitialized_default_construct_generic(Forward_Iterator first, Forward_Iterator last) {
        _uninitialized_default_construct_generic_impl(first, last, iter_category<Forward_Iterator>{});
    }

    template <typename Forward_Iterator>
    constexpr void uninitialized_default_construct(Forward_Iterator first, Forward_Iterator last) {
        if constexpr (!is_trivially_default_constructible_v<iter_value_type<Forward_Iterator>>) {
            _uninitialized_default_construct_generic(first, last);
        }
    }

    // *************************************************************************************
    // uninitialized_default_construct_a
    template <typename Forward_Iterator, typename Allocator>
    constexpr void _uninitialized_default_construct_a_generic_impl(Forward_Iterator first, Forward_Iterator last,
                                                Allocator& alloc, forward_iterator_tag) {
        using traits = allocator_traits<Allocator>;
        auto current = first;
        try {
            for (; current != last; ++current) {
                traits::construct(alloc, mystl::addressof(*current));
            }
        }catch (...) {
            for (; first != current; ++first) {
                traits::destroy(alloc, mystl::addressof(*first));
            }
            throw;
        }
    }

    template <typename Random_Iterator, typename Allocator>
    constexpr void _uninitialized_default_construct_a_generic_impl(Random_Iterator first, Random_Iterator last,
                                                Allocator& alloc, random_access_iterator_tag) {
        using diff_type = iter_difference_type<Random_Iterator>;
        diff_type n = last - first;
        _uninitialized_default_construct_n_a_generic(first, n, alloc);
    }

    template <typename Forward_Iterator, typename Allocator>
    constexpr void _uninitialized_default_construct_a_generic(Forward_Iterator first, Forward_Iterator last, Allocator& alloc) {
        _uninitialized_default_construct_a_generic_impl(first, last, alloc, iter_category<Forward_Iterator>{});
    }

    template <typename Forward_Iterator, typename Allocator>
    constexpr void uninitialized_default_construct_a(Forward_Iterator first, Forward_Iterator last, Allocator& alloc) {
        _uninitialized_default_construct_a_generic(first, last, alloc);
    }


    // *************************************************************************************
    // uninitialized_value_construct
    template <typename Forward_Iterator>
    constexpr void _uninit_trivially_value_construct(Forward_Iterator first, Forward_Iterator last, forward_iterator_tag) {
        using ValueType = iter_value_type<Forward_Iterator>;
        for (; first != last; ++first) {
            memset(mystl::addressof(*first), 0, sizeof(ValueType));
        }
    }

    template <typename Random_Iterator>
    constexpr void _uninit_trivially_value_construct(Random_Iterator first, Random_Iterator last, random_access_iterator_tag) {
        using ValueType = iter_value_type<Random_Iterator>;
        const auto n = static_cast<size_t>(last - first);
        memset(mystl::addressof(*first), 0, n*sizeof(ValueType));
    }

    template <typename Iterator>
    constexpr void _uninit_value_construct(Iterator first, Iterator last, true_type) {
        _uninit_trivially_value_construct(first, last, iter_category<Iterator>{});
    }

    template <typename Forward_Iterator>
    constexpr void _uninit_value_construct(Forward_Iterator first, Forward_Iterator last, false_type) {
        using ValueType = iter_value_type<Forward_Iterator>;
        auto current = first;
        try {
            for (; current != last; ++current) {
                mystl::construct(mystl::addressof(*current));
            }
        }catch (...) {
            mystl::destroy(first, current);
            throw;
        }
    }

    template <typename Forward_Iterator>
    constexpr void uninitialized_value_construct(Forward_Iterator first, Forward_Iterator last) {
        _uninit_value_construct(first, last, false_type{});
    }

    // *************************************************************************************
    // uninitialized_value_construct_n
    template <typename Forward_Iterator, typename Size>
    constexpr Forward_Iterator _uninit_trivially_value_construct_n(Forward_Iterator first, Size n, forward_iterator_tag) {
        using ValueType = iter_value_type<Forward_Iterator>;
        auto current = first;
        for (; n > 0; --n, ++current) {
            memset(mystl::addressof(*current), 0, sizeof(ValueType));
        }
        return current;
    }

    template <typename Random_Iterator, typename Size>
    constexpr Random_Iterator _uninit_trivially_value_construct_n(Random_Iterator first, Size n, random_access_iterator_tag) {
        using ValueType = iter_value_type<Random_Iterator>;
        memset(mystl::addressof(*first), 0, n*sizeof(ValueType));
        mystl::advance(first, n);
        return first;
    }

    template <typename Iterator, typename Size>
    constexpr Iterator _uninit_value_construct_n(Iterator first, Size n, true_type) {
        return _uninit_trivially_value_construct_n(first, n, iter_category<Iterator>{});
    }

    template <typename Forward_Iterator, typename Size>
    constexpr Forward_Iterator _uninit_value_construct_n(Forward_Iterator first, Size n, false_type) {
        auto current = first;
        try {
            for (; n > 0; --n, ++current) {
                mystl::construct(mystl::addressof(*current));
            }
        }catch (...) {
            mystl::destroy(first, current);
            throw;
        }
        return current;
    }

    template <typename Forward_Iterator, typename Size>
    constexpr Forward_Iterator uninitialized_value_construct_n(Forward_Iterator first, Size n) {
        return _uninit_value_construct_n(first, n, false_type{});
    }


} // namespace mystl

#endif //UNINITIALIZED_H
