#pragma once

#ifndef DEQUE_H
#define DEQUE_H

#include <cassert>
#include <cstddef>
#include <limits>

#include "iterator.h"
#include "memory.h"
#include "utility.h"
#include "stdexcept.h"
#include "initializer_list.h"

namespace mystl {

    template <typename T>
    inline constexpr size_t deque_block_size = sizeof(T) < 512 ? 512 / sizeof(T) : 1;

    template <typename T, typename Allocator> class deque_base;
    template <typename T, typename Allocator> class deque;

    template <typename T, bool IsConst>
    class deque_iterator {
        template <typename, bool> friend class deque_iterator;
        template <typename, typename> friend class deque_base;
        template <typename, typename> friend class deque;

    public:
        using iterator_category = mystl::random_access_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = conditional_t<IsConst, const T*, T*>;
        using reference = conditional_t<IsConst, const T&, T&>;

        constexpr deque_iterator() noexcept = default;

        template <bool OtherConst>
            requires (IsConst && !OtherConst)
        constexpr deque_iterator(const deque_iterator<T, OtherConst>& other) noexcept
            : _cur(other._cur), _first(other._first), _last(other._last), _node(other._node) {}

        constexpr reference operator*() const noexcept { return *_cur; }
        constexpr pointer operator->() const noexcept { return _cur; }

        constexpr deque_iterator& operator++() noexcept {
            if (++_cur == _last) {
                M_set_node(_node + 1);
                _cur = _first;
            }
            return *this;
        }
        constexpr deque_iterator operator++(int) noexcept {
            auto old = *this;
            ++*this;
            return old;
        }
        constexpr deque_iterator& operator--() noexcept {
            if (_cur == _first) {
                M_set_node(_node - 1);
                _cur = _last;
            }
            --_cur;
            return *this;
        }
        constexpr deque_iterator operator--(int) noexcept {
            auto old = *this;
            --*this;
            return old;
        }

        constexpr deque_iterator& operator+=(difference_type n) noexcept {
            if (n == 0) return *this; // Also permits a singular, moved-from empty range.
            constexpr difference_type block = deque_block_size<T>;
            // Divide before adding the local offset, avoiding overflow in n + offset.
            difference_type nodes = n / block;
            difference_type offset = (_cur - _first) + n % block;
            if (offset < 0) {
                --nodes;
                offset += block;
            } else if (offset >= block) {
                ++nodes;
                offset -= block;
            }
            M_set_node(_node + nodes);
            _cur = _first + offset;
            return *this;
        }
        constexpr deque_iterator& operator-=(difference_type n) noexcept {
            // A valid range has at most PTRDIFF_MAX elements.
            assert(n != (std::numeric_limits<difference_type>::min)());
            return *this += -n;
        }
        constexpr deque_iterator operator+(difference_type n) const noexcept {
            auto result = *this;
            return result += n;
        }
        constexpr deque_iterator operator-(difference_type n) const noexcept {
            auto result = *this;
            return result -= n;
        }
        friend constexpr deque_iterator operator+(difference_type n, deque_iterator it) noexcept {
            return it += n;
        }
        constexpr reference operator[](difference_type n) const noexcept { return *(*this + n); }

        template <bool C>
        constexpr difference_type operator-(const deque_iterator<T, C>& other) const noexcept {
            if (_node == other._node) {
                return _cur == other._cur ? 0 : _cur - other._cur;
            }
            return (_node - other._node) * static_cast<difference_type>(deque_block_size<T>)
                + (_cur - _first) - (other._cur - other._first);
        }
        template <bool C>
        constexpr bool operator==(const deque_iterator<T, C>& other) const noexcept {
            return _node == other._node && _cur == other._cur;
        }
        template <bool C>
        constexpr bool operator!=(const deque_iterator<T, C>& other) const noexcept {
            return !(*this == other);
        }
        template <bool C>
        constexpr bool operator<(const deque_iterator<T, C>& other) const noexcept {
            return _node == other._node ? _cur < other._cur : _node < other._node;
        }
        template <bool C>
        constexpr bool operator>(const deque_iterator<T, C>& other) const noexcept { return other < *this; }
        template <bool C>
        constexpr bool operator<=(const deque_iterator<T, C>& other) const noexcept { return !(other < *this); }
        template <bool C>
        constexpr bool operator>=(const deque_iterator<T, C>& other) const noexcept { return !(*this < other); }

    private:
        pointer _cur = nullptr;
        pointer _first = nullptr;
        pointer _last = nullptr;
        T* const* _node = nullptr;

