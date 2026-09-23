#pragma once

#ifndef ALGO_PARTITION_H
#define ALGO_PARTITION_H

#include "../iterator.h"
#include "../utility.h"
#include "algobase.h"

namespace mystl {
    // is_partitioned
    template <typename Input_Iterator, typename UnaryPred>
    constexpr bool is_partitioned(Input_Iterator first, Input_Iterator last, UnaryPred pred) {
        while (first != last && pred(*first)) ++first;
        while (first != last && !pred(*first)) ++first;
        return first == last;
    }

    // partition
    template <typename Forward_Iterator, typename UnaryPred>
    constexpr Forward_Iterator partition(Forward_Iterator first, Forward_Iterator last, UnaryPred pred) {
        first = find_if_not(first, last, pred);
        if (first == last) return last;
        for (Forward_Iterator it = next(first); it != last; ++it) {
            if (pred(*it)) {
                iter_swap(it, first);;
                ++first;
            }
        }
        return first;
    }

    // partition_copy
    template <typename Input_Iterator, typename Output_Iterator1, typename Output_Iterator2, typename UnaryPred>
    constexpr pair<Output_Iterator1, Output_Iterator2> partition_copy(Input_Iterator first, Input_Iterator last,
                                Output_Iterator1 result_true, Output_Iterator2 result_false, UnaryPred pred) {
        for (; first != last; ++first) {
            if (pred(*first)) {
                *result_true = *first;
                ++result_true;
            }else {
                *result_false = *first;
                ++result_false;
            }
        }
        return make_pair(result_true, result_false);
    }

    // stable_partition


    // partition_point
    template <typename Forward_Iterator, typename UnaryPred>
    constexpr Forward_Iterator partition_point_impl(Forward_Iterator first, Forward_Iterator last, UnaryPred pred,
                                                    forward_iterator_tag) {
        while (first != last && pred(*first)) ++first;
        return first;
    }

    template <typename Random_Iterator, typename UnaryPred>
    constexpr Random_Iterator partition_point_impl(Random_Iterator first, Random_Iterator last, UnaryPred pred,
                                                    random_access_iterator_tag) {
        for (auto dist = distance(first, last); dist > 0; ) {
            auto half = dist >> 1;
            Random_Iterator mid = next(first, half);
            if (pred(*mid)) {
                first = next(mid);
                dist -= half + 1;
            }else {
                dist = half;
            }
        }
        return first;
    }

    template <typename Iterator, typename UnaryPred>
    constexpr Iterator partition_point(Iterator first, Iterator last, UnaryPred pred) {
        return partition_point_impl(first, last, pred, iter_category<Iterator>{});
    }


} // namespace mystl

#endif //ALGO_PARTITION_H
