#pragma once

#ifndef ALGO_ORDERED_H
#define ALGO_ORDERED_H

#include "../iterator.h"
#include "../utility.h"
#include "../functional.h"
#include "heap_algo.h"
#include "temporary_buffer.h"

namespace mystl {
    // **********************************************************************************
    // is_sorted
    template <typename Forward_Iterator, typename Compare>
    constexpr bool is_sorted(Forward_Iterator first, Forward_Iterator last, Compare comp) {
        if (first == last) return true;
        Forward_Iterator second = first;
        for (++second; second != last; ++second, ++first) {
            if (comp(*second, *first)) return false;
        }
        return true;
    }

    template <typename Forward_Iterator>
    constexpr bool is_sorted(Forward_Iterator first, Forward_Iterator last) {
        return mystl::is_sorted(first, last, mystl::less<>{});
    }

    // is_sorted_until
    template <typename Forward_Iterator, typename Compare>
    constexpr Forward_Iterator is_sorted_until(Forward_Iterator first, Forward_Iterator last, Compare comp) {
        if (first == last) return last;
        Forward_Iterator second = first;
        for (++second; second != last; ++second, ++first) {
            if (comp(*second, *first)) return second;
        }
        return last;
    }

    template <typename Forward_Iterator>
    constexpr Forward_Iterator is_sorted_until(Forward_Iterator first, Forward_Iterator last) {
        return mystl::is_sorted_until(first, last, mystl::less<>{});
    }

    // **********************************************************************************
    // sort
    namespace detail {
        static constexpr size_t small_section_size = 32;

        constexpr size_t get_integral_log2(const size_t x) {
            size_t result = 0;
            for (size_t i = 1; i < x; ++result) i <<= 1;
            return result > 0 ? result - 1 : 0;
        }

        template <typename T, typename Compare>
        constexpr T median_of_three(const T& first, const T& second, const T& third, const Compare& comp) {
            if (comp(first, second)) {
                return comp(second, third) ? second : comp(first, third) ? third : first;
            }
            return comp(first, third) ? first : comp(second, third) ? third : second;
        }

        // insertion_sort algorithm, for small data size (less than small_section_size)
        template <typename Random_Iterator, typename Compare>
        constexpr void insertion_sort(Random_Iterator first, Random_Iterator last, const Compare& comp) {
            if (first == last) return;
            for (Random_Iterator it = first + 1; it != last; ++it) {
                if (auto value = mystl::move(*it); comp(value, *first)) {
                    mystl::copy_backward(first, it, it + 1);
                    *first = move(value);
                }else {
                    Random_Iterator prev = it;
                    while (prev != first && comp(value, *(prev - 1))) {
                        *prev = *(prev - 1);
                        --prev;
                    }
                    *prev = mystl::move(value);
                }
            }
        }

        // heap_sort algorithm, used when the recursive depth of quick_sort reaches its maximum
        template <typename Random_Iterator, typename Compare>
        constexpr void heap_sort(Random_Iterator first, Random_Iterator last, const Compare& comp) {
            mystl::make_heap(first, last, comp);
            mystl::sort_heap(first, last, comp);
        }

        // unchecked_partition, partition the data into two parts: the first part is smaller than pivot,
        // and the second part is bigger than pivot.
        template <typename Random_Iterator, typename Compare, typename Value = iter_value_type<Random_Iterator>>
        constexpr Random_Iterator unchecked_partition(Random_Iterator first, Random_Iterator last,
                                            const Compare& comp, const Value& pivot) {
            while (true) {
                while (first < last && comp(*first, pivot)) ++first;
                --last;
                while (first < last && comp(pivot, *last)) --last;
                if (first >= last) return first;
                mystl::iter_swap(first, last);
                ++first;
            }
        }

        // intro_sort algorithm
        template <typename Random_Iterator, typename Compare, typename Size>
        constexpr void intro_sort(Random_Iterator first, Random_Iterator last, const Compare& comp, Size recurse_limit) {
            // if data size less than small_section_size, using insertion sort
            if (static_cast<size_t>(last - first) < small_section_size) {
                insertion_sort(first, last, comp);
                return;
            }
            // if the recursive depth of quick_sort reaches its maximum, using heap_sort
            if (recurse_limit == 0) {
                heap_sort(first, last, comp);
                return;
            }
            // quick_sort
            --recurse_limit;
            using value_type = iter_value_type<Random_Iterator>;
            value_type pivot = median_of_three(*first, *(first + (last - first) / 2), *(last - 1), comp);
            Random_Iterator par_point = unchecked_partition(first, last, comp, pivot);

            intro_sort(first, par_point, comp, recurse_limit);
            intro_sort(par_point, last, comp, recurse_limit);
        }