        constexpr deque_iterator(T* const* node, size_t offset) noexcept {
            M_set_node(node);
            _cur = _first + offset;
        }
        constexpr void M_set_node(T* const* node) noexcept {
            _node = node;
            _first = *node;
            _last = _first + deque_block_size<T>;
        }
    };

    // Owns RAW storage only. The derived container constructs/destroys T objects.
    // Every non-null map slot owns one fixed-size block; active blocks occupy
    // [_start._node, _finish._node]; all slots outside that range are null.
    // _finish always lies inside an allocated block,
    // including when the requested range ends exactly at a block boundary.
    template <typename T, typename Allocator = mystl::allocator<T>>
    class deque_base {
    public:
        using allocator_type = Allocator;
        using T_alloc_type = typename allocator_traits<Allocator>::template rebind_alloc<T>;
        using Traits = mystl::allocator_traits<T_alloc_type>;
        using pointer = typename Traits::pointer;
        using Map_alloc_type = typename Traits::template rebind_alloc<pointer>;
        using Map_Traits = mystl::allocator_traits<Map_alloc_type>;
        using map_pointer = typename Map_Traits::pointer;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using iterator = deque_iterator<T, false>;
        using const_iterator = deque_iterator<T, true>;

        static_assert(is_same_v<typename Allocator::value_type, T>,
                      "deque allocator must have the same value_type as T");
        static_assert(is_same_v<pointer, T*> && is_same_v<map_pointer, T**>,
                      "deque currently supports only raw allocator pointers");

        allocator_type get_allocator() const { return allocator_type(M_allocator); }

    protected:
        static constexpr size_type block_size = deque_block_size<T>;
        map_pointer M_map = nullptr;
        size_type M_map_size = 0;
        iterator M_start;
        iterator M_finish;

        deque_base() : deque_base(0, allocator_type()) {}

        explicit deque_base(const allocator_type& alloc) : deque_base(0, alloc) {}

        explicit deque_base(size_type n, const allocator_type& alloc = allocator_type())
            : M_allocator(alloc), M_map_allocator(M_allocator) {
            M_initialize_map(n);
        }

        // Copy allocator state before transferring storage. If either allocator
        // copy throws, the source still owns its memory with unchanged allocators.
        deque_base(deque_base&& other)
            noexcept(is_nothrow_copy_constructible_v<T_alloc_type>
                     && is_nothrow_copy_constructible_v<Map_alloc_type>)
            : M_allocator(other.M_allocator), M_map_allocator(other.M_map_allocator) {
            M_take_storage(other);
        }

        // With unequal allocators, prepare a same-sized RAW destination range.
        // The derived container must relocate elements; the source remains intact.
        deque_base(deque_base&& other, const allocator_type& alloc)
            : M_allocator(alloc), M_map_allocator(M_allocator) {
            if (M_allocator == other.M_allocator && M_map_allocator == other.M_map_allocator) {
                M_take_storage(other);
            } else {
                M_initialize_map(other.M_storage_size());
            }
        }

        deque_base(const deque_base&) = delete;
        deque_base& operator=(const deque_base&) = delete;
        deque_base& operator=(deque_base&&) = delete;

        ~deque_base() noexcept { M_release_storage(); }

        T_alloc_type& get_T_allocator() noexcept { return M_allocator; }
        const T_alloc_type& get_T_allocator() const noexcept { return M_allocator; }

        size_type M_storage_size() const noexcept {
            return static_cast<size_type>(M_finish - M_start);
        }
        size_type M_max_size() const noexcept {
            // Leave a full block of headroom for iterator difference arithmetic.
            const size_type diff_limit = static_cast<size_type>(
                (std::numeric_limits<difference_type>::max)()) - block_size;
            const size_type alloc_limit = Traits::max_size(M_allocator);
            return alloc_limit < diff_limit ? alloc_limit : diff_limit;
        }

        pointer M_allocate_node() {
            if (block_size > Traits::max_size(M_allocator))
                throw mystl::length_error("deque block exceeds allocator max_size()");
            return Traits::allocate(M_allocator, block_size);
        }
        void M_deallocate_node(pointer p) noexcept {
            if (p) Traits::deallocate(M_allocator, p, block_size);
        }

