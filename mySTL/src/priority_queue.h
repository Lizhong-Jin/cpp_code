#pragma once

#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <limits>
#include <cassert>

#include "iterator.h"
#include "memory.h"
#include "utility.h"
#include "stdexcept.h"
#include "algorithm.h"
#include "initializer_list.h"
#include "vector.h"

namespace mystl {
    template <typename T, typename Container = mystl::vector<T>, 
            typename Compare = mystl::less<typename Container::value_type>>
    class priority_queue {

        static_assert(is_same_v<remove_cv_t<T>, T>,
                    "priority_queue must have a non_const and non_volatile value type");
        static_assert(is_same_v<T, typename Container::value_type>,
              "T must match Container::value_type");

    public:
        using container_type = Container;
        using value_compare = Compare;
        using value_type = typename Container::value_type;
        using size_type = typename Container::size_type;
        using reference = typename Container::reference;
        using const_reference = typename Container::const_reference;

    protected:
        container_type c;
        Compare comp;

    public:
        // *************************************************************************************
        // construct
        constexpr priority_queue(): priority_queue(Compare(), Container()) {}

        explicit constexpr priority_queue(const Compare& compare): priority_queue(compare, Container()) {}

        constexpr priority_queue(const Compare& compare, const Container& cont): c(cont), comp(compare) {
            mystl::make_heap(c.begin(), c.end(), comp);
        }

        constexpr priority_queue(const Compare& compare, Container&& cont): c(mystl::move(cont)), comp(compare) {
            mystl::make_heap(c.begin(), c.end(), comp);
        }

        constexpr priority_queue(const priority_queue& other): c(other.c), comp(other.comp) {}

        constexpr priority_queue(priority_queue&& other) 
            noexcept(is_nothrow_move_constructible_v<Container> && is_nothrow_move_constructible_v<Compare>)
            : c(mystl::move(other.c)), comp(mystl::move(other.comp)) {}

        template <typename Input_Iterator>
        requires mystl::is_input_iterator_v<Input_Iterator>
        constexpr priority_queue(Input_Iterator first, Input_Iterator last, const Compare& compare = Compare())
            : priority_queue(first, last, compare, Container()) {}

        template <typename Input_Iterator>
        requires mystl::is_input_iterator_v<Input_Iterator>
        constexpr priority_queue(Input_Iterator first, Input_Iterator last, const Compare& compare, const Container& cont)
            : c(cont), comp(compare) {
            c.insert(c.end(), first, last);
            mystl::make_heap(c.begin(), c.end(), comp);
        }

        template <typename Input_Iterator>
        requires mystl::is_input_iterator_v<Input_Iterator>
        constexpr priority_queue(Input_Iterator first, Input_Iterator last, const Compare& compare, Container&& cont)
            : c(mystl::move(cont)), comp(compare) {
            c.insert(c.end(), first, last);
            mystl::make_heap(c.begin(), c.end(), comp);
        }

        template <typename Allocator>
            requires mystl::uses_allocator_v<Container, Allocator>
        explicit constexpr priority_queue(const Allocator& alloc): priority_queue(Compare(), Container(alloc)) {}

        template <typename Allocator>
            requires mystl::uses_allocator_v<Container, Allocator>
        constexpr priority_queue(const Compare& compare, const Allocator& alloc): priority_queue(compare, Container(alloc)) {}

        template <typename Allocator>
            requires mystl::uses_allocator_v<Container, Allocator>
        constexpr priority_queue(const Compare& compare, const Container& cont, const Allocator& alloc)
            : c(cont, alloc), comp(compare) {
            mystl::make_heap(c.begin(), c.end(), comp);
        }

        template <typename Allocator>
            requires mystl::uses_allocator_v<Container, Allocator>
        constexpr priority_queue(const Compare& compare, Container&& cont, const Allocator& alloc)
            : c(mystl::move(cont), alloc), comp(compare) {
            mystl::make_heap(c.begin(), c.end(), comp);
        }

        template <typename Allocator>
            requires mystl::uses_allocator_v<Container, Allocator>
        constexpr priority_queue(const priority_queue& other, const Allocator& alloc)
            : c(other.c, alloc), comp(other.comp) {}