        // sort implement
        template <typename Random_Iterator, typename Compare>
        constexpr void _sort(Random_Iterator first, Random_Iterator last, const Compare& comp) {
            intro_sort(first, last, comp, 2 * get_integral_log2(static_cast<size_t>(last - first)));
        }

    }

    template <typename Random_Iterator, typename Compare>
    constexpr void sort(Random_Iterator first, Random_Iterator last, Compare comp) {
        detail::_sort(first, last, comp);
    }

    template <typename Random_Iterator>
    constexpr void sort(Random_Iterator first, Random_Iterator last) {
        detail::_sort(first, last, mystl::less<>{});
    }

    // **********************************************************************************
    // partial_sort
    namespace detail {
        template <typename Random_Iterator, typename Compare>
        constexpr void sift_down(Random_Iterator first, Random_Iterator last, const Compare& comp) {
            using diff_type = iter_difference_type<Random_Iterator>;
            diff_type n = last - first;
            auto value = move(*first);
            diff_type i = 0;
            for (diff_type l_sub, r_sub, next_node; ; ) {
                l_sub = (i << 1) + 1;
                if (l_sub >= n) break;
                r_sub = l_sub + 1;
                next_node = r_sub < n ? (comp(*(first + l_sub), *(first + r_sub)) ? r_sub : l_sub) : l_sub;
                if (!comp(value, *(first + next_node))) break;
                *(first + i) = *(first + next_node);
                i = next_node;
            }
            *(first + i) = move(value);
        }

        template <typename Random_Iterator, typename Compare>
        constexpr void heap_select(Random_Iterator first, Random_Iterator middle, Random_Iterator last, const Compare& comp) {
            mystl::make_heap(first, middle, comp);
            for (Random_Iterator it = middle; it != last; ++it) {
                if (comp(*it, *first)) {
                    mystl::iter_swap(it, first);
                    sift_down(first, middle, comp);
                }
            }
        }

        template <typename Random_Iterator, typename Compare>
        constexpr void _partial_sort(Random_Iterator first, Random_Iterator middle, Random_Iterator last,
                                    const Compare& comp) {
            if (first == middle) return;
            if (middle == last) mystl::sort(first, last, comp);
            heap_select(first, middle, last, comp);
            mystl::sort_heap(first, middle, comp);
        }
    }

    template <typename Random_Iterator, typename Compare>
    constexpr void partial_sort(Random_Iterator first, Random_Iterator middle, Random_Iterator last, Compare comp) {
        detail::_partial_sort(first, middle, last, comp);
    }

    template <typename Random_Iterator>
    constexpr void partial_sort(Random_Iterator first, Random_Iterator middle, Random_Iterator last) {
        detail::_partial_sort(first, middle, last, mystl::less<>{});
    }

    // **********************************************************************************
    // partial_sort_copy
    namespace detail {
        template <typename Input_Iterator, typename Random_Iterator, typename Compare>
        constexpr void heap_select_copy(Input_Iterator first, Input_Iterator last,
                                    Random_Iterator d_first, Random_Iterator d_last, Compare comp) {
            mystl::make_heap(d_first, d_last, comp);
            for (; first != last; ++first) {
                if (comp(*first, *d_first)) {
                    *d_first = *first;
                    sift_down(d_first, d_last, comp);
                }
            }
        }

        template <typename Input_Iterator, typename Random_Iterator, typename Compare>
        constexpr Random_Iterator _partial_sort_copy(Input_Iterator first, Input_Iterator last,
                                    Random_Iterator d_first, Random_Iterator d_last, const Compare& comp) {
            Input_Iterator it = first;
            Random_Iterator d_it = d_first;
            for (; it != last && d_it != d_last; ++it, ++d_it) *d_it = *it;

            if (it == last) {
                detail::_sort(d_first, d_it, comp);
            }else {
                detail::heap_select_copy(it, last, d_first, d_it, comp);
                mystl::sort_heap(d_first, d_it, comp);
            }
            return d_it;
        }
    }

