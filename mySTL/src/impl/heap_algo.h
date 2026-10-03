#pragma once

#ifndef HEAP_ALGO_H
#define HEAP_ALGO_H

#include "../functional.h"
#include "../iterator.h"
#include "type_traits_normal.h"

namespace mystl {
    // **********************************************************************************
    // is_heap
    template <class Random_Iterator, class Compare>
    constexpr bool is_heap(Random_Iterator first, Random_Iterator last, Compare comp) {
        using diff_type = iter_difference_type<Random_Iterator>;
        const diff_type n = last - first;
        if (n <= 1) return true;
        Random_Iterator child = first + 1;
        for (diff_type i = 1; i < n; ++i, ++child) {
            Random_Iterator parent = first + ((i - 1) >> 1);
            if (comp(*parent, *child)) return false;
        }
        return true;
    }

    template <class Random_Iterator>
    constexpr bool is_heap(Random_Iterator first, Random_Iterator last) {
        return mystl::is_heap(first, last, mystl::less<>{});
    }

    // is_heap_until
    template <class Random_Iterator, class Compare>
    constexpr Random_Iterator is_heap_until(Random_Iterator first, Random_Iterator last, Compare comp) {
        using diff_type = iter_difference_type<Random_Iterator>;
        const diff_type n = last - first;
        if (n <= 1) return last;
        Random_Iterator child = first + 1;
        for (diff_type i = 1; i < n; ++i, ++child) {
            Random_Iterator parent = first + ((i - 1) >> 1);
            if (comp(*parent, *child)) return child;
        }
        return last;
    }

    template <class Random_Iterator>
    constexpr Random_Iterator is_heap_until(Random_Iterator first, Random_Iterator last) {
        return mystl::is_heap_until(first, last, mystl::less<>{});
    }

    // **********************************************************************************
    // push_heap
    namespace detail {
        template <class Random_Iterator, class Compare>
        constexpr void _push_heap(Random_Iterator first, Random_Iterator last, Compare& comp) {
            using diff_type = iter_difference_type<Random_Iterator>;
            const diff_type n = last - first;
            if (n <= 1) return;
            diff_type child = n - 1;
            auto value = mystl::move(*(first + child));
            while (child > 0) {
                diff_type parent = (child - 1) >> 1;
                if (!comp(*(first + parent), value)) break;
                if constexpr (is_move_assignable_v<iter_value_type<Random_Iterator>>) {
                    *(first + child) = mystl::move(*(first + parent));
                } else {
                    *(first + child) = *(first + parent);
                }
                child = parent;
            }
            *(first + child) = mystl::move(value);
        }
    }

    template <class Random_Iterator, class Compare>
    constexpr void push_heap(Random_Iterator first, Random_Iterator last, Compare comp) {
        detail::_push_heap(first, last, comp);
    }

    template <class Random_Iterator>
    constexpr void push_heap(Random_Iterator first, Random_Iterator last) {
        mystl::push_heap(first, last, mystl::less<>{});
    }

    // **********************************************************************************
    // pop_heap
    namespace detail {
        template <class Random_Iterator, class Compare>
        constexpr void _pop_heap(Random_Iterator first, Random_Iterator last, Compare& comp) {
            using diff_type = iter_difference_type<Random_Iterator>;
            const diff_type n = last - first - 1;
            if (n <= 0) return;
            mystl::swap(*first, *(first + n));
            auto value = mystl::move(*first);
            diff_type i = 0;
            for (diff_type l_idx, r_idx, next_node_idx; ; ) {
                l_idx = (i << 1) + 1;
                if (l_idx >= n) break;
                r_idx = l_idx + 1;
                next_node_idx = r_idx < n ? (comp(*(first + l_idx), *(first + r_idx)) ? r_idx : l_idx) : l_idx;
                if (!comp(value, *(first + next_node_idx))) break;
                if constexpr (is_move_assignable_v<iter_value_type<Random_Iterator>>) {
                    *(first + i) = mystl::move(*(first + next_node_idx));
                } else {
                    *(first + i) = *(first + next_node_idx);
                }
                i = next_node_idx;
            }
            *(first + i) = mystl::move(value);
        }
    }

    template <class Random_Iterator, class Compare>
    constexpr void pop_heap(Random_Iterator first, Random_Iterator last, Compare comp) {
        detail::_pop_heap(first, last, comp);
    }

    template <class Random_Iterator>
    constexpr void pop_heap(Random_Iterator first, Random_Iterator last) {
        mystl::pop_heap(first, last, mystl::less<>{});
    }

    // **********************************************************************************
    // make_heap
    namespace detail {
        template <class Random_Iterator, class Compare>
        constexpr void _make_heap(Random_Iterator first, Random_Iterator last, Compare& comp) {
            using diff_type = iter_difference_type<Random_Iterator>;
            const diff_type n = last - first;
            if (n <= 1) return;
            const diff_type max_i = n >> 1;
            for (diff_type i = max_i; i-- > 0; ) {
                auto value = mystl::move(*(first + i));
                diff_type parent = i;
                for (diff_type l_sub, r_sub, next_node; ; ) {
                    l_sub = (parent << 1) + 1;
                    if (l_sub >= n) break;
                    r_sub = l_sub + 1;
                    next_node = r_sub < n ? (comp(*(first + l_sub), *(first + r_sub)) ? r_sub : l_sub) : l_sub;
                    if (!comp(value, *(first + next_node))) break;
                    if constexpr (is_move_assignable_v<iter_value_type<Random_Iterator>>) {
                        *(first + parent) = mystl::move(*(first + next_node));
                    } else {
                        *(first + parent) = *(first + next_node);
                    }
                    parent = next_node;
                }
                *(first + parent) = mystl::move(value);
            }
        }
    }
    template <class Random_Iterator, class Compare>
    constexpr void make_heap(Random_Iterator first, Random_Iterator last, Compare comp) {
        detail::_make_heap(first, last, comp);
    }

    template <class Random_Iterator>
    constexpr void make_heap(Random_Iterator first, Random_Iterator last) {
        mystl::make_heap(first, last, mystl::less<>{});
    }

    // **********************************************************************************
    // sort_heap
    template <class Random_Iterator, class Compare>
    constexpr void sort_heap(Random_Iterator first, Random_Iterator last, Compare comp) {
        while (first != last) {
            detail::_pop_heap(first, last, comp);
            --last;
        }
    }

    template <class Random_Iterator>
    constexpr void sort_heap(Random_Iterator first, Random_Iterator last) {
        mystl::sort_heap(first, last, mystl::less<>{});
    }


} // namespace mystl

#endif //HEAP_ALGO_H
