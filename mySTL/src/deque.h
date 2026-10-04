#pragma once

#ifndef DEQUE_H
#define DEQUE_H

#include <limits>
#include <cassert>

#include "iterator.h"
#include "memory.h"
#include "utility.h"
#include "stdexcept.h"


namespace mystl {

    // *************************************************************************************
    // deque block
    template <typename T, typename Allocator = mystl::allocator<T>>
    struct deque_block {
        using allocator_type = Allocator;
        using T_alloc_type = typename allocator_traits<Allocator>::template rebind_alloc<T>;
        using pointer = typename allocator_traits<T_alloc_type>::pointer;

        struct _block_data {
            pointer _begin;
            pointer _end;
            pointer _end_of_storage;

            constexpr _block_data() noexcept : _begin(), _end(), _end_of_storage() {}

            constexpr _block_data(_block_data&& other) noexcept
                : _begin(other._begin), _end(other._end), _end_of_storage(other._end_of_storage) 
            {
                other._begin = other._end = other._end_of_storage = pointer();
            }

            constexpr void _copy_data(const _block_data& other) noexcept {
                _begin = other._begin;
                _end = other._end;
                _end_of_storage = other._end_of_storage;
            }

            constexpr void _swap_data(_block_data& other) noexcept {
                _block_data temp;
                temp._copy_data(*this);
                _copy_data(other);
                other._copy_data(temp);
            }
        };

        struct _block_impl: T_alloc_type, _block_data {
            constexpr _block_impl() noexcept(is_nothrow_default_constructible_v<T_alloc_type>)
                requires is_default_constructible_v<T_alloc_type> : T_alloc_type() {}
            
            constexpr _block_impl(const T_alloc_type& alloc) noexcept: T_alloc_type(alloc) {}

            constexpr _block_impl(T_alloc_type&& alloc) noexcept: T_alloc_type(mystl::move(alloc)) {}

            constexpr _block_impl(_block_impl&& other) noexcept:
                : T_alloc_type(mystl::move(other)), _block_data(mystl::move(other)) {}

            constexpr _block_impl(T_alloc_type&& alloc, _block_data&& other) noexcept
                    : T_alloc_type(mystl::move(alloc)), _block_data(mystl::move(other)) {}
        }

        _block_impl Block_impl;

        constexpr pointer Block_allocate(size_t n) {
            using Traits = allocator_traits<T_alloc_type>;
            return n == 0 ? pointer() : Traits::allocate(Block_impl, n)
        }

        constexpr void Block_deallocate(pointer p, size_t n) {
            using Traits = allocator_traits<T_alloc_type>;
            if (p) Traits::deallocate(Block_impl, p, n);
        }

        // get allocator
        constexpr T_alloc_type& get_T_allocator() noexcept {
            return static_cast<T_alloc_type&>(this -> Block_impl);
        }

        constexpr const T_alloc_type& get_T_allocator() const noexcept {
            return static_cast<const T_alloc_type&>(this -> Block_impl);
        }

        constexpr allocator_type get_allocator() const noexcept {
            return allocator_type(get_T_allocator());
        }

        // constructor of deque_block
        constexpr deque_block() = default;

        constexpr deque_block(const allocator_type& alloc) noexcept : Block_impl(alloc) {}

        constexpr deque_block(const size_t n) : Block_impl() {Block_create_storage(n);}

        constexpr deque_block(const size_t n, const allocator_type& alloc) : Block_impl(alloc) {Block_create_storage(n);}

        constexpr deque_block(deque_block&& other) = default;

        constexpr deque_block(T_alloc_type&& alloc) noexcept : Block_impl(mystl::move(alloc)) {}

        constexpr deque_block(deque_block&& other, const allocator_type& alloc) : Block_impl(alloc) {
            if (other.get_allocator() == alloc) {
                this -> Block_impl._swap_data(other.M_impl);
            }else {
                const size_t n = other.Block_impl._end - other.Block_impl._begin;
                // only allocate the space here, and move the data form other to this in queue_base
                Block_create_storage(n);
            }
        }

        constexpr deque_block(const allocator_type& alloc, deque_block&& other)
                : Block_impl(T_alloc_type(alloc), mystl::move(other.Block_impl)) {}

        // destructor
        constexpr ~deque_block() noexcept {
            if (this -> Block_impl._begin) {
                const ptrdiff_t n = this->Block_impl._end_of_storage - this->Block_impl._begin;
                Block_deallocate(Block_impl._begin, static_cast<size_t>(n));
            }
        }
    
    protected:
        constexpr void Block_create_storage(size_t n) {
            this -> Block_impl._begin = this->Block_allocate(n);
            this -> Block_impl._end = this->Block_impl._begin;
            this -> Block_impl._end_of_storage = this->Block_impl._begin + n;
        }

    }; // struct deque_block

    // *************************************************************************************
    // deque base
    template <typename T, typename Allocator = mystl::allocator<T>>
    class deque_base {
        using allocator_type = Allocator;
        using T_alloc_type = typename allocator_traits<Allocator>::template rebind_alloc<T>;
        using Traits = mystl::allocator_traits<T_alloc_type>

        using Map_alloc_type = Traits::rebind_alloc<T*>
        using Map_Traits = mystl::allocator_traits<MapAlloc>;

        using pointer = typename allocator_traits<T_alloc_type>::pointer;

        


    }; // struct deque_base


    // *************************************************************************************
    // standard container deque
    template <typename T, typename Allocator = mystl::allocator<T>>
    class deque {

        static_assert(is_same_v<remove_cv_t<T>, T>,
                    "deque must have a non_const and non_volatile value type");
        static_assert(is_same_v<typename Allocator::value_type, T>,
                    "deque must have the same value type as its allocator");

    public:
        using value_type                = T;
        using allocator_type            = Allocator;
        using size_type                 = size_t;
        using difference_type           = ptrdiff_t;
        using reference                 = T&;
        using const_reference           = const T&;
        using pointer                   = typename allocator_traits<Allocator>::pointer;
        using const_pointer             = typename allocator_traits<Allocator>::const_pointer;
        using iterator                  = mystl::iterator<mystl::random_access_iterator_tag, T>;
        using const_iterator            = mystl::iterator<mystl::random_access_iterator_tag, const T>;
        using reverse_iterator          = mystl::reverse_iterator<iterator>;
        using const_reverse_iterator    = mystl::reverse_iterator<const_iterator>;

    }; // class deque



} // namespace mystl

#endif //DEQUE_H