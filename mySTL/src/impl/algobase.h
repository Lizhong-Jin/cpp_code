#pragma once

#ifndef ALGOBASE_H
#define ALGOBASE_H

#include "../cstring.h"
#include "../iterator.h"
#include "../utility.h"
#include "../functional.h"

namespace mystl {
#ifdef max
#pragma message ("#undefing macro max")
#undef max
#endif // max

#ifdef min
#pragma message ("#undef macro min")
#undef min
#endif // min

    // **********************************************************************************
    // max
    template <typename T>
    constexpr const T& max(const T& lhs, const T& rhs) {
        return mystl::less<>{}(lhs, rhs) ? rhs : lhs;
    }
    template <typename T, typename Compare>
    constexpr const T& max(const T& lhs, const T& rhs, Compare comp) {
        return comp(lhs, rhs) ? rhs : lhs;
    }

    // **********************************************************************************
    // min
    template <typename T>
    constexpr const T& min(const T& lhs, const T& rhs) {
        return mystl::less<>{}(lhs, rhs) ? lhs : rhs;
    }
    template <typename T, typename Compare>
    constexpr const T& min(const T& lhs, const T& rhs, Compare comp) {
        return comp(lhs, rhs) ? lhs : rhs;
    }

    // **********************************************************************************
    // max_element
    template <typename Forward_Iterator>
    constexpr Forward_Iterator max_element(Forward_Iterator first, Forward_Iterator last) {
        if (first == last) return first;
        Forward_Iterator result = first;
        while (++first != last) {
            if (mystl::less<>{}(*result, *first)) result = first;
        }
        return result;
    }

    template <typename Forward_Iterator, typename Compare>
    constexpr Forward_Iterator max_element(Forward_Iterator first, Forward_Iterator last, Compare comp) {
        if (first == last) return first;
        Forward_Iterator result = first;
        while (++first != last) {
            if (comp(*result, *first)) result = first;
        }
        return result;
    }

    // **********************************************************************************
    // min_element
    template <typename Forward_Iterator>
    constexpr Forward_Iterator min_element(Forward_Iterator first, Forward_Iterator last) {
        if (first == last) return first;
        Forward_Iterator result = first;
        while (++first != last) {
            if (mystl::less<>{}(*first, *result)) result = first;
        }
        return result;
    }

    template <typename Forward_Iterator, typename Compare>
    constexpr Forward_Iterator min_element(Forward_Iterator first, Forward_Iterator last, Compare comp) {
        if (first == last) return first;
        Forward_Iterator result = first;
        while (++first != last) {
            if (comp(*first, *result)) result = first;
        }
        return result;
    }

    // **********************************************************************************
    // minmax
    template <typename T>
    constexpr pair<const T&, const T&> minmax(const T& lhs, const T& rhs) {
        return mystl::less<>{}(lhs, rhs) ? mystl::make_pair(lhs, rhs) : mystl::make_pair(rhs, lhs);
    }

    template <typename T, typename Compare>
    constexpr pair<const T&, const T&> minmax(const T& lhs, const T& rhs, Compare comp) {
        return comp(lhs, rhs) ? mystl::make_pair(lhs, rhs) : mystl::make_pair(rhs, lhs);
    }

    // minmax_element
    template <typename Forward_Iterator, typename Compare>
    constexpr pair<Forward_Iterator, Forward_Iterator> minmax_element(Forward_Iterator first,
                                                        Forward_Iterator last, Compare comp) {
        Forward_Iterator min = first, max = first;
        if (first == last || ++first == last) return mystl::make_pair(min, max);

        for (Forward_Iterator second = first; first != last; ) {
            if (++second == last) {
                if (comp(*first, *min)) min = first;
                else if (comp(*max, *first)) max = first;
                break;
            }
            if (comp(*first, *second)) {
                if (comp(*min, *first)) min = first;
                if (comp(*second, *max)) max = second;
            }else {
                if (comp(*min, *second)) min = second;
                if (comp(*first, *max)) max = first;
            }
            first = ++second;
        }
        return mystl::make_pair(min, max);
    }

    template <typename Forward_Iterator>
    constexpr pair<Forward_Iterator, Forward_Iterator> minmax_element(Forward_Iterator first, Forward_Iterator last) {
        return mystl::minmax_element(first, last, less<>{});
    }

    // **********************************************************************************
    // clamp
    template <typename T>
    constexpr const T& clamp(const T& v, const T& low, const T& high) {
        return mystl::less<>{}(v, low) ? low : mystl::less<>{}(high, v) ? high : v;
    }