    template <typename Input_Iterator, typename Random_Iterator, typename Compare>
    constexpr Random_Iterator partial_sort_copy(Input_Iterator first, Input_Iterator last,
                                    Random_Iterator d_first, Random_Iterator d_last, Compare comp) {
        return detail::_partial_sort_copy(first, last, d_first, d_last, comp);
    }

    template <typename Input_Iterator, typename Random_Iterator>
    constexpr Random_Iterator partial_sort_copy(Input_Iterator first, Input_Iterator last,
                                    Random_Iterator d_first, Random_Iterator d_last) {
        return detail::_partial_sort_copy(first, last, d_first, d_last, mystl::less<>{});
    }

    // **********************************************************************************
    // nth_element
    template <typename Random_Iterator, typename Compare>
    constexpr void nth_element(Random_Iterator first, Random_Iterator nth, Random_Iterator last, Compare comp) {
        using value_type = iter_value_type<Random_Iterator>;
        constexpr size_t small_section_size = sizeof(value_type) <= 16 ? 16 : sizeof(value_type) <= 32 ? 12 : 8;
        if (first == last) return;
        while (last - first > small_section_size) {
            value_type pivot = detail::median_of_three(*first, *(first + (last - first) / 2), *(last - 1), comp);
            Random_Iterator par_point = detail::unchecked_partition(first, last, comp, pivot);
            if (nth < par_point) {
                last = par_point;
            }else {
                first = par_point;
            }
        }
        detail::insertion_sort(first, last, comp);
    }

    template <typename Random_Iterator>
    constexpr void nth_element(Random_Iterator first, Random_Iterator nth, Random_Iterator last) {
        mystl::nth_element(first, nth, last, less<>{});
    }

    // **********************************************************************************
    // lower_bound && upper_bound
    namespace detail {
        template <typename Forward_Iterator, typename T = iter_value_type<Forward_Iterator>, typename Compare>
        constexpr Forward_Iterator lower_bound_impl(Forward_Iterator first, Forward_Iterator last, const T& value,
                                                    const Compare& comp) {
            using diff_type = iter_difference_type<Forward_Iterator>;
            diff_type length = distance(first, last);
            Forward_Iterator it;
            while (length > 0) {
                diff_type step = length / 2;
                it = first;
                mystl::advance(it, step);
                if (comp(*it, value)) {
                    length -= step + 1;
                    first = ++it;
                }else {
                    length = step;
                }
            }
            return first;
        }

        template <typename Forward_Iterator, typename T = iter_value_type<Forward_Iterator>, typename Compare>
        constexpr Forward_Iterator upper_bound_impl(Forward_Iterator first, Forward_Iterator last, const T& value,
                                                    const Compare& comp) {
            using diff_type = iter_difference_type<Forward_Iterator>;
            diff_type length = distance(first, last);
            Forward_Iterator it;
            while (length > 0) {
                diff_type step = length / 2;
                it = first;
                mystl::advance(it, step);
                if (!comp(value, *it)) {
                    length -= step + 1;
                    first = ++it;
                }else {
                    length = step;
                }
            }
            return first;
        }
    }
    template <typename Forward_Iterator, typename T = iter_value_type<Forward_Iterator>, typename Compare>
    constexpr Forward_Iterator lower_bound(Forward_Iterator first, Forward_Iterator last, const T& value, Compare comp) {
        return detail::lower_bound_impl(first, last, value, comp);
    }

    template <typename Forward_Iterator, typename T = iter_value_type<Forward_Iterator>>
    constexpr Forward_Iterator lower_bound(Forward_Iterator first, Forward_Iterator last, const T& value) {
        return detail::lower_bound_impl(first, last, value, mystl::less<>{});
    }

    template <typename Forward_Iterator, typename T = iter_value_type<Forward_Iterator>, typename Compare>
    constexpr Forward_Iterator upper_bound(Forward_Iterator first, Forward_Iterator last, const T& value, Compare comp) {
        return detail::upper_bound_impl(first, last, value, comp);
    }

    template <typename Forward_Iterator, typename T = iter_value_type<Forward_Iterator>>
    constexpr Forward_Iterator upper_bound(Forward_Iterator first, Forward_Iterator last, const T& value) {
        return detail::upper_bound_impl(first, last, value, mystl::less<>{});
    }

    // **********************************************************************************
    // binary_search
    template <typename Forward_Iterator, typename T = iter_value_type<Forward_Iterator>, typename Compare>
    constexpr bool binary_search(Forward_Iterator first, Forward_Iterator last, const T& value, Compare comp) {
        first = lower_bound(first, last, value, comp);
        return !(first == last) && !comp(value, *first);
    }

