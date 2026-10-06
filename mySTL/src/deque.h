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

        constexpr size_type M_storage_size() const noexcept {
            return static_cast<size_type>(M_finish - M_start);
        }

        constexpr size_type M_get_front_slot() noexcept {
            return static_cast<size_type>(M_start._node - M_map);
        }
        constexpr size_type M_get_back_slot() noexcept {
            return M_map_size - static_cast<size_type>(M_finish._node - M_map) - 1;
        }

        constexpr size_type M_get_front_empty_position() noexcept {
            const size_type front_slot = M_get_front_slot();
            return front_slot * block_size + static_cast<size_type>(M_start._cur - M_start._first);
        }
        constexpr size_type M_get_back_empty_position() noexcept {
            const size_type back_slot = M_get_back_slot();
            return back_slot * block_size + static_cast<size_type>(M_finish._last - M_finish._cur) - 1;
        }

        constexpr size_type M_max_size() const noexcept {
            // Leave a full block of headroom for iterator difference arithmetic.
            const size_type diff_limit = static_cast<size_type>(
                (std::numeric_limits<difference_type>::max)()) - block_size;
            const size_type alloc_limit = Traits::max_size(M_allocator);
            return alloc_limit < diff_limit ? alloc_limit : diff_limit;
        }
        static constexpr size_type M_max_size(const allocator_type& alloc) noexcept {
            // Leave a full block of headroom for iterator difference arithmetic.
            const size_type diff_limit = static_cast<size_type>(
                (std::numeric_limits<difference_type>::max)()) - block_size;
            const size_type alloc_limit = Traits::max_size(alloc);
            return alloc_limit < diff_limit ? alloc_limit : diff_limit;
        }

        constexpr pointer M_allocate_node() {
            if (block_size > Traits::max_size(M_allocator))
                throw mystl::length_error("deque block exceeds allocator max_size()");
            return Traits::allocate(M_allocator, block_size);
        }
        constexpr void M_deallocate_node(pointer p) noexcept {
            if (p) Traits::deallocate(M_allocator, p, block_size);
        }

        // Valid only for an uninitialized/moved-from base. Allocates no T objects.
        constexpr void M_initialize_map(size_type n) {
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
        constexpr void M_reserve_map_at_front(size_type nodes = 1) {
            M_reserve_map(nodes, true);
        }
        constexpr void M_reserve_map_at_back(size_type nodes = 1) {
            M_reserve_map(nodes, false);
        }

        // Reallocate the map and optimize centering, 
        // new map size must be larger than old map size
        constexpr void M_reallocate_map(size_type n) {
            if (n <= M_map_size) return;
            const size_type limit = M_map_limit();
            if (n > limit) {
                throw mystl::length_error("deque map is too large");
            }
            assert(M_map != nullptr);
            const size_type front = static_cast<size_type>(M_start._node - M_map);
            const size_type back = static_cast<size_type>(M_finish._node - M_map);
            const size_type used = back - front + 1;
            size_type capacity = n > limit - 2 ? limit : n + 2;
            map_pointer map = M_allocate_map(capacity);
            size_type new_front = (capacity - used) / 2;
            for (size_type i = 0; i < used; ++i) {
                map[new_front + i] = M_map[front + i];
            }
            iterator start(map + new_front, static_cast<size_type>(M_start._cur - M_start._first));
            iterator finish(map + new_front + used - 1, static_cast<size_type>(M_finish._cur - M_finish._first));
            M_deallocate_map(M_map, M_map_size);
            M_map = map;
            M_map_size = capacity;
            M_start = start;
            M_finish = finish;
        }

        // shift map
        constexpr void M_shift_map_left(size_type n) {
            if (n == 0) return;
            const size_type front_slot = M_get_front_slot();
            if (front_slot < n) {
                throw mystl::range_error("shift distance out of range");
            }
            const size_type front = M_start._node - M_map;
            const size_type back = M_finish._node - M_map;
            const size_type used = M_finish._node - M_start._node + 1;
            const size_type first = front - n;
            for (size_type i = 0; i < used; ++i) {
                M_map[first + i] = M_map[front + i];
            }
            for (size_type i = first + used; i <= back; ++i) {
                M_map[i] = nullptr;
            }
            M_start = iterator(M_map + first, M_start._cur - M_start._first);
            M_finish = iterator(M_map + back - n, M_finish._cur - M_finish._first);
        }

        constexpr void M_shift_map_right(size_type n) {
            if (n == 0) return;
            const size_type back_slot = M_get_back_slot();
            if (back_slot < n) {
                throw mystl::range_error("shift distance out of range");
            }
            const size_type front = M_start._node - M_map;
            const size_type back = M_finish._node - M_map;
            const size_type used = M_finish._node - M_start._node + 1;
            const size_type second = back + n;
            for (size_type i = 0; i < used; ++i) {
                M_map[second - i] = M_map[back - i];
            }
            for (size_type i = 0; i < n; ++i) {
                M_map[front + i] = nullptr;
            }
            M_start = iterator(M_map + front + n, M_start._cur - M_start._first);
            M_finish = iterator(M_map + second, M_finish._cur - M_finish._first);
        }

        // The derived container must destroy all live T objects before calling this.
        constexpr void M_release_storage() noexcept {
            if (!M_map) return;
            for (size_type i = 0; i < M_map_size; ++i)
                M_deallocate_node(M_map[i]);
            M_deallocate_map(M_map, M_map_size);
            M_map = nullptr;
            M_map_size = 0;
            M_start = iterator();
            M_finish = iterator();
        }

        constexpr void M_swap_data(deque_base& other) noexcept {
            map_pointer temp_map = M_map;
            size_type temp_map_size = M_map_size;
            iterator temp_start = M_start;
            iterator temp_finish = M_finish;

            M_map = other.M_map;
            M_map_size = other.M_map_size;
            M_start = other.M_start;
            M_finish = other.M_finish;

            other.M_map = temp_map;
            other.M_map_size = temp_map_size;
            other.M_start = temp_start;
            other.M_finish = temp_finish;
        }

    private:
        T_alloc_type M_allocator;
        Map_alloc_type M_map_allocator;

        constexpr size_type M_map_limit() const noexcept {
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

    

    // *************************************************************************************
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
        using Base::block_size;
        using Base::M_map;
        using Base::M_map_size;
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

        constexpr allocator_type get_allocator() const noexcept {return Base::get_allocator();}

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
        deque(Forward_Iterator first, Forward_Iterator last, const allocator_type& alloc, forward_iterator_tag)
            : Base(mystl::distance(first, last), alloc) {
            range_initialize(first, last, forward_iterator_tag{});
        }

    public:
        template <typename Input_Iterator>
            requires mystl::is_input_iterator_v<Input_Iterator>
        deque(Input_Iterator first, Input_Iterator last, const allocator_type& alloc = Allocator()) 
            : deque(first, last, alloc, iter_category<Input_Iterator>{}) {}

        deque(const deque& other) 
            : deque(other, Allocator_traits::select_on_container_copy_construction(other.get_T_allocator())) {}

        deque(const deque& other, const type_identity_t<allocator_type>& alloc)
            : Base(check_init_len(other.size(), alloc), alloc)
        {
            mystl::uninitialized_copy_a(other.begin(), other.end(), M_start, get_T_allocator());
        }

        deque(deque&& other) : Base(mystl::move(other)) {}

        deque(deque&& other, const type_identity_t<allocator_type>& alloc) 
            : Base(mystl::move(other), alloc) {
            // A storage transfer leaves the source map null. Otherwise the base
            // only prepared raw destination storage for unequal allocators.
            if (other.M_map != nullptr) {
                // This helper destroys any partially constructed destination on
                // failure; base destruction then releases its raw storage.
                mystl::uninitialized_move_a(other.M_start, other.M_finish, M_start, get_T_allocator());
                mystl::destroy_a(other.M_start, other.M_finish, other.get_T_allocator());
                other.M_release_storage();
            }
        }

        deque(initializer_list<value_type> list, const allocator_type& alloc = Allocator())
            : Base(list.size(), alloc) {
            range_initialize_n(list.begin(), list.size());
        }

        // *************************************************************************************
        // destruct
        constexpr ~deque() noexcept {
            mystl::destroy_a(M_start, M_finish, get_T_allocator());
        }

        // *************************************************************************************
        // operator == and assign
        constexpr void assign(size_type count, const value_type& value) {
            fill_assign(count, value);
        }

        template <typename Input_Iterator>
            requires mystl::is_input_iterator_v<Input_Iterator>
        constexpr void assign(Input_Iterator first, Input_Iterator last) {
            range_assign(first, last, iter_category<Input_Iterator>{});
        }

        constexpr void assign(initializer_list<value_type> list) {
            range_assign(list.begin(), list.end(), list.size());
        }

        constexpr deque& operator=(const deque& other) {
            if (this != &other){
                if constexpr (Allocator_traits::propagate_on_container_copy_assignment::value) {
                    if (get_T_allocator() != other.get_T_allocator()) {
                        mystl::destroy_a(M_start, M_finish, get_T_allocator());
                        Base::M_release_storage();
                    }
                    // Propagation is required even when the allocators compare equal.
                    get_T_allocator() = other.get_T_allocator();
                }
                assign(other.begin(), other.end());
            }
            return *this;
        }

        constexpr deque& operator=(deque&& other)
        noexcept(Allocator_traits::propagate_on_container_move_assignment::value ||
            Allocator_traits::is_always_equal::value) 
        {
            if (this != &other) {
                move_assign(mystl::move(other), typename Allocator_traits::propagate_on_container_move_assignment{});
            }
            return *this;
        }

        constexpr deque& operator=(initializer_list<value_type> list) {
            range_assign(list.begin(), list.end(), list.size());
            return *this;
        }

        // *************************************************************************************
        // element access
        // operator [] and at
        [[nodiscard]] constexpr reference operator[](size_type idx) noexcept {
            return *(M_start + idx);
        }

        [[nodiscard]] constexpr const_reference operator[](size_type idx) const noexcept {
            return *(M_start + idx);
        }

        [[nodiscard]] constexpr reference at(size_type idx) {
            range_check(idx);
            return *(M_start + idx);
        }

        [[nodiscard]] constexpr const_reference at(size_type idx) const {
            range_check(idx);
            return *(M_start + idx);
        }

        // front, back
        [[nodiscard]] constexpr reference front() noexcept {
            assert(!empty());
            return *M_start;
        }

        [[nodiscard]] constexpr const_reference front() const noexcept {
            assert(!empty());
            return *M_start;
        }

        [[nodiscard]] constexpr reference back() noexcept {
            assert(!empty());
            return *(M_finish - 1);
        }

        [[nodiscard]] constexpr const_reference back() const noexcept {
            assert(!empty());
            return *(M_finish - 1);
        }

        // *************************************************************************************
        // iterator
        [[nodiscard]] constexpr iterator begin() noexcept {
            return iterator(M_start);
        }

        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return const_iterator(M_start);
        }

        [[nodiscard]] constexpr iterator end() noexcept {
            return iterator(M_finish);
        }

        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return const_iterator(M_finish);
        }

        [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
            return const_iterator(M_start);
        }

        [[nodiscard]] constexpr const_iterator cend() const noexcept {
            return const_iterator(M_finish);
        }

        [[nodiscard]] constexpr reverse_iterator rbegin() noexcept {
            return reverse_iterator(M_finish);
        }

        [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(M_finish);
        }

        [[nodiscard]] constexpr reverse_iterator rend() noexcept {
            return reverse_iterator(M_start);
        }

        [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(M_start);
        }

        [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept {
            return const_reverse_iterator(M_finish);
        }
        [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept {
            return const_reverse_iterator(M_start);
        }


        // *************************************************************************************
        // capacity
        [[nodiscard]] constexpr bool empty() const noexcept {
            return M_start == M_finish;
        }

        // size, return the number of element in deque
        [[nodiscard]] constexpr size_type size() const noexcept {
            return Base::M_storage_size();
        }

        // max_size, return the maximum number of element that deque can contain
        [[nodiscard]] constexpr size_type max_size() const noexcept {
            return Base::M_max_size();
        }


    private:
        // Owns only blocks added outside the old active range. The map must not
        // move while this guard is alive. Recording successful allocations uses
        // indices only, so bookkeeping cannot throw after allocating a block.
        struct Guard_nodes {
            deque& owner;
            size_type front_first;
            const size_type front_last;
            const size_type back_first;
            size_type back_last;
            bool active = true;

            explicit Guard_nodes(deque& d) noexcept
                : owner(d), front_first(d.M_start._node - d.M_map),
                  front_last(front_first), back_first(d.M_finish._node - d.M_map + 1),
                  back_last(back_first) {}

            Guard_nodes(const Guard_nodes&) = delete;
            Guard_nodes& operator=(const Guard_nodes&) = delete;
            Guard_nodes(Guard_nodes&&) = delete;
            Guard_nodes& operator=(Guard_nodes&&) = delete;

            ~Guard_nodes() noexcept {
                if (!active) return;
                for (size_type i = front_first; i < front_last; ++i) {
                    owner.M_deallocate_node(owner.M_map[i]);
                    owner.M_map[i] = nullptr;
                }
                for (size_type i = back_first; i < back_last; ++i) {
                    owner.M_deallocate_node(owner.M_map[i]);
                    owner.M_map[i] = nullptr;
                }
            }

            void allocate(size_type front_nodes, size_type back_nodes) {
                assert(front_nodes <= front_first);
                assert(back_nodes <= owner.M_map_size - back_last);
                for (size_type i = 0; i < front_nodes; ++i) {
                    const size_type index = front_first - 1;
                    assert(owner.M_map[index] == nullptr);
                    owner.M_map[index] = owner.M_allocate_node();
                    front_first = index;
                }
                for (size_type i = 0; i < back_nodes; ++i) {
                    assert(owner.M_map[back_last] == nullptr);
                    owner.M_map[back_last] = owner.M_allocate_node();
                    ++back_last;
                }
            }
            void release() noexcept { active = false; }
        };

        // Tracks exactly the objects whose construction has completed. Declare
        // object guards AFTER the block guard: objects must be destroyed first.
        struct Guard_objects {
            T_alloc_type& alloc;
            iterator first;
            iterator last;

            Guard_objects(T_alloc_type& a, iterator begin) noexcept
                : alloc(a), first(begin), last(begin) {}

            Guard_objects(const Guard_objects&) = delete;
            Guard_objects& operator=(const Guard_objects&) = delete;
            Guard_objects(Guard_objects&&) = delete;
            Guard_objects& operator=(Guard_objects&&) = delete;

            ~Guard_objects() noexcept {
                while (last != first) {
                    --last;
                    Allocator_traits::destroy(alloc, last.operator->());
                }
            }
            void fill_until(iterator end, const value_type& value) {
                while (last != end) {
                    Allocator_traits::construct(alloc, last.operator->(), value);
                    ++last;
                }
            }
            template <typename Input_Iterator>
            void range_assign_until(Input_Iterator first, iterator end) {
                while (last != end) {
                    Allocator_traits::construct(alloc, last.operator->(), *first);
                    ++last;
                    ++first;
                }
            }
            template <typename Input_Iterator>
            void range_move_assign_until(Input_Iterator first, iterator end) {
                while (last != end) {
                    Allocator_traits::construct(alloc, last.operator->(), mystl::move(*first));
                    ++last;
                    ++first;
                }
            }

            void release() noexcept { first = last; }
        };

    public:
        // *************************************************************************************
        // modifiers of deque
        constexpr void clear() noexcept {
            if (empty()) return;
            mystl::destroy_a(M_start, M_finish, get_T_allocator());
            size_type mid = (M_map_size - 1) / 2;
            const size_type front = M_start._node - M_map;
            const size_type back = M_finish._node - M_map;
            if (front > mid) {
                mid = front;
            }else if (back < mid) {
                mid = back;
            }
            for (size_type i = front; i < mid; ++i) {
                Base::M_deallocate_node(M_map[i]);
                M_map[i] = nullptr;
            }
            for (size_type i = mid + 1; i <= back; ++i) {
                Base::M_deallocate_node(M_map[i]);
                M_map[i] = nullptr;
            }
            M_start = iterator(M_map + mid, 0);
            M_finish = iterator(M_map + mid, 0);
        }

        // emplace_back
        template <typename... Args>
        constexpr reference emplace_back(Args&&... args) {
            if (size() >= max_size()) {
                throw mystl::length_error("deque cannot be larger than max_size()");
            }
            if (!M_map) Base::M_initialize_map(0);
            
            const bool cross_block = M_finish._cur == M_finish._last - 1;
            if (cross_block) Base::M_reserve_map_at_back();

            Guard_nodes nodes(*this);
            nodes.allocate(0, cross_block ? 1 : 0);

            Allocator_traits::construct(get_T_allocator(), M_finish.operator->(), mystl::forward<Args>(args)...);
            ++M_finish;
            nodes.release();
            return back();
        }

        // push_back
        constexpr void push_back(const value_type& value) {
            emplace_back(value);
        }

        constexpr void push_back(value_type&& value) {
            emplace_back(mystl::move(value));
        }

        // emplace_front
        template <typename... Args>
        constexpr reference emplace_front(Args&&... args) {
            if (size() >= max_size()) {
                throw mystl::length_error("deque cannot be larger than max_size()");
            }
            if (!M_map) Base::M_initialize_map(0);
            
            const bool cross_block = M_start._cur == M_start._first;
            if (cross_block) Base::M_reserve_map_at_front();

            Guard_nodes nodes(*this);
            nodes.allocate(cross_block ? 1 : 0, 0);

            Allocator_traits::construct(get_T_allocator(), (M_start - 1).operator->(), mystl::forward<Args>(args)...);
            --M_start;
            nodes.release();
            return front();
        }

        // push_front
        constexpr void push_front(const value_type& value) {
            emplace_front(value);
        }

        constexpr void push_front(value_type&& value) {
            emplace_front(mystl::move(value));
        }

        // pop_back & pop_front
        constexpr void pop_back() {
            assert(!empty());
            Allocator_traits::destroy(get_T_allocator(), (M_finish - 1).operator->());
            if (M_finish._cur == M_finish._first) {
                Base::M_deallocate_node(M_map[M_finish._node - M_map]);
                M_map[M_finish._node - M_map] = nullptr;
            }
            --M_finish;
        }

        constexpr void pop_front() {
            assert(!empty());
            Allocator_traits::destroy(get_T_allocator(), M_start.operator->());
            if (M_start._cur == M_start._last - 1) {
                Base::M_deallocate_node(M_map[M_start._node - M_map]);
                M_map[M_start._node - M_map] = nullptr;
            }  
            ++M_start;
        }

        // emplace
    private:
        template <typename... Args>
        constexpr iterator move_front_and_emplace(const_iterator pos, Args&&... args) {
            value_type insert_obj(mystl::forward<Args>(args)...);
            const size_type n = pos - M_start;
            iterator insert_pos = M_start + n - 1;

            const bool cross_block = M_start._cur == M_start._first;
            Guard_nodes nodes(*this);
            nodes.allocate(cross_block ? 1 : 0, 0);

            Guard_objects head(get_T_allocator(), M_start - 1); 

            if constexpr (is_nothrow_move_constructible_v<value_type> || !is_copy_constructible_v<value_type>){
                head.range_move_assign_until(M_start, M_start);
            }else {
                head.range_assign_until(M_start, M_start);
            }
            if constexpr (is_nothrow_move_assignable_v<value_type> || !is_copy_assignable_v<value_type>){
                mystl::move(M_start + 1, insert_pos + 1, M_start);
            }else {
                mystl::copy(M_start + 1, insert_pos + 1, M_start);
            }
            if constexpr (is_nothrow_move_assignable_v<value_type> || !is_copy_assignable_v<value_type>) {
                *insert_pos = mystl::move(insert_obj);
            } else {
                *insert_pos = insert_obj;
            }

            --M_start;
            nodes.release();
            head.release();
            return insert_pos;
        }

        template <typename... Args>
        constexpr iterator move_back_and_emplace(const_iterator pos, Args&&... args) {
            value_type insert_obj(mystl::forward<Args>(args)...);
            const size_type n = pos - M_start;
            iterator insert_pos = M_start + n;

            const bool cross_block = M_finish._cur == M_finish._last - 1;
            Guard_nodes nodes(*this);
            nodes.allocate(0, cross_block ? 1 : 0);

            Guard_objects tail(get_T_allocator(), M_finish); 

            if constexpr (is_nothrow_move_constructible_v<value_type> || !is_copy_constructible_v<value_type>){
                tail.range_move_assign_until(M_finish - 1, M_finish);
            }else {
                tail.range_assign_until(M_finish - 1, M_finish);
            }
            if constexpr (is_nothrow_move_assignable_v<value_type> || !is_copy_assignable_v<value_type>){
                mystl::move_backward(insert_pos, M_finish - 1, M_finish);
            }else {
                mystl::copy_backward(insert_pos, M_finish - 1, M_finish);
            }
            if constexpr (is_nothrow_move_assignable_v<value_type> || !is_copy_assignable_v<value_type>) {
                *insert_pos = mystl::move(insert_obj);
            } else {
                *insert_pos = insert_obj;
            }

            ++M_finish;
            nodes.release();
            tail.release();
            return insert_pos;
        }

    public:
        template <typename... Args>
        constexpr iterator emplace(const_iterator pos, Args&&... args) {
            if (size() >= max_size()) {
                throw mystl::length_error("deque cannot be larger than max_size()");
            }
            if (!M_map) {
                assert(pos == M_start);
                Base::M_initialize_map(0);
                emplace_back(mystl::forward<Args>(args)...);
                return M_start;
            }
            assert(pos >= M_start && pos <= M_finish);
            if (pos == cbegin()) {
                emplace_front(mystl::forward<Args>(args)...);
                return begin();
            }
            if (pos == cend()) {
                emplace_back(mystl::forward<Args>(args)...);
                return end() - 1;
            }
            const size_type first = pos - M_start;
            iterator insert_pos;
            if (first <= size() / 2) {
                if (Base::M_get_front_empty_position() > 0) {
                    insert_pos = move_front_and_emplace(pos, mystl::forward<Args>(args)...);
                }else if (Base::M_get_back_slot() > 0) {
                    const size_type tatal_slot = Base::M_get_back_slot();
                    Base::M_shift_map_right((tatal_slot + 1) / 2);
                    insert_pos = M_start + first;
                    insert_pos = move_front_and_emplace(insert_pos, mystl::forward<Args>(args)...);
                }else {
                    if (M_map_size >= Base::M_map_limit())
                        throw mystl::length_error("no extra space available for emplace");
                    Base::M_reserve_map_at_front();
                    insert_pos = M_start + first;
                    insert_pos = move_front_and_emplace(insert_pos, mystl::forward<Args>(args)...);
                }
            }else {
                if (Base::M_get_back_empty_position() > 0) {
                    insert_pos = move_back_and_emplace(pos, mystl::forward<Args>(args)...);
                }else if (Base::M_get_front_slot() > 0) {
                    const size_type tatal_slot = Base::M_get_front_slot();
                    Base::M_shift_map_left((tatal_slot + 1) / 2);
                    insert_pos = M_start + first;
                    insert_pos = move_back_and_emplace(insert_pos, mystl::forward<Args>(args)...);
                }else {
                    if (M_map_size >= Base::M_map_limit())
                        throw mystl::length_error("no extra space available for emplace");
                    Base::M_reserve_map_at_back();
                    insert_pos = M_start + first;
                    insert_pos = move_back_and_emplace(insert_pos, mystl::forward<Args>(args)...);
                }
            }
            return insert_pos;
        }

        // insert
        constexpr iterator insert(const_iterator pos, const value_type& value) {
            return emplace(pos, value);
        }

        constexpr iterator insert(const_iterator pos, value_type&& value) {
            return emplace(pos, mystl::move(value));
        }




    private:
        // *************************************************************************************
        // fill assign
        constexpr void fill_assign(size_type count, const value_type& value) {
            if (count > max_size())
                throw mystl::length_error("cannot create deque larger than max_size()");
            if (count == 0) {
                clear();
                return;
            }
            // value may refer to an element that this operation assigns/destroys.
            const value_type stable(value);
            if (!M_map) Base::M_initialize_map(0);

            size_type full_size = M_map_size * block_size - 1;
            if (count > full_size) {
                Base::M_reallocate_map(count / block_size + 1);
                full_size = M_map_size * block_size - 1;
            }
            if (count > size()) {
                fill_assign_impl(count, stable);
            } else {
                const size_type redundant_count = size() - count;
                const size_type front_empty = Base::M_get_front_empty_position();
                const size_type desired_front = (full_size - count) / 2;
                const size_type front_redundant = desired_front > front_empty
                    ? mystl::min(redundant_count, desired_front - front_empty) : 0;
                const size_type back_redundant = redundant_count - front_redundant;
                iterator new_start = M_start + static_cast<difference_type>(front_redundant);
                iterator new_finish = M_finish - static_cast<difference_type>(back_redundant);

                // No lifetimes end until all potentially throwing assignments finish.
                mystl::fill(new_start, new_finish, stable);
                mystl::destroy_a(M_start, new_start, get_T_allocator());
                mystl::destroy_a(new_finish, M_finish, get_T_allocator());
                for (size_type i = M_start._node - M_map; i < size_type(new_start._node - M_map); ++i) {
                    Base::M_deallocate_node(M_map[i]);
                    M_map[i] = nullptr;
                }
                for (size_type i = new_finish._node - M_map + 1; i <= size_type(M_finish._node - M_map); ++i) {
                    Base::M_deallocate_node(M_map[i]);
                    M_map[i] = nullptr;
                }
                M_start = new_start;
                M_finish = new_finish;
            }
        }

        constexpr void fill_assign_impl(size_type count, const value_type& value) {
            // The caller has already prepared a map large enough for count. Plan
            // both ends before allocating anything; never relocate the map while
            // the guards hold indices or iterators into it.
            const size_type front_empty = Base::M_get_front_empty_position();
            const size_type back_empty = Base::M_get_back_empty_position();
            const size_type extra_count = count - size();
            assert(extra_count <= front_empty + back_empty);
            const size_type desired_front = (front_empty + back_empty - extra_count) / 2;
            const size_type front_extra = front_empty > desired_front
                ? mystl::min(extra_count, front_empty - desired_front) : 0;
            const size_type back_extra = extra_count - front_extra;

            const size_type front_offset = M_start._cur - M_start._first;
            const size_type remaining = front_extra > front_offset ? front_extra - front_offset : 0;
            const size_type front_nodes = remaining / block_size + (remaining % block_size != 0);
            const size_type back_offset = M_finish._cur - M_finish._first;
            const size_type back_nodes = back_extra / block_size
                + (back_offset + back_extra % block_size) / block_size;

            Guard_nodes nodes(*this);
            nodes.allocate(front_nodes, back_nodes);

            iterator new_start = M_start - static_cast<difference_type>(front_extra);
            iterator new_finish = M_finish + static_cast<difference_type>(back_extra);

            Guard_objects head(get_T_allocator(), new_start);
            Guard_objects tail(get_T_allocator(), M_finish);

            head.fill_until(M_start, value);
            mystl::fill(M_start, M_finish, value);
            tail.fill_until(new_finish, value);

            // Commit only after every construction and assignment has succeeded.
            M_start = new_start;
            M_finish = new_finish;
            head.release();
            tail.release();
            nodes.release();
        }

        template <typename Input_Iterator>
        constexpr void range_assign(Input_Iterator first, Input_Iterator last, input_iterator_tag) {
            clear();
            for (; first != last; ++first) {
                emplace_back(*first);
            }
        }

        // range assign
        template <typename Forward_Iterator>
        constexpr void range_assign(Forward_Iterator first, Forward_Iterator last, forward_iterator_tag) {
            const size_type n = mystl::distance(first, last);
            range_assign(first, last, n);
        }

        template <typename Iterator>
        constexpr void range_assign(Iterator first, Iterator last, size_type n) {
            if (n > max_size())
                throw mystl::length_error("cannot create deque larger than max_size()");
            if (n == 0){
                clear();
                return;
            }
            if (!M_map) Base::M_initialize_map(0);

            size_type full_size = M_map_size * block_size - 1;
            if (n > full_size) {
                Base::M_reallocate_map(n / block_size + 1);
                full_size = M_map_size * block_size - 1;
            }
            if (n > size()) {
                range_assign_impl(first, last, n);
            } else {
                const size_type redundant_n = size() - n;
                const size_type front_empty = Base::M_get_front_empty_position();
                const size_type desired_front = (full_size - n) / 2;
                const size_type front_redundant = desired_front > front_empty
                    ? mystl::min(redundant_n, desired_front - front_empty) : 0;
                const size_type back_redundant = redundant_n - front_redundant;
                iterator new_start = M_start + static_cast<difference_type>(front_redundant);
                iterator new_finish = M_finish - static_cast<difference_type>(back_redundant);

                // No lifetimes end until all potentially throwing assignments finish.
                mystl::copy(first, last, new_start);
                mystl::destroy_a(M_start, new_start, get_T_allocator());
                mystl::destroy_a(new_finish, M_finish, get_T_allocator());
                for (size_type i = M_start._node - M_map; i < size_type(new_start._node - M_map); ++i) {
                    Base::M_deallocate_node(M_map[i]);
                    M_map[i] = nullptr;
                }
                for (size_type i = new_finish._node - M_map + 1; i <= size_type(M_finish._node - M_map); ++i) {
                    Base::M_deallocate_node(M_map[i]);
                    M_map[i] = nullptr;
                }
                M_start = new_start;
                M_finish = new_finish;
            }
        }

        template <typename Iterator>
        constexpr void range_assign_impl(Iterator first, Iterator last, size_type n) {
            const size_type front_empty = Base::M_get_front_empty_position();
            const size_type back_empty = Base::M_get_back_empty_position();
            const size_type extra_n = n - size();
            assert(extra_n <= front_empty + back_empty);
            const size_type desired_front = (front_empty + back_empty - extra_n) / 2;
            const size_type front_extra = front_empty > desired_front
                ? mystl::min(extra_n, front_empty - desired_front) : 0;
            const size_type back_extra = extra_n - front_extra;

            const size_type front_offset = M_start._cur - M_start._first;
            const size_type remaining = front_extra > front_offset ? front_extra - front_offset : 0;
            const size_type front_nodes = remaining / block_size + (remaining % block_size != 0);
            const size_type back_offset = M_finish._cur - M_finish._first;
            const size_type back_nodes = back_extra / block_size
                + (back_offset + back_extra % block_size) / block_size;

            Guard_nodes nodes(*this);
            nodes.allocate(front_nodes, back_nodes);

            iterator new_start = M_start - static_cast<difference_type>(front_extra);
            iterator new_finish = M_finish + static_cast<difference_type>(back_extra);

            Guard_objects head(get_T_allocator(), new_start);
            Guard_objects tail(get_T_allocator(), M_finish);

            auto it1 = first;
            auto it2 = first;
            mystl::advance(it1, front_extra);
            mystl::advance(it2, front_extra + size());

            head.range_assign_until(first, M_start);
            mystl::copy(it1, it2, M_start);
            tail.range_assign_until(it2, new_finish);

            M_start = new_start;
            M_finish = new_finish;
            head.release();
            tail.release();
            nodes.release();
        }

        // move assign
        constexpr void move_assign(deque&& other, true_type) {
            get_T_allocator() = mystl::move(other.get_T_allocator());
            mystl::destroy_a(M_start, M_finish, get_T_allocator());
            Base::M_release_storage();
            Base::M_swap_data(other);
        }

        constexpr void move_assign(deque&& other, false_type) {
            if (get_T_allocator() == other.get_T_allocator()) {
                mystl::destroy_a(M_start, M_finish, get_T_allocator());
                Base::M_release_storage();
                Base::M_swap_data(other);
            }else {
                range_move_assign(other.begin(), other.end(), other.size());
                mystl::destroy_a(other.M_start, other.M_finish, other.get_T_allocator());
                other.M_release_storage();
            }
        }

        template <typename Forward_Iterator>
        constexpr void range_move_assign(Forward_Iterator first, Forward_Iterator last, size_type n) {
            if (n > max_size())
                throw mystl::length_error("cannot create deque larger than max_size()");
            if (n == 0) {
                clear();
                return;
            }
            if (!M_map) Base::M_initialize_map(0);

            size_type full_size = M_map_size * block_size - 1;
            if (n > full_size) {
                Base::M_reallocate_map(n / block_size + 1);
                full_size = M_map_size * block_size - 1;
            }
            if (n > size()) {
                range_move_assign_impl(first, last, n);
            }else {
                const size_type redundant_n = size() - n;
                const size_type front_empty = Base::M_get_front_empty_position();
                const size_type desired_front = (full_size - n) / 2;
                const size_type front_redundant = desired_front > front_empty
                    ? mystl::min(redundant_n, desired_front - front_empty) : 0;
                const size_type back_redundant = redundant_n - front_redundant;
                iterator new_start = M_start + static_cast<difference_type>(front_redundant);
                iterator new_finish = M_finish - static_cast<difference_type>(back_redundant);

                // No lifetimes end until all potentially throwing assignments finish.
                mystl::move(first, last, new_start);
                mystl::destroy_a(M_start, new_start, get_T_allocator());
                mystl::destroy_a(new_finish, M_finish, get_T_allocator());
                for (size_type i = M_start._node - M_map; i < size_type(new_start._node - M_map); ++i) {
                    Base::M_deallocate_node(M_map[i]);
                    M_map[i] = nullptr;
                }
                for (size_type i = new_finish._node - M_map + 1; i <= size_type(M_finish._node - M_map); ++i) {
                    Base::M_deallocate_node(M_map[i]);
                    M_map[i] = nullptr;
                }
                M_start = new_start;
                M_finish = new_finish;
            }
        }

        template <typename Iterator>
        constexpr void range_move_assign_impl(Iterator first, Iterator last, size_type n) {
            const size_type front_empty = Base::M_get_front_empty_position();
            const size_type back_empty = Base::M_get_back_empty_position();
            const size_type extra_n = n - size();
            assert(extra_n <= front_empty + back_empty);
            const size_type desired_front = (front_empty + back_empty - extra_n) / 2;
            const size_type front_extra = front_empty > desired_front
                ? mystl::min(extra_n, front_empty - desired_front) : 0;
            const size_type back_extra = extra_n - front_extra;

            const size_type front_offset = M_start._cur - M_start._first;
            const size_type remaining = front_extra > front_offset ? front_extra - front_offset : 0;
            const size_type front_nodes = remaining / block_size + (remaining % block_size != 0);
            const size_type back_offset = M_finish._cur - M_finish._first;
            const size_type back_nodes = back_extra / block_size
                + (back_offset + back_extra % block_size) / block_size;

            Guard_nodes nodes(*this);
            nodes.allocate(front_nodes, back_nodes);

            iterator new_start = M_start - static_cast<difference_type>(front_extra);
            iterator new_finish = M_finish + static_cast<difference_type>(back_extra);

            Guard_objects head(get_T_allocator(), new_start);
            Guard_objects tail(get_T_allocator(), M_finish);

            auto it1 = first;
            auto it2 = first;
            mystl::advance(it1, front_extra);
            mystl::advance(it2, front_extra + size());

            head.range_move_assign_until(first, M_start);
            mystl::move(it1, it2, M_start);
            tail.range_move_assign_until(it2, new_finish);

            M_start = new_start;
            M_finish = new_finish;
            head.release();
            tail.release();
            nodes.release();
        }

        // initialize
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

        // range and length helper
        constexpr void range_check(size_type idx) const {
            if (idx >= size()) {
                throw mystl::out_of_range("index out of range");
            }
        }

        static constexpr size_type check_init_len(const size_type n, const T_alloc_type& alloc) {
            if (n > get_max_size(alloc)) {
                throw mystl::length_error("cannot create deque larger than max_size()");
            }
            return n;
        }

        static constexpr size_type get_max_size(const T_alloc_type& alloc) noexcept {
            return Base::M_max_size(alloc);
        }
    };

} // namespace mystl

#endif // DEQUE_H