    template <typename T, typename Compare>
    constexpr const T& clamp(const T& v, const T& low, const T& high, Compare comp) {
        return comp(v, low) ? low : comp(high, v) ? high : v;
    }

    // **********************************************************************************
    // equal
    template <typename Input_Iterator1, typename Input_Iterator2>
    bool equal(Input_Iterator1 first1, Input_Iterator1 last1, Input_Iterator2 first2) {
        while (first1 != last1) {
            if (*first1 != *first2) return false;
            ++first1; ++first2;
        }
        return true;
    }

    template <typename Input_Iterator1, typename Input_Iterator2, typename Compare>
    bool equal(Input_Iterator1 first1, Input_Iterator1 last1, Input_Iterator2 first2, Compare comp) {
        while (first1 != last1) {
            if (! comp(*first1, *first2)) return false;
            ++first1; ++first2;
        }
        return true;
    }

    // **********************************************************************************
    // lexicographical_compare
    template <typename Input_Iterator1, typename Input_Iterator2>
    bool lexicographical_compare(Input_Iterator1 first1, Input_Iterator1 last1,
                                Input_Iterator2 first2, Input_Iterator2 last2) {
        constexpr auto comp = mystl::less<>{};
        while (first1 != last1 && first2 != last2) {
            if (comp(*first1, *first2)) return true;
            if (comp(*first2, *first1)) return false;
            ++first1; ++first2;
        }
        return first1 == last1 && first2 != last2;
    }

    template <typename Input_Iterator1, typename Input_Iterator2, typename Compare>
    bool lexicographical_compare(Input_Iterator1 first1, Input_Iterator1 last1,
                                Input_Iterator2 first2, Input_Iterator2 last2, Compare comp) {
        while (first1 != last1 && first2 != last2) {
            if (comp(*first1, *first2)) return true;
            if (comp(*first2, *first1)) return false;
            ++first1; ++first2;
        }
        return first1 == last1 && first2 != last2;
    }

    // specialization for const unsigned char* trait
    inline bool lexicographical_compare(const unsigned char* first1, const unsigned char* last1,
                                const unsigned char* first2, const unsigned char* last2) {
        const auto n1 = last1 - first1;
        const auto n2 = last2 - first2;
        const auto result = mystl::memcmp(first1, first2, mystl::min(n1, n2));
        return result != 0 ? result < 0 : n1 < n2;
    }

    // **********************************************************************************
    // lexicographical_compare_three_way


    // *******************************************************************
    // all_of && any_of && none_of
    template <typename Input_Iterator, typename UnaryPred>
    constexpr bool all_of(Input_Iterator first, Input_Iterator last, UnaryPred pred) {
        for (; first != last; ++first) {
            if (!pred(*first)) return false;
        }
        return true;
    }

    template <typename Input_Iterator, typename UnaryPred>
    constexpr bool any_of(Input_Iterator first, Input_Iterator last, UnaryPred pred) {
        for (; first != last; ++first) {
            if (pred(*first)) return true;
        }
        return false;
    }

    template <typename Input_Iterator, typename UnaryPred>
    constexpr bool none_of(Input_Iterator first, Input_Iterator last, UnaryPred pred) {
        for (; first != last; ++first) {
            if (pred(*first)) return false;
        }
        return true;
    }

    // *******************************************************************
    // for_each
    template <typename Input_Iterator, typename UnaryFunc>
    constexpr UnaryFunc for_each(Input_Iterator first, Input_Iterator last, UnaryFunc f) {
        for (; first != last; ++first) f(*first);
        return f;
    }

    // for_each_n
    template <typename Input_Iterator, typename Size, typename UnaryFunc>
    constexpr UnaryFunc for_each_n(Input_Iterator first, Size n, UnaryFunc f) {
        for (; n > 0; --n, ++first) f(*first);
        return f;
    }

    // *******************************************************************
    // count
    template <typename Input_Iterator, typename T = iter_value_type<Input_Iterator>>
    constexpr iter_difference_type<Input_Iterator> count(Input_Iterator first, Input_Iterator last, const T& value) {
        iter_difference_type<Input_Iterator> count = 0;
        for (; first != last; ++first) {
            if (*first == value) ++count;
        }
        return count;
    }

    // count_if
    template <typename Input_Iterator, typename UnaryPred>
    constexpr iter_difference_type<Input_Iterator> count_if(Input_Iterator first, Input_Iterator last, UnaryPred pred) {
        iter_difference_type<Input_Iterator> count = 0;
        for (; first != last; ++first) {
            if (pred(*first)) ++count;
        }
        return count;
    }