    template <typename Forward_Iterator, typename T = iter_value_type<Forward_Iterator>>
    constexpr bool binary_search(Forward_Iterator first, Forward_Iterator last, const T& value) {
        return binary_search(first, last, value, less<>{});
    }

    // **********************************************************************************
    // equal_range
    template <typename Forward_Iterator, typename T = iter_value_type<Forward_Iterator>, typename Compare>
    constexpr pair<Forward_Iterator, Forward_Iterator> equal_range(Forward_Iterator first, Forward_Iterator last,
                                                                    const T& value, Compare comp) {
        return mystl::make_pair(lower_bound(first, last, value, comp), upper_bound(first, last, value, comp));
    }

    template <typename Forward_Iterator, typename T = iter_value_type<Forward_Iterator>>
    constexpr pair<Forward_Iterator, Forward_Iterator> equal_range(Forward_Iterator first, Forward_Iterator last,
                                                                    const T& value) {
        return mystl::make_pair(lower_bound(first, last, value, mystl::less<>{}),
                        upper_bound(first, last, value, mystl::less<>{}));
    }

    // **********************************************************************************
    // merge
    template <typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator, typename Compare>
    constexpr Output_Iterator merge(Input_Iterator1 first1, Input_Iterator1 last1,
                                    Input_Iterator2 first2, Input_Iterator2 last2,
                                    Output_Iterator result, Compare comp) {
        for (; first1 != last1; ++result) {
            if (first2 == last2) {
                return copy(first1, last1, result);
            }
            if (comp(*first2, *first1)) {
                *result = *first2;
                ++first2;
            }else {
                *result = *first1;
                ++first1;
            }
        }
        return copy(first2, last2, result);
    }

    template <typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator>
    constexpr Output_Iterator merge(Input_Iterator1 first1, Input_Iterator1 last1,
                                    Input_Iterator2 first2, Input_Iterator2 last2, Output_Iterator result) {
        return merge(first1, last1, first2, last2, result, less<>{});
    }

    // inplace_merge
    namespace detail {
        // accomplish inplace_merge without extra place
        template <typename Bidirectional_Iterator, typename Compare>
        constexpr void inplace_merge_without_additional_mem(Bidirectional_Iterator first, Bidirectional_Iterator middle,
                                                Bidirectional_Iterator last, const Compare& comp) {
            if (first == middle || middle == last) return;
            if (!comp(*middle, *(middle-1))) return;
            using diff_type = iter_difference_type<Bidirectional_Iterator>;
            diff_type length1 = distance(first, middle);
            diff_type length2 = distance(middle, last);
            if (length1 + length2 == 2) {
                if (comp(*middle, *first)) iter_swap(first, middle);
                return;
            }

            Bidirectional_Iterator first_cut, second_cut;
            if (length1 > length2) {
                first_cut = first;
                advance(first_cut, length1 / 2);
                second_cut = detail::lower_bound_impl(middle, last, *first_cut, comp);
            }else {
                second_cut = middle;
                advance(second_cut, length2 / 2);
                first_cut = detail::upper_bound_impl(first, middle, *second_cut, comp);
            }
            Bidirectional_Iterator new_middle = rotate(first_cut, middle, second_cut);

            inplace_merge_without_additional_mem(first, first_cut, new_middle, comp);
            inplace_merge_without_additional_mem(new_middle, second_cut, last, comp);
        }

        // inplace_merge with small extra place ( the size of buffer < min(length1, length2) )
        template <typename Bidirectional_Iterator, typename Compare, typename T = iter_value_type<Bidirectional_Iterator>,
                    typename Pointer = T*, typename Distance>
        constexpr void inplace_merge_small_additional_mem(Bidirectional_Iterator first, Bidirectional_Iterator middle,
                                                Bidirectional_Iterator last, const Compare& comp,
                                                Pointer buffer, Distance buffer_size) {
            Bidirectional_Iterator it1 = first;
            Bidirectional_Iterator it2 = middle;
            Pointer buf_ptr = buffer;
            Pointer buf_end = buffer + buffer_size;
            while (it1 != middle && it2 != last) {
                if (comp(*it2, *it1)) {
                    *buf_ptr = move(*it2);
                    ++it2;
                }else {
                    *buf_ptr = move(*it1);
                    ++it1;
                }
                ++buf_ptr;
                if (buf_ptr == buf_end) {
                    move_backward(it1, middle, it2);
                    first = move(buffer, buf_end, first);
                    it1 = first;
                    middle = it2;
                    buf_ptr = buffer;
                }
            }
            if (it1 != middle) {
                move_backward(it1, middle, last);
            }
            move(buffer, buf_ptr, first);
        }

