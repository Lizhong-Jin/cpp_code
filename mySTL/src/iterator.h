#pragma once

#ifndef ITERATOR_H
#define ITERATOR_H

#include "utility.h"

namespace mystl {
    // *******************************************************************
    // iterator categories
    struct input_iterator_tag {};
    struct output_iterator_tag {};
    struct forward_iterator_tag : public input_iterator_tag {};
    struct bidirectional_iterator_tag : public forward_iterator_tag {};
    struct random_access_iterator_tag : public bidirectional_iterator_tag {};
    struct contiguous_iterator_tag : public random_access_iterator_tag {};

    // template of iterator
    template <typename Category, typename T, typename Distance=ptrdiff_t, typename Pointer=T*, typename Reference=T&>
    struct iterator {
        typedef Category    iterator_category;
        typedef T           value_type;
        typedef Distance    difference_type;
        typedef Pointer     pointer;
        typedef Reference   reference;
    };

    // *******************************************************************
    // iterator traits
    // has_iterator_cat
    template <typename T, typename = void>
    struct has_iterator_cat : false_type {};
    template <typename T>
    struct has_iterator_cat<T, void_t<typename remove_cv_ref_t<T>::iterator_category>> : true_type {};
    template <typename T>
    static constexpr bool has_iterator_cat_v = has_iterator_cat<T>::value;

    // iterator_traits_base
    template <typename T, bool = has_iterator_cat<T>::value>
    struct iterator_traits_base {};
    template <typename T>
    struct iterator_traits_base<T, true> {
        using U = remove_cv_ref_t<T>;

        using iterator_category = typename U::iterator_category;
        using value_type        = typename U::value_type;
        using difference_type   = typename U::difference_type;
        using pointer           = typename U::pointer;
        using reference         = typename U::reference;
    };

    // iterator_traits
    template <typename T>
    struct iterator_traits : iterator_traits_base<T> {};

    // native pointer specialization
    template <typename T>
    struct iterator_traits<T*> {
        static_assert(is_object_v<T>, "iterator_traits<T*> requires T to be an object type");

        using iterator_category = contiguous_iterator_tag;
        using value_type        = remove_cv_t<T>;
        using difference_type   = std::ptrdiff_t;
        using pointer           = T*;
        using reference         = T&;
    };

    template <typename Iterator> using iter_category        = iterator_traits<Iterator>::iterator_category;
    template <typename Iterator> using iter_value_type      = iterator_traits<Iterator>::value_type;
    template <typename Iterator> using iter_difference_type = iterator_traits<Iterator>::difference_type;
    template <typename Iterator> using iter_pointer         = iterator_traits<Iterator>::pointer;
    template <typename Iterator> using iter_reference       = iterator_traits<Iterator>::reference;

    // is_iterator_of
    template <typename T, typename U, bool = has_iterator_cat_v<iterator_traits<T>>>
    struct is_iterator_of : bool_constant<is_convertible_v<iter_category<T>, U>> {};
    template <typename T, typename U>
    struct is_iterator_of<T, U, false> : false_type {};
    template <typename T, typename U>
    static constexpr bool is_iterator_of_v = is_iterator_of<T, U>::value;

    // *******************************************************************
    // traits specific iterator
    template <typename Iterator>
    struct is_exactly_input_iterator : bool_constant<is_iterator_of_v<Iterator, input_iterator_tag> &&
                                                    !is_iterator_of_v<Iterator, forward_iterator_tag>> {};
    template <typename Iterator>
    struct is_input_iterator : is_iterator_of<Iterator, input_iterator_tag> {};
    template <typename Iterator>
    struct is_output_iterator : is_iterator_of<Iterator, output_iterator_tag> {};
    template <typename Iterator>
    struct is_forward_iterator : is_iterator_of<Iterator, forward_iterator_tag> {};
    template <typename Iterator>
    struct is_bidirectional_iterator : is_iterator_of<Iterator, bidirectional_iterator_tag> {};
    template <typename Iterator>
    struct is_random_access_iterator : is_iterator_of<Iterator, random_access_iterator_tag> {};
    template <typename Iterator>
    struct is_contiguous_iterator : is_iterator_of<Iterator, contiguous_iterator_tag> {};

    template <typename Iterator> static constexpr bool is_exactly_input_iterator_v  = is_exactly_input_iterator<Iterator>::value;
    template <typename Iterator> static constexpr bool is_input_iterator_v          = is_input_iterator<Iterator>::value;
    template <typename Iterator> static constexpr bool is_output_iterator_v         = is_output_iterator<Iterator>::value;
    template <typename Iterator> static constexpr bool is_forward_iterator_v        = is_forward_iterator<Iterator>::value;
    template <typename Iterator> static constexpr bool is_bidirectional_iterator_v  = is_bidirectional_iterator<Iterator>::value;
    template <typename Iterator> static constexpr bool is_random_access_iterator_v  = is_random_access_iterator<Iterator>::value;
    template <typename Iterator> static constexpr bool is_contiguous_iterator_v     = is_contiguous_iterator<Iterator>::value;


    template <typename Iterator>
    struct is_iterator : bool_constant<is_input_iterator_v<Iterator> || is_output_iterator_v<Iterator>> {};
    template <typename Iterator> static constexpr bool is_iterator_v = is_iterator<Iterator>::value;