        template <typename Allocator>
            requires mystl::uses_allocator_v<Container, Allocator>
        constexpr priority_queue(priority_queue&& other, const Allocator& alloc)
            : c(mystl::move(other.c), alloc), comp(mystl::move(other.comp)) {}

        template <typename Input_Iterator, typename Allocator>
            requires (mystl::uses_allocator_v<Container, Allocator> &&
                    mystl::is_input_iterator_v<Input_Iterator>)
        constexpr priority_queue(Input_Iterator first, Input_Iterator last, const Allocator& alloc)
            : priority_queue(first, last, Compare(), Container(alloc)) {}

        template <typename Input_Iterator, typename Allocator>
            requires (mystl::uses_allocator_v<Container, Allocator> &&
                    mystl::is_input_iterator_v<Input_Iterator>)
        constexpr priority_queue(Input_Iterator first, Input_Iterator last, const Compare& compare, const Allocator& alloc)
            : priority_queue(first, last, compare, Container(alloc)) {}

        template <typename Input_Iterator, typename Allocator>
            requires (mystl::uses_allocator_v<Container, Allocator> &&
                    mystl::is_input_iterator_v<Input_Iterator>)
        constexpr priority_queue(Input_Iterator first, Input_Iterator last, const Compare& compare, 
                                const Container& cont, const Allocator& alloc)
            : c(cont, alloc), comp(compare) {
            c.insert(c.end(), first, last);
            mystl::make_heap(c.begin(), c.end(), comp);
        }

        template <typename Input_Iterator, typename Allocator>
            requires (mystl::uses_allocator_v<Container, Allocator> &&
                    mystl::is_input_iterator_v<Input_Iterator>)
        constexpr priority_queue(Input_Iterator first, Input_Iterator last, const Compare& compare, 
                                Container&& cont, const Allocator& alloc)
            : c(mystl::move(cont), alloc), comp(compare) {
            c.insert(c.end(), first, last);
            mystl::make_heap(c.begin(), c.end(), comp);
        }

        // *************************************************************************************
        // destruct
        constexpr ~priority_queue() = default;

        // *************************************************************************************
        // operator = and assign
        constexpr priority_queue& operator=(const priority_queue& other) {
            if (this != &other) {
                c = other.c;
                comp = other.comp;
            }
            return *this;
        }

        constexpr priority_queue& operator=(priority_queue&& other) 
            noexcept(is_nothrow_move_assignable_v<Container> && is_nothrow_move_assignable_v<Compare>) {
            if (this != &other) {
                c = mystl::move(other.c);
                comp = mystl::move(other.comp);
            }
            return *this;
        }

        // *************************************************************************************
        // top, empty, size
        constexpr const_reference top() const {
            assert(!empty());
            return c.front();
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return c.empty();
        }

        [[nodiscard]] constexpr size_type size() const noexcept {
            return c.size();
        }

        // *************************************************************************************
        // push, emplace, pop
        constexpr void push(const value_type& value) {
            c.push_back(value);
            mystl::push_heap(c.begin(), c.end(), comp);
        }

        constexpr void push(value_type&& value) {
            c.push_back(mystl::move(value));
            mystl::push_heap(c.begin(), c.end(), comp);
        }

        template <typename... Args>
        constexpr void emplace(Args&&... args) {
            c.emplace_back(mystl::forward<Args>(args)...);
            mystl::push_heap(c.begin(), c.end(), comp);
        }

        constexpr void pop() {
            MYSTL_DEBUG(!empty());
            mystl::pop_heap(c.begin(), c.end(), comp);
            c.pop_back();
        }

        // *************************************************************************************
        // swap
        using mystl::swap;
        constexpr void swap(priority_queue& other)  
            noexcept(noexcept(swap(c, other.c)) && noexcept(swap(comp, other.comp)))
        {
            if (this != &other) {
                swap(c, other.c);
                swap(comp, other.comp);
            }
        }

    }; // struct priority_queue

    template <typename T, typename Container, typename Compare>
    constexpr void swap(priority_queue<T, Container, Compare>& lhs,
                priority_queue<T, Container, Compare>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }
} // namespace mystl


#endif //PRIORITY_QUEUE_H