        // move_merge && move_merge_backward
        // merge_forward, first1 ~ last1 is the second segment of data
        template <typename Bidirectional_Iterator, typename Compare>
        constexpr void merge_forward(Bidirectional_Iterator first1, Bidirectional_Iterator last1,
                                    Bidirectional_Iterator buffer, Bidirectional_Iterator buffer_end,
                                    Bidirectional_Iterator result, const Compare& comp) {
            for (; first1 != last1; ++result) {
                if (buffer == buffer_end) return;
                if (comp(*first1, *buffer)) {
                    *result = move(*first1);
                    ++first1;
                }else {
                    *result = move(*buffer);
                    ++buffer;
                }
            }
            move(buffer, buffer_end, result);
        }

        // merge_backward, first1 ~ last1 is the first segment of data
        template <typename Bidirectional_Iterator, typename Compare>
        constexpr void merge_backward(Bidirectional_Iterator first1, Bidirectional_Iterator last1,
                                    Bidirectional_Iterator buffer, Bidirectional_Iterator buffer_end,
                                    Bidirectional_Iterator result, const Compare& comp) {
            while (first1 != last1) {
                if (buffer == buffer_end) return;
                if (comp(*--buffer_end, *--last1)) {
                    *--result = move(*last1);
                    ++buffer_end;
                }else {
                    *--result = move(*buffer_end);
                    ++last1;
                }
            }
            move_backward(buffer, buffer_end, result);
        }

        // inplace merge with enough extra place
        template <typename Bidirectional_Iterator, typename Compare, typename T = iter_value_type<Bidirectional_Iterator>,
                    typename Pointer = T*, typename Distance>
        constexpr void inplace_merge_additional_mem(Bidirectional_Iterator first, Bidirectional_Iterator middle,
                                                Bidirectional_Iterator last, const Compare& comp,
                                                Pointer buffer, Distance buffer_size, Distance len1, Distance len2) {
            if (buffer_size >= len1) {
                Pointer buffer_end = move(first, middle, buffer);
                merge_forward(middle, last, buffer, buffer_end, first, comp);
            }else if (buffer_size >= len2) {
                Pointer buffer_end = move(middle, last, buffer);
                merge_backward(first, middle, buffer, buffer_end, last, comp);
            }else {
                inplace_merge_small_additional_mem(first, middle, last, comp, buffer, buffer_size);
            }
        }

        // inplace merge implement
        template <typename Bidirectional_Iterator, typename Compare>
        constexpr void _inplace_merge(Bidirectional_Iterator first, Bidirectional_Iterator middle,
                                                Bidirectional_Iterator last, const Compare& comp) {
            if (first == middle || middle == last) return;
            if (!comp(*middle, *(middle-1))) return;
            if (comp(*(last - 1), *first)) {
                rotate(first, middle, last);
                return;
            }
            using diff_type = iter_difference_type<Bidirectional_Iterator>;
            using value_type = iter_value_type<Bidirectional_Iterator>;
            diff_type len1 = distance(first, middle);
            diff_type len2 = distance(middle, last);
            temporary_buffer<Bidirectional_Iterator, value_type> temp_buffer(first, min(len1, len2));
            auto buffer = temp_buffer.begin();
            auto buffer_size = temp_buffer.size();
            if (buffer == nullptr || buffer_size == 0) {
                inplace_merge_without_additional_mem(first, middle, last, comp);
            }else {
                inplace_merge_additional_mem(first, middle, last, comp, buffer, buffer_size, len1, len2);
            }
        }

    }

    template <typename Bidirectional_Iterator, typename Compare>
    constexpr void inplace_merge(Bidirectional_Iterator first, Bidirectional_Iterator middle,
                                                Bidirectional_Iterator last, Compare comp) {
        detail::_inplace_merge(first, middle, last, comp);
    }

    template <typename Bidirectional_Iterator>
    constexpr void inplace_merge(Bidirectional_Iterator first, Bidirectional_Iterator middle, Bidirectional_Iterator last) {
        detail::_inplace_merge(first, middle, last, mystl::less<>{});
    }