    // *******************************************************************
    // find && find_if && find_if_not
    template <typename Input_Iterator, typename T = iter_value_type<Input_Iterator>>
    constexpr Input_Iterator find(Input_Iterator first, Input_Iterator last, const T& value) {
        while (first != last && *first != value) ++first;
        return first;
    }

    template <typename Input_Iterator, typename UnaryPred>
    constexpr Input_Iterator find_if(Input_Iterator first, Input_Iterator last, UnaryPred pred) {
        while (first != last && !pred(*first)) ++first;
        return first;
    }

    template <typename Input_Iterator, typename UnaryPred>
    constexpr Input_Iterator find_if_not(Input_Iterator first, Input_Iterator last, UnaryPred pred) {
        while (first != last && pred(*first)) ++first;
        return first;
    }

    // *******************************************************************
    // mismatch
    template <typename Input_Iterator1, typename Input_Iterator2, typename BinaryPred>
    constexpr pair<Input_Iterator1, Input_Iterator2> mismatch(Input_Iterator1 first1, Input_Iterator1 last1,
                                                                    Input_Iterator2 first2, BinaryPred pred) {
        while (first1 != last1 && pred(*first1, *first2)) {
            ++first1; ++first2;
        }
        return mystl::make_pair(first1, first2);
    }

    template <typename Input_Iterator1, typename Input_Iterator2>
    constexpr pair<Input_Iterator1, Input_Iterator2> mismatch(Input_Iterator1 first1, Input_Iterator1 last1,
                                                                    Input_Iterator2 first2) {
        while (first1 != last1 && *first1 == *first2) {
            ++first1; ++first2;
        }
        return mystl::make_pair(first1, first2);
    }

    template <typename Input_Iterator1, typename Input_Iterator2, typename BinaryPred>
    constexpr pair<Input_Iterator1, Input_Iterator2> mismatch(Input_Iterator1 first1, Input_Iterator1 last1,
                                                Input_Iterator2 first2, Input_Iterator2 last2, BinaryPred pred) {
        while (first1 != last1 && first2 != last2 && pred(*first1, *first2)) {
            ++first1; ++first2;
        }
        return mystl::make_pair(first1, first2);
    }

    template <typename Input_Iterator1, typename Input_Iterator2>
    constexpr pair<Input_Iterator1, Input_Iterator2> mismatch(Input_Iterator1 first1, Input_Iterator1 last1,
                                                Input_Iterator2 first2, Input_Iterator2 last2) {
        while (first1 != last1 && first2 != last2 && *first1 == *first2) {
            ++first1; ++first2;
        }
        return mystl::make_pair(first1, first2);
    }

    // *******************************************************************
    // find_first_of
    template <typename Input_Iterator, typename Forward_Iterator>
    constexpr Input_Iterator find_first_of(Input_Iterator first, Input_Iterator last,
                                        Forward_Iterator s_first, Forward_Iterator s_last) {
        for (; first != last; ++first) {
            for (Forward_Iterator s_it = s_first; s_it != s_last; ++s_it) {
                if (*first == *s_it) return first;
            }
        }
        return last;
    }

    template <typename Input_Iterator, typename Forward_Iterator, typename BinaryPred>
    constexpr Input_Iterator find_first_of(Input_Iterator first, Input_Iterator last,
                                        Forward_Iterator s_first, Forward_Iterator s_last, BinaryPred b_pred) {
        for (; first != last; ++first) {
            for (Forward_Iterator s_it = s_first; s_it != s_last; ++s_it) {
                if (b_pred(*first, *s_it)) return first;
            }
        }
        return last;
    }

    // *******************************************************************
    // adjacent_find
    template <typename Forward_Iterator>
    constexpr Forward_Iterator adjacent_find(Forward_Iterator first, Forward_Iterator last) {
        if (first == last) return first;
        Forward_Iterator second = first;
        ++second;
        for (; second != last; ++second, ++first) {
            if (*second == *first) return first;
        }
        return last;
    }

    template <typename Forward_Iterator, typename BinaryPred>
    constexpr Forward_Iterator adjacent_find(Forward_Iterator first, Forward_Iterator last, BinaryPred b_pred) {
        if (first == last) return first;
        Forward_Iterator second = first;
        ++second;
        for (; second != last; ++second, ++first) {
            if (b_pred(*first, *second)) return first;
        }
        return last;
    }

    // *******************************************************************
    // search
    template <typename Forward_Iterator1, typename Forward_Iterator2>
    constexpr Forward_Iterator1 search(Forward_Iterator1 first, Forward_Iterator1 last,
                                Forward_Iterator2 s_first, Forward_Iterator2 s_last) {
        if (s_first == s_last) return first;
        while (true) {
            Forward_Iterator1 it = first;
            for (Forward_Iterator2 s_it = s_first; ; ++it, ++s_it) {
                if (it == last) return last;
                if (s_it == s_last) return first;
                if (!(*it == *s_it)) break;
            }
        }
    }