        // Valid only for an uninitialized/moved-from base. Allocates no T objects.
        void M_initialize_map(size_type n) {
            assert(M_map == nullptr);
            const size_type limit = M_map_limit();
            if (n > M_max_size() || limit < 3)
                throw mystl::length_error("deque storage is too large");
            const size_type nodes = n / block_size + 1;
            if (nodes > limit - 2)
                throw mystl::length_error("deque map is too large");
            size_type capacity = nodes + 2;
            if (capacity < 8) capacity = limit < 8 ? limit : 8;
            map_pointer map = M_allocate_map(capacity);
            const size_type first = (capacity - nodes) / 2;
            size_type allocated = 0;
            try {
                for (; allocated < nodes; ++allocated)
                    map[first + allocated] = M_allocate_node();
            } catch (...) {
                for (size_type i = 0; i < allocated; ++i)
                    M_deallocate_node(map[first + i]);
                M_deallocate_map(map, capacity);
                throw;
            }
            M_map = map;
            M_map_size = capacity;
            M_start = iterator(map + first, 0);
            M_finish = iterator(map + first + nodes - 1, n % block_size);
        }

        // Reserve pointer slots only, not element blocks. On success map growth
        // may invalidate iterators, never element addresses. On failure no change.
        // Future insertions must also roll back/coordinate any later T construction.
        void M_reserve_map_at_front(size_type nodes = 1) {
            M_reserve_map(nodes, true);
        }
        void M_reserve_map_at_back(size_type nodes = 1) {
            M_reserve_map(nodes, false);
        }

        // The derived container must destroy all live T objects before calling this.
        void M_release_storage() noexcept {
            if (!M_map) return;
            for (size_type i = 0; i < M_map_size; ++i)
                M_deallocate_node(M_map[i]);
            M_deallocate_map(M_map, M_map_size);
            M_map = nullptr;
            M_map_size = 0;
            M_start = iterator();
            M_finish = iterator();
        }

    private:
        T_alloc_type M_allocator;
        Map_alloc_type M_map_allocator;

        size_type M_map_limit() const noexcept {
            const size_type alloc_limit = Map_Traits::max_size(M_map_allocator);
            const size_type diff_limit = static_cast<size_type>(
                (std::numeric_limits<difference_type>::max)()) / block_size;
            return alloc_limit < diff_limit ? alloc_limit : diff_limit;
        }

        map_pointer M_allocate_map(size_type n) {
            map_pointer map = Map_Traits::allocate(M_map_allocator, n);
            size_type constructed = 0;
            try {
                for (; constructed < n; ++constructed)
                    Map_Traits::construct(M_map_allocator, map + constructed, pointer());
            } catch (...) {
                while (constructed)
                    Map_Traits::destroy(M_map_allocator, map + --constructed);
                Map_Traits::deallocate(M_map_allocator, map, n);
                throw;
            }
            return map;
        }
        void M_deallocate_map(map_pointer map, size_type n) noexcept {
            for (size_type i = 0; i < n; ++i)
                Map_Traits::destroy(M_map_allocator, map + i);
            Map_Traits::deallocate(M_map_allocator, map, n);
        }

        void M_take_storage(deque_base& other) noexcept {
            M_map = other.M_map;
            M_map_size = other.M_map_size;
            M_start = other.M_start;
            M_finish = other.M_finish;
            other.M_map = nullptr;
            other.M_map_size = 0;
            other.M_start = iterator();
            other.M_finish = iterator();
        }

        void M_reserve_map(size_type extra, bool at_front) {
            if (extra == 0) return;
            // Reuse moved-from bases through M_initialize_map(0) first.
            assert(M_map != nullptr);
            const size_type front = static_cast<size_type>(M_start._node - M_map);
            const size_type back = static_cast<size_type>(M_finish._node - M_map);
            const size_type available = at_front ? front : M_map_size - back - 1;
            if (extra <= available) return;
            const size_type used = back - front + 1;
            const size_type limit = M_map_limit();
            if (limit < used || limit - used < 2 || extra > limit - used - 2)
                throw mystl::length_error("deque map is too large");
            const size_type needed = used + extra + 2;
            size_type capacity = M_map_size;
            if (capacity < needed) {
                capacity = capacity > limit / 2 ? limit : capacity * 2;
                if (capacity < needed) capacity = needed;
            }
            // A new map also handles recentering without overlapping copies.
            map_pointer map = M_allocate_map(capacity);
            const size_type first = (capacity - used - extra) / 2 + (at_front ? extra : 0);
            for (size_type i = 0; i < used; ++i)
                map[first + i] = M_map[front + i];
            iterator start(map + first, static_cast<size_type>(M_start._cur - M_start._first));
            iterator finish(map + first + used - 1,
                            static_cast<size_type>(M_finish._cur - M_finish._first));
            M_deallocate_map(M_map, M_map_size);
            M_map = map;
            M_map_size = capacity;
            M_start = start;
            M_finish = finish;
        }
    };