    // *******************************************************************
    // calculate distance
    // distance between input iterator
    template <typename Input_Iterator>
    iter_difference_type<Input_Iterator> distance_dispatch(Input_Iterator first, Input_Iterator last, input_iterator_tag) {
        iter_difference_type<Input_Iterator> diff = 0;
        while (first != last) {
            ++first;
            ++diff;
        }
        return diff;
    }
    // distance between random access iterator
    template <typename Random_Iterator>
    iter_difference_type<Random_Iterator> distance_dispatch(Random_Iterator first, Random_Iterator last, random_access_iterator_tag) {
        return last - first;
    }

    template <typename Iterator>
    iter_difference_type<Iterator> distance(Iterator first, Iterator last) {
        return distance_dispatch(first, last, iter_category<Iterator>{});
    }

    // advance
    // input iterator
    template <typename Input_Iterator, typename Distance>
    constexpr void advance_dispatch(Input_Iterator& iter, Distance n, input_iterator_tag) {
        while (n--) ++iter;
    }
    // bidirectional iterator
    template <typename Bidirectional_Iterator, typename Distance>
    constexpr void advance_dispatch(Bidirectional_Iterator& iter, Distance n, bidirectional_iterator_tag) {
        if (n>=0) while (n--) ++iter;
        else while (n++) --iter;
    }
    // random access iterator
    template <typename Random_Iterator, typename Distance>
    constexpr void advance_dispatch(Random_Iterator& iter, Distance n, random_access_iterator_tag) {
        iter+=n;
    }

    template <typename Iterator, typename Distance>
    constexpr void advance(Iterator& iter, Distance n) {
        advance_dispatch(iter, n, iter_category<Iterator>{});
    }

    // next
    template <typename Input_Iterator>
    constexpr Input_Iterator next(Input_Iterator iter, iter_difference_type<Input_Iterator> n = 1) {
        advance(iter, n);
        return iter;
    }

    // *******************************************************************
    // reverse iterator
    template <typename Iterator>
    class reverse_iterator {
    private:
        Iterator current;
    public:
        using iter_category = iter_category<Iterator>;
        using value_type = iter_value_type<Iterator>;
        using difference_type = iter_difference_type<Iterator>;
        using reference = iter_reference<Iterator>;
        using pointer = iter_pointer<Iterator>;

        using iterator_type = Iterator;
        using self = reverse_iterator<Iterator>;

        // constructor
        reverse_iterator() = default;
        explicit reverse_iterator(iterator_type it) : current(it) {}
        reverse_iterator(const self& rhs) : current(rhs.current) {}

        // retrieve current
        iterator_type base() const { return current; }

        // overload operator
        reference operator*() const {
            auto tmp = current;
            return *--tmp;
        }
        pointer operator->() const {
            return &operator*();
        }
        self& operator++() {
            --current;
            return *this;
        }
        self operator++(int) {
            self tmp = *this;
            --current;
            return tmp;
        }
        self& operator--() {
            ++current;
            return *this;
        }
        self operator--(int) {
            self tmp = *this;
            ++current;
            return tmp;
        }
        self& operator+=(difference_type n) {
            current -= n;
            return *this;
        }
        self operator+(difference_type n) const {
            return self(current-n);
        }
        self& operator-=(difference_type n) {
            current += n;
            return *this;
        }
        self operator-(difference_type n) const {
            return self(current+n);
        }
        reference operator[](difference_type n) const {
            return *(*this+n);
        }
    }; // reverse iterator

    // overload operator
    template <typename Rev_Iterator>
    reverse_iterator<Rev_Iterator>::difference_type
    operator-(const reverse_iterator<Rev_Iterator>& lhs, const reverse_iterator<Rev_Iterator>& rhs) {
        return rhs.base() - lhs.base();
    }
    template <typename Rev_Iterator>
    bool operator==(const reverse_iterator<Rev_Iterator>& lhs, const reverse_iterator<Rev_Iterator>& rhs) {
        return lhs.base() == rhs.base();
    }
    template <typename Rev_Iterator>
    bool operator!=(const reverse_iterator<Rev_Iterator>& lhs, const reverse_iterator<Rev_Iterator>& rhs) {
        return lhs.base() != rhs.base();
    }
    template <typename Rev_Iterator>
    bool operator>(const reverse_iterator<Rev_Iterator>& lhs, const reverse_iterator<Rev_Iterator>& rhs) {
        return lhs.base() < rhs.base();
    }
    template <typename Rev_Iterator>
    bool operator<(const reverse_iterator<Rev_Iterator>& lhs, const reverse_iterator<Rev_Iterator>& rhs) {
        return lhs.base() > rhs.base();
    }
    template <typename Rev_Iterator>
    bool operator>=(const reverse_iterator<Rev_Iterator>& lhs, const reverse_iterator<Rev_Iterator>& rhs) {
        return lhs.base() <= rhs.base();
    }
    template <typename Rev_Iterator>
    bool operator<=(const reverse_iterator<Rev_Iterator>& lhs, const reverse_iterator<Rev_Iterator>& rhs) {
        return lhs.base() >= rhs.base();
    }

} // namespace mystl

#endif //ITERATOR_H