    template <typename Forward_Iterator1, typename Forward_Iterator2, typename BinaryPred>
    constexpr Forward_Iterator1 search(Forward_Iterator1 first, Forward_Iterator1 last,
                                Forward_Iterator2 s_first, Forward_Iterator2 s_last, BinaryPred b_pred) {
        if (s_first == s_last) return first;
        while (true) {
            Forward_Iterator1 it = first;
            for (Forward_Iterator2 s_it = s_first; ; ++it, ++s_it) {
                if (it == last) return last;
                if (s_it == s_last) return first;
                if (!b_pred(*it, *s_it)) break;
            }
        }
    }

    // search_n
    template <typename Forward_Iterator, typename Size, typename T = iter_value_type<Forward_Iterator>>
    constexpr Forward_Iterator search_n(Forward_Iterator first, Forward_Iterator last, Size count, const T& value) {
        if (count <= 0) return first;
        if (count == 1) return find(first, last, value);
        for (; first != last; ++first) {
            if (!(*first == value)) continue;
            Forward_Iterator it = first;
            Size i = 0;
            for (; i < count; ++i) {
                ++first;
                if (first == last) return last;
                if (!(*first == value)) break;
            }
            if (i == count) return it;
        }
        return last;
    }

    template <typename Forward_Iterator, typename Size, typename T = iter_value_type<Forward_Iterator>, typename BinaryPred>
    constexpr Forward_Iterator search_n(Forward_Iterator first, Forward_Iterator last,
                                        Size count, const T& value, BinaryPred b_pred) {
        if (count <= 0) return first;
        for (; first != last; ++first) {
            if (!b_pred(*first, value)) continue;
            Forward_Iterator it = first;
            Size i = 0;
            for (; i < count; ++i) {
                ++first;
                if (first == last) return last;
                if (!b_pred(*first, value)) break;
            }
            if (i == count) return it;
        }
        return last;
    }

    // *******************************************************************
    // find_end
    template <typename Forward_Iterator1, typename Forward_Iterator2>
    constexpr Forward_Iterator1 find_end(Forward_Iterator1 first, Forward_Iterator1 last,
                                        Forward_Iterator2 s_first, Forward_Iterator2 s_last) {
        if (s_first == s_last) return last;
        Forward_Iterator1 result = last;
        while (true) {
            Forward_Iterator1 new_result = search(first, last, s_first, s_last);
            if (new_result == last) break;
            result = new_result;
            first = new_result;
            ++first;
        }
        return result;
    }

    template <typename Forward_Iterator1, typename Forward_Iterator2, typename BinaryPred>
    constexpr Forward_Iterator1 find_end(Forward_Iterator1 first, Forward_Iterator1 last,
                                        Forward_Iterator2 s_first, Forward_Iterator2 s_last, BinaryPred pred) {
        if (s_first == s_last) return last;
        Forward_Iterator1 result = last;
        while (true) {
            Forward_Iterator1 new_result = search(first, last, s_first, s_last, pred);
            if (new_result == last) break;
            result = new_result;
            first = new_result;
            ++first;
        }
        return result;
    }

    // **********************************************************************************
    // copy
    // input iterator
    template <typename Input_Iterator, typename Output_Iterator>
    constexpr Output_Iterator unchecked_copy_cat
    (Input_Iterator first, Input_Iterator last, Output_Iterator result, input_iterator_tag) {
        while (first != last) {
            *result = *first;
            ++result; ++first;
        }
        return result;
    }
    // random access iterator
    template <typename Random_Iterator, typename Output_Iterator>
    constexpr Output_Iterator unchecked_copy_cat
    (Random_Iterator first, Random_Iterator last, Output_Iterator result, random_access_iterator_tag) {
        for (auto diff = last-first; diff>0; --diff, ++first, ++result) {
            *result = *first;
        }
        return result;
    }

    template <typename Iterator, typename Output_Iterator>
    constexpr Output_Iterator unchecked_copy(Iterator first, Iterator last, Output_Iterator result) {
        return mystl::unchecked_copy_cat(first, last, result, iter_category<Iterator>{});
    }

    // specialization for trivially_copy_assignable trait
    template <typename Tp, typename Up>
    constexpr enable_if_t<is_same_v<remove_cv_t<Tp>, Up> && is_trivially_copy_assignable_v<Up>, Up*>
    unchecked_copy(Tp* first, Tp* last, Up* result) {
        const auto n = static_cast<size_t>(last - first);
        if (n!=0) mystl::memcpy(result, first, n*sizeof(Up));
        return result+n;
    }