    // **********************************************************************************
    // stable_sort
    namespace detail {
        template <typename Random_Iterator, typename Compare, typename T = iter_value_type<Random_Iterator>,
                    typename Pointer = T*, typename Distance>
        constexpr void stable_intro_sort(Random_Iterator first, Random_Iterator last, const Compare& comp,
                                Pointer buffer, Distance buffer_size) {
            using diff_type = iter_difference_type<Random_Iterator>;
            diff_type n = last - first;
            // if data size less than small_section_size, using insertion sort
            if (n < small_section_size) {
                insertion_sort(first, last, comp);
                return;
            }
            // merge sort
            Random_Iterator mid = first + n / 2;
            stable_intro_sort(first, mid, comp, buffer, buffer_size);
            stable_intro_sort(mid, last, comp, buffer, buffer_size);

            // merge the sorted first half and second half
            if (buffer == nullptr || buffer_size == 0) {
                inplace_merge_without_additional_mem(first, mid, last, comp);
            }else {
                inplace_merge_additional_mem(first, mid, last, comp, buffer, buffer_size,
                                            mid - first, last - mid);
            }
        }

        template <typename Random_Iterator, typename Compare>
        constexpr void _stable_sort(Random_Iterator first, Random_Iterator last, const Compare& comp) {
            using diff_type = iter_difference_type<Random_Iterator>;
            using value_type = iter_value_type<Random_Iterator>;
            diff_type n = last - first;
            if (n < small_section_size) {
                insertion_sort(first, last, comp);
                return;
            }
            temporary_buffer<Random_Iterator, value_type> temp_buffer(first, (n + 1) / 2);
            auto buffer = temp_buffer.begin();
            auto size = temp_buffer.size();
            stable_intro_sort(first, last, comp, buffer, size);
        }
    }

    template <typename Random_Iterator, typename Compare>
    constexpr void stable_sort(Random_Iterator first, Random_Iterator last, Compare comp) {
        detail::_stable_sort(first, last, comp);
    }

    template <typename Random_Iterator>
    constexpr void stable_sort(Random_Iterator first, Random_Iterator last) {
        detail::_stable_sort(first, last, mystl::less<>{});
    }

    // **********************************************************************************
    // set algorithms
    namespace detail {
        // includes implement
        template<typename Input_Iterator1, typename Input_Iterator2, typename Compare>
        constexpr bool _includes(Input_Iterator1 first1, Input_Iterator1 last1,
                                Input_Iterator2 first2, Input_Iterator2 last2, const Compare& comp) {
            for (; first2 != last2; ++first1) {
                if (first1 == last1 || comp(*first2, *first1)) return false;
                if (!comp(*first1, *first2)) ++first2;
            }
            return true;
        }

        // set_difference implement
        template<typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator, typename Compare>
        constexpr Output_Iterator _set_difference(Input_Iterator1 first1, Input_Iterator1 last1,
                        Input_Iterator2 first2, Input_Iterator2 last2, Output_Iterator result, const Compare& comp) {
            while (first1 != last1) {
                if (first2 == last2) return mystl::copy(first1, last1, result);
                if (comp(*first1, *first2)) {
                    *result = *first1;
                    ++result;
                    ++first1;
                }else {
                    if (!comp(*first2, *first1)) ++first1;
                    ++first2;
                }
            }
            return result;
        }

        // set_intersection implement
        template<typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator, typename Compare>
        constexpr Output_Iterator _set_intersection(Input_Iterator1 first1, Input_Iterator1 last1,
                        Input_Iterator2 first2, Input_Iterator2 last2, Output_Iterator result, const Compare& comp) {
            while (first1 != last1 && first2 != last2) {
                if (comp(*first1, *first2)) {
                    ++first1;
                }else {
                    if (!comp(*first2, *first1)) {
                        *result = *first1;
                        ++result;
                        ++first1;
                    }
                    ++first2;
                }
            }
            return result;
        }

        // set_symmetric_difference implement
        template<typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator, typename Compare>
        constexpr Output_Iterator _set_symmetric_difference(Input_Iterator1 first1, Input_Iterator1 last1,
                        Input_Iterator2 first2, Input_Iterator2 last2, Output_Iterator result, const Compare& comp) {
            while (first1 != last1) {
                if (first2 == last2) return mystl::copy(first1, last1, result);
                if (comp(*first1, *first2)) {
                    *result = *first1;
                    ++result;
                    ++first1;
                }else if (comp(*first2, *first1)) {
                    *result = *first2;
                    ++result;
                    ++first2;
                }else {
                    ++first1;
                    ++first2;
                }
            }
            return mystl::copy(first2, last2, result);
        }