    // Container interface scaffold. Element construction and modifiers are not
    // implemented yet; do not expose the base's raw-size constructor as a deque ctor.
    template <typename T, typename Allocator = mystl::allocator<T>>
    class deque : protected deque_base<T, Allocator> {
        static_assert(is_same_v<remove_cv_t<T>, T>,
                      "deque must have a non_const and non_volatile value type");
        using Base = deque_base<T, Allocator>;
        using T_alloc_type      = Base::T_alloc_type;
        using Allocator_traits  = allocator_traits<T_alloc_type>;

    protected:
        using Base::M_start;
        using Base::M_finish;
        using Base::get_T_allocator;

    public:
        using value_type = T;
        using allocator_type = Allocator;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = typename allocator_traits<Allocator>::pointer;
        using const_pointer = typename allocator_traits<Allocator>::const_pointer;
        using iterator = deque_iterator<T, false>;
        using const_iterator = deque_iterator<T, true>;
        using reverse_iterator = mystl::reverse_iterator<iterator>;
        using const_reverse_iterator = mystl::reverse_iterator<const_iterator>;

        // *************************************************************************************
        // construct
        deque() = default;

        explicit deque(const allocator_type& alloc) : Base(alloc) {}

        explicit deque(size_type count, const allocator_type& alloc = Allocator()) 
            : Base(check_init_len(count, alloc), alloc) {
            default_initialize(count);
        }

        deque(size_type count, const value_type value, const allocator_type& alloc = Allocator())
            : Base(count, alloc)
        {
            fill_initialize(value);
        }
    
    private:
        template <typename Input_Iterator>
        deque(Input_Iterator first, Input_Iterator last, const allocator_type& alloc, input_iterator_tag)
            : Base(alloc) {
            range_initialize(first, last, input_iterator_tag{});
        }         

        template <typename Forward_Iterator>
        deque(Input_Iterator first, Input_Iterator last, const allocator_type& alloc, forward_iterator_tag)
            : Base(mystl::distance(first, last), alloc) {
            range_initialize(first, last, forward_iterator_tag{});
        }

    public:
        template <typename Input_Iterator>
        deque(Input_Iterator first, Input_Iterator last, const allocator_type& alloc = Allocator()) 
            : deque(first, last, alloc, iter_category<Input_Iterator>{})

        deque(const deque& other) 
            : deque(other, Allocator_traits::select_on_container_copy_construction(other.get_T_allocator())) {}

        deque(const deque& other, const type_identity_t<allocator_type>& alloc)
            : Base(check_init_len(other.size(), alloc), alloc)
        {
            mystl::uninitialized_copy_a(other.begin(), other.end(), M_start, get_T_allocator());
        }

        deque(deque&& other) : Base(mystl::move(other)) {}

        deque(deque&& other, const type_identity_t<allocator_type>& alloc) 
            : Base(mystl::move(other), alloc) {}

        deque(initializer_list<value_type> list, const allocator_type& alloc = Allocator())
            : Base(list.size(), alloc) {
            range_initialize_n(list.begin(), list.size());
        }

        // *************************************************************************************
        // destruct
        constexpr ~deque() noexcept {
            mystl::destroy_a(M_start, M_finish, get_T_allocator());
        }


    private:
        constexpr void default_initialize(size_type n) {
            uninitialized_default_construct_n_a(M_start, n, get_T_allocator());
        }

        constexpr void fill_initialize(const value_type& value) {
            uninitialized_fill_a(M_start, M_finish, value, get_T_allocator());
        }

        template <typename Iterator>
        constexpr void range_initialize_n(Iterator first, size_type n) {
            uninitialized_copy_n_a(first, n, M_start, get_T_allocator());
        }

        template <typename Input_Iterator>
        constexpr void range_initialize(Input_Iterator first, Input_Iterator last, input_iterator_tag) {
            try {
                for (; first != last; ++first){
                    emplace_back(*first);
                }
            }catch (...) {
                clear();
                throw;
            }
        }

        template <typename Forward_Iterator>
        constexpr void range_initialize(Forward_Iterator first, Forward_Iterator last, forward_iterator_tag) {
            uninitialized_copy_a(first, last, M_start, get_T_allocator());
        }


        static constexpr size_type check_init_len(const size_type n, const T_alloc_type& alloc) {
            if (n > get_max_size(alloc)) {
                throw mystl::length_error("cannot create deque larger than max_size()");
            }
            return n;
        }

        static constexpr size_type get_max_size(const T_alloc_type& alloc) noexcept {
            const size_t diff_max = std::numeric_limits<ptrdiff_t>::max() / sizeof(T);
            const size_t alloc_max = Allocator_traits::max_size(alloc);
            return min(diff_max, alloc_max);
        }
    };

} // namespace mystl

#endif // DEQUE_H