    template <typename Input, typename Output>
    constexpr Output copy(Input first, Input last, Output result) {
        return mystl::unchecked_copy(first, last, result);
    }

    // copy_if
    template <typename Input_Iterator, typename Output_Iterator, class UnaryPred>
    constexpr Output_Iterator copy_if(Input_Iterator first, Input_Iterator last, Output_Iterator result, UnaryPred up) {
        while (first != last) {
            if (up(*first)) *result++ = *first;
            ++first;
        }
        return result;
    }

    // **********************************************************************************
    // copy_backward
    // bidirectional iterator
    template <typename Bidirectional_Iterator1, typename Bidirectional_Iterator2>
    constexpr Bidirectional_Iterator2 unchecked_copy_backward_cat(Bidirectional_Iterator1 first, Bidirectional_Iterator1 last,
                                                        Bidirectional_Iterator2 result, bidirectional_iterator_tag) {
        while (first != last) *--result = *--last;
        return result;
    }

    // random access iterator
    template <typename Random_Iterator, typename Bidirectional_Iterator>
    constexpr Bidirectional_Iterator unchecked_copy_backward_cat(Random_Iterator first, Random_Iterator last,
                                                        Bidirectional_Iterator result, random_access_iterator_tag) {
        for (auto n=last-first; n>0; --n) *--result = *--last;
        return result;
    }

    template <typename Iterator, typename Bidirectional_Iterator>
    constexpr Bidirectional_Iterator unchecked_copy_backward(Iterator first, Iterator last, Bidirectional_Iterator result) {
        return mystl::unchecked_copy_backward_cat(first, last, result, iter_category<Iterator>{});
    }

    // specialization for trivially_copy_assignable trait
    // ...

    template <typename Input, typename Output>
    constexpr Output copy_backward(Input first, Input last, Output result) {
        return mystl::unchecked_copy_backward(first, last, result);
    }

    // **********************************************************************************
    // copy_n
    // input iterator
    template <typename Input_Iterator, typename Size, typename Output_Iterator>
    constexpr pair<Input_Iterator, Output_Iterator>
    unchecked_copy_n(Input_Iterator first, Size n, Output_Iterator result, input_iterator_tag) {
        while (n>0) {*result = *first; ++first; ++result; --n;}
        return mystl::make_pair(first, result);
    }

    // random access iterator
    template <typename Random_Iterator, typename Size, typename Output_Iterator>
    constexpr pair<Random_Iterator, Output_Iterator>
    unchecked_copy_n(Random_Iterator first, Size n, Output_Iterator result, random_access_iterator_tag) {
        auto last = first + n;
        return mystl::make_pair(last, copy(first, last, result));
    }

    template <typename Iterator, typename Size, typename Output_Iterator>
    constexpr pair<Iterator, Output_Iterator> copy_n(Iterator first, Size n, Output_Iterator result) {
        return mystl::unchecked_copy_n(first, n, result, iter_category<Iterator>());
    }

    // **********************************************************************************
    // move
    // input iterator
    template <typename Input_Iterator, typename Output_Iterator>
    constexpr Output_Iterator unchecked_move_cat(Input_Iterator first, Input_Iterator last,
                                        Output_Iterator result, input_iterator_tag) {
        while (first != last) {*result = move(*first); ++first; ++result;}
        return result;
    }

    // random access iterator
    template <typename Random_Iterator, typename Output_Iterator>
    constexpr Output_Iterator unchecked_move_cat(Random_Iterator first, Random_Iterator last,
                                        Output_Iterator result, random_access_iterator_tag) {
        for (auto n=last-first; n>0; --n, ++first, ++result) *result = mystl::move(*first);
        return result;
    }

    template <typename Iterator, typename Output_Iterator>
    constexpr Output_Iterator unchecked_move(Iterator first, Iterator last, Output_Iterator result) {
        return mystl::unchecked_move_cat(first, last, result, iter_category<Iterator>{});
    }

    // specialization for trivially_move_assignable trait
    template <typename Tp, typename Up>
    constexpr enable_if_t<is_same_v<remove_cv_t<Tp>, Up> && is_trivially_move_assignable_v<Up>, Up*>
    unchecked_move(Tp* first, Tp* last, Up* result) {
        const auto n = static_cast<size_t>(last - first);
        if (n!=0) mystl::memmove(result, first, n*sizeof(Up));
        return result+n;
    }

    template <typename Input, typename Output>
    constexpr Output move(Input first, Input last, Output result) {
        return mystl::unchecked_move(first, last, result);
    }