        // set_union implement
        template<typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator, typename Compare>
        constexpr Output_Iterator _set_union(Input_Iterator1 first1, Input_Iterator1 last1,
                        Input_Iterator2 first2, Input_Iterator2 last2, Output_Iterator result, const Compare& comp) {
            for (; first1 != last1; ++result) {
                if (first2 == last2) return mystl::copy(first1, last1, result);
                if (comp(*first1, *first2)) {
                    *result = *first1;
                    ++first1;
                }else if (comp(*first2, *first1)) {
                    *result = *first2;
                    ++first2;
                }else {
                    *result = *first1;
                    ++first1;
                    ++first2;
                }
            }
            return mystl::copy(first2, last2, result);
        }
    }

    // includes
    template<typename Input_Iterator1, typename Input_Iterator2, typename Compare>
    constexpr bool includes(Input_Iterator1 first1, Input_Iterator1 last1,
                                Input_Iterator2 first2, Input_Iterator2 last2, Compare comp) {
        return detail::_includes(first1, last1, first2, last2, comp);
    }

    template <typename Input_Iterator1, typename Input_Iterator2>
    constexpr bool includes(Input_Iterator1 first1, Input_Iterator1 last1,
                               Input_Iterator2 first2, Input_Iterator2 last2) {
        return detail::_includes(first1, last1, first2, last2, mystl::less<>{});
    }

    // set_difference
    template <typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator, typename Compare>
    constexpr Output_Iterator set_difference(Input_Iterator1 first1, Input_Iterator1 last1,
                        Input_Iterator2 first2, Input_Iterator2 last2, Output_Iterator result, Compare comp) {
        return detail::_set_difference(first1, last1, first2, last2, result, comp);
    }

    template <typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator>
    constexpr Output_Iterator set_difference(Input_Iterator1 first1, Input_Iterator1 last1,
                        Input_Iterator2 first2, Input_Iterator2 last2, Output_Iterator result) {
        return detail::_set_difference(first1, last1, first2, last2, result, mystl::less<>{});
    }

    // set_intersection
    template<typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator, typename Compare>
    constexpr Output_Iterator set_intersection(Input_Iterator1 first1, Input_Iterator1 last1,
                       Input_Iterator2 first2, Input_Iterator2 last2, Output_Iterator result, Compare comp) {
        return detail::_set_intersection(first1, last1, first2, last2, result, comp);
    }

    template<typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator>
    constexpr Output_Iterator set_intersection(Input_Iterator1 first1, Input_Iterator1 last1,
                       Input_Iterator2 first2, Input_Iterator2 last2, Output_Iterator result) {
        return detail::_set_intersection(first1, last1, first2, last2, result, mystl::less<>{});
    }

    // set_symmetric_difference
    template<typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator, typename Compare>
    constexpr Output_Iterator set_symmetric_difference(Input_Iterator1 first1, Input_Iterator1 last1,
                       Input_Iterator2 first2, Input_Iterator2 last2, Output_Iterator result, Compare comp) {
        return detail::_set_symmetric_difference(first1, last1, first2, last2, result, comp);
    }

    template<typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator>
    constexpr Output_Iterator set_symmetric_difference(Input_Iterator1 first1, Input_Iterator1 last1,
                       Input_Iterator2 first2, Input_Iterator2 last2, Output_Iterator result) {
        return detail::_set_symmetric_difference(first1, last1, first2, last2, result, mystl::less<>{});
    }

    // set_union
    template<typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator, typename Compare>
    constexpr Output_Iterator set_union(Input_Iterator1 first1, Input_Iterator1 last1,
                       Input_Iterator2 first2, Input_Iterator2 last2, Output_Iterator result, Compare comp) {
        return detail::_set_union(first1, last1, first2, last2, result, comp);
    }

    template<typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator>
    constexpr Output_Iterator set_union(Input_Iterator1 first1, Input_Iterator1 last1,
                       Input_Iterator2 first2, Input_Iterator2 last2, Output_Iterator result) {
        return detail::_set_union(first1, last1, first2, last2, result, mystl::less<>{});
    }


} // namespace mystl

#endif //ALGO_ORDERED_H