    // **********************************************************************************
    // move_backward
    // bidirectional iterator
    template <typename Bidirectional_Iterator, typename Output_Iterator>
    constexpr Output_Iterator unchecked_move_backward_cat(Bidirectional_Iterator first, Bidirectional_Iterator last,
                                                Output_Iterator result, bidirectional_iterator_tag) {
        while (first != last) *--result = mystl::move(*--last);
        return result;
    }

    // random access iterator
    template <typename Random_Iterator, typename Output_Iterator>
    constexpr Output_Iterator unchecked_move_backward_cat(Random_Iterator first, Random_Iterator last,
                                                Output_Iterator result, random_access_iterator_tag) {
        for (auto n = last - first; n > 0; --n) *--result = mystl::move(*--last);
        return result;
    }

    template <typename Iterator, typename Output_Iterator>
    constexpr Output_Iterator unchecked_move_backward(Iterator first, Iterator last, Output_Iterator result) {
        return mystl::unchecked_move_backward_cat(first, last, result, iter_category<Iterator>{});
    }

    // specialization for trivially_move_assignable trait
    // ...

    template <typename Input, typename Output>
    constexpr Output move_backward(Input first, Input last, Output result) {
        return mystl::unchecked_move_backward(first, last, result);
    }

    // **********************************************************************************
    // fill_n
    template <typename Output_Iterator, typename Size, typename T>
    constexpr Output_Iterator unchecked_fill_n(Output_Iterator first, Size n, const T& value) {
        while (n>0) {*first = value; ++first; --n;}
        return first;
    }

    // specialization for 1-Byte trait
    template <typename Tp, typename Size, typename Up>
    constexpr enable_if_t<is_integral_v<Tp> && sizeof(Tp) == 1 && !is_same_v<Tp, bool> && is_integral_v<Up> && sizeof(Up) == 1, Tp*>
    unchecked_fill_n(Tp* first, Size n, const Up& value) {
        if (n>0) mystl::memset(first, static_cast<unsigned char>(value), static_cast<size_t>(n));
        return first+n;
    }

    template <typename Output, typename Size, typename T>
    constexpr Output fill_n(Output first, Size n, const T& value) {
        return mystl::unchecked_fill_n(first, n, value);
    }

    // **********************************************************************************
    // fill
    // forward iterator
    template <typename Forward_Iterator, typename T>
    constexpr void fill_cat(Forward_Iterator first, Forward_Iterator last, const T& value, forward_iterator_tag) {
        while (first != last) {*first = value; ++first;}
    }

    // random access iterator
    template <typename Random_Iterator, typename T>
    constexpr void fill_cat(Random_Iterator first, Random_Iterator last, const T& value, random_access_iterator_tag) {
        mystl::fill_n(first, last-first, value);
    }

    template <typename Iterator, typename T>
    constexpr void fill(Iterator first, Iterator last, const T& value) {
        return mystl::fill_cat(first, last, value, iter_category<Iterator>{});
    }

    // **********************************************************************************
    // transform
    template <typename Input_Iterator, typename Output_Iterator, typename UnaryOp>
    constexpr Output_Iterator transform(Input_Iterator first, Input_Iterator last, Output_Iterator result, UnaryOp op) {
        for (; first != last; ++first, ++result) *result = op(*first);
        return result;
    }

    template <typename Input_Iterator1, typename Input_Iterator2, typename Output_Iterator, typename BinaryOp>
    constexpr Output_Iterator transform(Input_Iterator1 first1, Input_Iterator1 last1, Input_Iterator2 first2,
                                        Output_Iterator result, BinaryOp op) {
        for (; first1 != last1; ++first1, ++result, ++first2) *result = op(*first1, *first2);
        return result;
    }

    // **********************************************************************************
    // generate && generate_n
    template <typename Forward_Iterator, typename Generator>
    constexpr void generate(Forward_Iterator first, Forward_Iterator last, Generator gen) {
        for (; first != last; ++first) *first = gen();
    }

    template <typename Forward_Iterator, typename Size, typename Generator>
    constexpr void generate_n(Forward_Iterator first, Size n, Generator gen) {
        for (; n>0; --n, ++first) *first = gen();
    }

    // **********************************************************************************
    // remove
    template <typename Forward_Iterator, typename T = iter_value_type<Forward_Iterator>>
    constexpr Forward_Iterator remove(Forward_Iterator first, Forward_Iterator last, const T& value) {
        first = mystl::find(first, last, value);
        if (first != last)
            for (Forward_Iterator it = first; ++it != last;)
                if (!(*it == value)) {
                    *first = move(*it);
                    ++first;
                }
        return first;
    }

    // remove_if
    template <typename Forward_Iterator, typename UnaryPred>
    constexpr Forward_Iterator remove_if(Forward_Iterator first, Forward_Iterator last, UnaryPred pred) {
        first = mystl::find_if(first, last, pred);
        if (first != last)
            for (Forward_Iterator it = first; ++it != last;)
                if (!pred(*it)) {
                    *first = move(*it);
                    ++first;
                }
        return first;
    }

    // **********************************************************************************
    // remove_copy
    template <typename Input_Iterator, typename Output_Iterator, typename T = iter_value_type<Input_Iterator>>
    constexpr Output_Iterator remove_copy(Input_Iterator first, Input_Iterator last,
                                            Output_Iterator result, const T& value) {
        for (; first != last; ++first)
            if (!(*first == value)) {
                *result = *first;
                ++result;
            }
        return result;
    }

    // remove_copy_if
    template <typename Input_Iterator, typename Output_Iterator, typename UnaryPred>
    constexpr Output_Iterator remove_copy(Input_Iterator first, Input_Iterator last,
                                            Output_Iterator result, UnaryPred pred) {
        for (; first != last; ++first)
            if (!pred(*first)) {
                *result = *first;
                ++result;
            }
        return result;
    }

    // **********************************************************************************
    // replace
    template <typename Forward_Iterator, typename T = iter_value_type<Forward_Iterator>>
    constexpr void replace(Forward_Iterator first, Forward_Iterator last, const T& old_value, const T& new_value) {
        for (; first != last; ++first) {
            if (*first == old_value) *first = new_value;
        }
    }

    // replace_if
    template <typename Forward_Iterator, typename UnaryPred, typename T = iter_value_type<Forward_Iterator>>
    constexpr void replace_if(Forward_Iterator first, Forward_Iterator last, UnaryPred pred, const T& new_value) {
        for (; first != last; ++first) {
            if (pred(*first)) *first = new_value;
        }
    }

    // **********************************************************************************
    // replace_copy
    template <typename Input_Iterator, typename Output_Iterator, typename T>
    constexpr Output_Iterator replace_copy(Input_Iterator first, Input_Iterator last, Output_Iterator result,
                                            const T& old_value, const T& new_value) {
        for (; first != last; ++first) {
            *result = (*first == old_value) ? new_value : *first;
            ++result;
        }
        return result;
    }

    // replace_copy_if
    template <typename Input_Iterator, typename Output_Iterator, typename UnaryPred,
            typename T = iter_value_type<Input_Iterator>>
    constexpr Output_Iterator replace_copy_if(Input_Iterator first, Input_Iterator last, Output_Iterator result,
                                            UnaryPred pred, const T& new_value) {
        for (; first != last; ++first) {
            *result = pred(*first) ? new_value : *first;
            ++result;
        }
        return result;
    }

    // **********************************************************************************
    // swap_ranges
    template <typename Forward_Iterator1, typename Forward_Iterator2>
    constexpr Forward_Iterator2 swap_ranges(Forward_Iterator1 first1, Forward_Iterator1 last1,
                                            Forward_Iterator2 first2) {
        for (; first1 != last1; ++first1, ++first2) {
            mystl::swap(*first1, *first2);
        }
        return first2;
    }

    // iter_swap
    template <typename Iter1, typename Iter2>
    constexpr void iter_swap(Iter1 lhs, Iter2 rhs) {
        mystl::swap(*lhs, *rhs);
    }

    // **********************************************************************************
    // reverse
    template <typename Bidirectional_Iterator>
    constexpr void reverse_impl(Bidirectional_Iterator first, Bidirectional_Iterator last, bidirectional_iterator_tag) {
        while (first != last && first != --last) {
            mystl::swap(*first, *last);
            ++first;
        }
    }

    template <typename Random_Iterator>
    constexpr void reverse_impl(Random_Iterator first, Random_Iterator last, random_access_iterator_tag) {
        for (--last; first < last; ++first, --last) mystl::swap(*first, *last);
    }

    template <typename Iterator>
    constexpr void reverse(Iterator first, Iterator last) {
        mystl::reverse_impl(first, last, iter_category<Iterator>{});
    }

    // reverse_copy
    template <typename Bidirectional_Iterator, typename Output_Iterator>
    constexpr Output_Iterator reverse_copy(Bidirectional_Iterator first, Bidirectional_Iterator last,
                                            Output_Iterator result) {
        for (; first != last; ++result) *result = *--last;
        return result;
    }

    // **********************************************************************************
    // rotate
    template <typename Forward_Iterator>
    constexpr Forward_Iterator rotate(Forward_Iterator first, Forward_Iterator mid, Forward_Iterator last) {
        if (first == mid) return last;
        if (mid == last) return first;
        Forward_Iterator write = first;
        Forward_Iterator next_mid = first;
        for (Forward_Iterator read = mid; read != last; ++read, ++write) {
            if (write == next_mid) next_mid = read;
            mystl::swap(*write, *read);
        }
        mystl::rotate(write, next_mid, last);
        return write;
    }

    // rotate_copy
    template <typename Forward_Iterator, typename Output_Iterator>
    constexpr Output_Iterator rotate_copy(Forward_Iterator first, Forward_Iterator mid,
                                            Forward_Iterator last, Output_Iterator result) {
        result = mystl::copy(mid, last, result);
        return mystl::copy(first, mid, result);
    }

    // **********************************************************************************
    // shift_left
    template <typename Forward_Iterator>
    constexpr Forward_Iterator shift_left(Forward_Iterator first, Forward_Iterator last,
                                            iter_difference_type<Forward_Iterator> n) {
        if (n == 0 || first == last) return last;
        Forward_Iterator it = first;
        while (n-- > 0) {
            ++it;
            if (it == last) return first;
        }
        for (; it != last; ++it, ++first) *first = mystl::move(*it);
        return first;
    }

    // shift_right
    template <typename Forward_Iterator>
    constexpr Forward_Iterator shift_right(Forward_Iterator first, Forward_Iterator last,
                                            iter_difference_type<Forward_Iterator> n) {
        if (n == 0 || first == last) return first;
        using diff_type = iter_difference_type<Forward_Iterator>;
        diff_type len = mystl::distance(first, last);
        if (!(n < len)) return last;
        Forward_Iterator mid = first;
        mystl::advance(mid, len - n);
        return mystl::rotate(first, mid, last);
    }

    // **********************************************************************************
    // random_shuffle

    // shuffle

    // **********************************************************************************
    // sample

    // **********************************************************************************
    // unique
    template <typename Forward_Iterator>
    constexpr Forward_Iterator unique(Forward_Iterator first, Forward_Iterator last) {
        if (first == last) return last;
        Forward_Iterator it = first;
        while (++it != last) {
            if (!(*it == *first) && ++first != it) *first = mystl::move(*it);
        }
        return first;
    }

    template <typename Forward_Iterator, typename BinaryPred>
    constexpr Forward_Iterator unique(Forward_Iterator first, Forward_Iterator last, BinaryPred b_pred) {
        if (first == last) return last;
        Forward_Iterator it = first;
        while (++it != last) {
            if (!b_pred(*it, *first) && ++first != it) *first = mystl::move(*it);
        }
        return first;
    }

    // unique_copy
    template <typename Input_Iterator, typename Output_Iterator>
    constexpr Output_Iterator unique_copy(Input_Iterator first, Input_Iterator last, Output_Iterator result) {
        if (first == last) return result;
        Input_Iterator it = first;
        *result = mystl::move(*first);
        while (++it != last) {
            if (!(*it == *first)) {
                *++result = mystl::move(*it);
                first = it;
            }
        }
        return ++result;
    }

    template <typename Input_Iterator, typename Output_Iterator, typename BinaryPred>
    constexpr Output_Iterator unique_copy(Input_Iterator first, Input_Iterator last,
                                            Output_Iterator result, BinaryPred b_pred) {
        if (first == last) return result;
        Input_Iterator it = first;
        *result = mystl::move(*first);
        while (++it != last) {
            if (!b_pred(*it, *first)) {
                *++result = mystl::move(*it);
                first = it;
            }
        }
        return ++result;
    }

    // **********************************************************************************
    // is_permutation
    template <typename Forward_Iterator1, typename Forward_Iterator2>
    constexpr bool is_permutation(Forward_Iterator1 first1, Forward_Iterator1 last1, Forward_Iterator2 first2) {
        while (first1 != last1 && *first1 == *first2) {
            ++first1;
            ++first2;
        }
        if (first1 == last1) return true;

        auto last2 = first2;
        for (auto it = first1; it != last1; ++it) ++last2;
        for (auto it = first1; it != last1; ++it) {
            auto previous = first1;
            while (previous != it && !(*previous == *it)) ++previous;
            if (previous != it) continue;

            std::size_t count1 = 0, count2 = 0;
            for (auto current = it; current != last1; ++current)
                if (*current == *it) ++count1;
            for (auto current = first2; current != last2; ++current)
                if (*current == *it) ++count2;
            if (count1 != count2) return false;
        }
        return true;
    }

} // namespace mystl

#endif //ALGOBASE_H
