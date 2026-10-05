#pragma once

#ifndef VECTOR_H
#define VECTOR_H

#include <limits>
#include <cassert>

#include "iterator.h"
#include "memory.h"
#include "utility.h"
#include "stdexcept.h"
#include "algorithm.h"
#include "initializer_list.h"

namespace mystl {
#define MYSTL_DEBUG(expr) assert(expr)
#define THROW_LENGTH_ERROR_IF(expr, what) if((expr)) throw mystl::length_error(what)
#define THROW_OUT_OF_RANGE_IF(expr, what) if((expr)) throw mystl::out_of_range(what)
#define THROW_RUNTIME_ERROR_IF(expr, what) if((expr)) throw mystl::runtime_error(what)

#ifdef max
#pragma message("#undefing marco max")
#undef max
#endif // max

#ifdef min
#pragma message("#undefing marco min")
#undef min
#endif // min

    template <typename T, typename Allocator>
    struct vector_base {
        using allocator_type = Allocator;
        using T_alloc_type = typename allocator_traits<Allocator>::template rebind_alloc<T>;
        using pointer = typename allocator_traits<T_alloc_type>::pointer;

        struct _vector_impl_data {
            pointer _begin;
            pointer _end;
            pointer _end_of_storage;

            constexpr _vector_impl_data() noexcept : _begin(), _end(), _end_of_storage() {}

            constexpr _vector_impl_data(_vector_impl_data&& other) noexcept
                        : _begin(other._begin), _end(other._end), _end_of_storage(other._end_of_storage) {
                other._begin = other._end = other._end_of_storage = pointer();
            }

            constexpr void _copy_data(const _vector_impl_data& other) noexcept {
                _begin = other._begin;
                _end = other._end;
                _end_of_storage = other._end_of_storage;
            }

            constexpr void _swap_data(_vector_impl_data& other) noexcept {
                _vector_impl_data temp;
                temp._copy_data(*this);
                _copy_data(other);
                other._copy_data(temp);
            }
        };

        struct _vector_impl : T_alloc_type, _vector_impl_data {
            constexpr _vector_impl() noexcept(is_nothrow_default_constructible_v<T_alloc_type>)
                    requires is_default_constructible_v<T_alloc_type> : T_alloc_type() {}

            constexpr _vector_impl(const T_alloc_type& alloc) noexcept : T_alloc_type(alloc) {}

            constexpr _vector_impl(T_alloc_type&& alloc) noexcept : T_alloc_type(mystl::move(alloc)) {}

            constexpr _vector_impl(_vector_impl&& other) noexcept
                    : T_alloc_type(mystl::move(other)), _vector_impl_data(mystl::move(other)) {}

            constexpr _vector_impl(T_alloc_type&& alloc, _vector_impl_data&& other) noexcept
                    : T_alloc_type(mystl::move(alloc)), _vector_impl_data(mystl::move(other)) {}
        };

        _vector_impl M_impl;

        constexpr pointer M_allocate(size_t n) {
            using Traits = allocator_traits<T_alloc_type>;
            return n == 0 ? pointer() : Traits::allocate(M_impl, n);
        }

        constexpr void M_deallocate(pointer p, size_t n) {
            using Traits = allocator_traits<T_alloc_type>;
            if (p) Traits::deallocate(M_impl, p, n);
        }

        // get allocator
        constexpr T_alloc_type& get_T_allocator() noexcept {
            return static_cast<T_alloc_type&>(this -> M_impl);
        }

        constexpr const T_alloc_type& get_T_allocator() const noexcept {
            return static_cast<const T_alloc_type&>(this -> M_impl);
        }

        constexpr allocator_type get_allocator() const noexcept {
            return allocator_type(get_T_allocator());
        }

        // constructor of vector_base
        constexpr vector_base() = default;

        constexpr vector_base(const allocator_type& alloc) noexcept : M_impl(alloc) {}

        constexpr vector_base(const size_t n) : M_impl() {M_create_storage(n);}

        constexpr vector_base(const size_t n, const allocator_type& alloc) : M_impl(alloc) {M_create_storage(n);}

        constexpr vector_base(vector_base&& other) = default;

        constexpr vector_base(T_alloc_type&& alloc) noexcept : M_impl(mystl::move(alloc)) {}

        constexpr vector_base(vector_base&& other, const allocator_type& alloc) : M_impl(alloc) {
            if (other.get_allocator() == alloc) {
                this -> M_impl._swap_data(other.M_impl);
            }else {
                const size_t n = other.M_impl._end - other.M_impl._begin;
                // only allocate the space here, and move the data form other to this in vector class
                M_create_storage(n);
            }
        }

        constexpr vector_base(const allocator_type& alloc, vector_base&& other)
                : M_impl(T_alloc_type(alloc), mystl::move(other.M_impl)) {}

        // destructor
        constexpr ~vector_base() noexcept {
            if (this -> M_impl._begin) {
                const ptrdiff_t n = this->M_impl._end_of_storage - this->M_impl._begin;
                M_deallocate(M_impl._begin, static_cast<size_t>(n));
            }
        }

    protected:
        constexpr void M_create_storage(size_t n) {
            this -> M_impl._begin = this->M_allocate(n);
            this -> M_impl._end = this->M_impl._begin;
            this -> M_impl._end_of_storage = this->M_impl._begin + n;
        }

    }; // struct vector_base


    // *************************************************************************************
    // standard container vector
    template <typename T, typename Allocator = mystl::allocator<T>>
    class vector : protected vector_base<T, Allocator> {

        static_assert(is_same_v<remove_cv_t<T>, T>,
                    "vector must have a non_const and non_volatile value type");
        static_assert(is_same_v<typename Allocator::value_type, T>,
                    "vector must have the same value type as its allocator");

    public:
        using Base              = vector_base<T, Allocator>;
        using T_alloc_type      = Base::T_alloc_type;
        using Allocator_traits  = allocator_traits<T_alloc_type>;

        using allocator_type    = Allocator;
        using value_type        = T;
        using size_type         = size_t;
        using difference_type   = ptrdiff_t;
        using reference         = T&;
        using const_reference   = const T&;
        using pointer           = allocator_traits<Allocator>::pointer;
        using const_pointer     = allocator_traits<Allocator>::const_pointer;

        using iterator          = T*;
        using const_iterator    = const T*;
        using reverse_iterator  = mystl::reverse_iterator<iterator>;
        using const_reverse_iterator = mystl::reverse_iterator<const_iterator>;

        constexpr allocator_type get_allocator() const noexcept {return Base::get_allocator();}

    protected:
        using Base::M_impl;
        using Base::M_allocate;
        using Base::M_deallocate;
        using Base::get_T_allocator;

        private:
#if defined(__clang__) || defined(__GNUC__)
    [[gnu::noinline]]
#endif
    [[noreturn]] static void throw_growth_length_error() {
        throw mystl::length_error(
            "cannot create vector larger than max_size()");
    }

    public:
        // *************************************************************************************
        // construct
        constexpr vector() = default;

        explicit constexpr vector(const allocator_type& alloc) noexcept : Base(alloc) {}

        explicit constexpr vector(size_type n, const allocator_type& alloc = allocator_type())
                : Base(check_init_len(n, alloc), alloc) {
            default_initialize(n);
        }

        constexpr vector(size_type n, const value_type& value, const allocator_type& alloc = allocator_type())
                : Base(check_init_len(n, alloc), alloc) {
            fill_initialized(n, value);
        }

        constexpr vector(const vector& other)
            : vector(other, Allocator_traits::select_on_container_copy_construction(other.get_T_allocator())) {}

        constexpr vector(vector&& other) noexcept = default;

        constexpr vector(const vector& other, const type_identity_t<allocator_type>& alloc)
            : Base(check_init_len(other.size(), alloc), alloc) {
            this -> M_impl._end = mystl::uninitialized_copy_a(other.begin(), other.end(),
                                                                this->M_impl._begin, get_T_allocator());
        }

    private:
        constexpr vector(vector&& other, const allocator_type& alloc, true_type) noexcept
            : Base(alloc, mystl::move(other)) {}

        constexpr vector(vector&& other, const allocator_type& alloc, false_type) : Base(alloc) {
            if (other.get_allocator() == alloc) {
                this -> M_impl._swap_data(other.M_impl);
            }else {
                this -> M_create_storage(check_init_len(other.size(), get_T_allocator()));
                this -> M_impl._end = mystl::uninitialized_move_a(other.begin(), other.end(),
                                                                this -> M_impl._begin, get_T_allocator() );
                other.clear();
            }
        }

    public:
        constexpr vector(vector&& other, const type_identity_t<allocator_type>& alloc)
        noexcept(Allocator_traits::is_always_equal::value)
            : vector(mystl::move(other), alloc, typename Allocator_traits::is_always_equal{}) {}

        constexpr vector(initializer_list<value_type> list, const allocator_type& alloc = allocator_type()) : Base(alloc) {
            range_initialized_n(list.begin(), list.end(), list.size());
        }

        template <typename Input_Iterator>
            requires mystl::is_input_iterator_v<Input_Iterator>
        constexpr vector(Input_Iterator first, Input_Iterator last, const allocator_type& alloc = allocator_type())
            : Base(alloc) {
            range_initialized(first, last, iter_category<Input_Iterator>{});
        }

        // *************************************************************************************
        // destruct
        constexpr ~vector() noexcept {
            mystl::destroy_a(this -> M_impl._begin, this -> M_impl._end, get_T_allocator());
        }

        // *************************************************************************************
        // operator = and assign
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
        
        constexpr vector& operator=(const vector& other) {
            if (this != &other) {
                if constexpr (Allocator_traits::propagate_on_container_copy_assignment::value) {
                    if (get_T_allocator() != other.get_T_allocator()) {
                        release_storage();
                    }
                    // Propagation is required even when the allocators compare equal.
                    get_T_allocator() = other.get_T_allocator();
                }
                assign(other.begin(), other.end());
            }
            return *this;
        }

        constexpr vector& operator=(vector&& other) 
        noexcept(Allocator_traits::propagate_on_container_move_assignment::value ||
            Allocator_traits::is_always_equal::value) 
        {
            if (this != &other) {
                move_assign(mystl::move(other), typename Allocator_traits::propagate_on_container_move_assignment{});
            }
            return *this;
        }


        // *************************************************************************************
        // element access
        // operator [] and at
        [[nodiscard]] constexpr reference operator[](size_type idx) noexcept {
            return *(this -> M_impl._begin + idx);
        }

        [[nodiscard]] constexpr const_reference operator[](size_type idx) const noexcept {
            return *(this -> M_impl._begin + idx);
        }

        [[nodiscard]] constexpr reference at(size_type idx) {
            range_check(idx);
            return *(this -> M_impl._begin + idx);
        }

        [[nodiscard]] constexpr const_reference at(size_type idx) const {
            range_check(idx);
            return *(this -> M_impl._begin + idx);
        }

        // front, back and data
        [[nodiscard]] constexpr reference front() noexcept {
            MYSTL_DEBUG(!empty());
            return *(this -> M_impl._begin);
        }

        [[nodiscard]] constexpr const_reference front() const noexcept {
            MYSTL_DEBUG(!empty());
            return *(this -> M_impl._begin);
        }

        [[nodiscard]] constexpr reference back() noexcept {
            MYSTL_DEBUG(!empty());
            return *(this -> M_impl._end - 1);
        }

        [[nodiscard]] constexpr const_reference back() const noexcept {
            MYSTL_DEBUG(!empty());
            return *(this -> M_impl._end - 1);
        }

        [[nodiscard]] constexpr T* data() noexcept {
            return data_ptr(this -> M_impl._begin);
        }

        [[nodiscard]] constexpr const T* data() const noexcept {
            return data_ptr(this -> M_impl._begin);
        }

        // *************************************************************************************
        // iterator
        [[nodiscard]] constexpr iterator begin() noexcept {
            return iterator(this -> M_impl._begin);
        }

        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return const_iterator(this -> M_impl._begin);
        }

        [[nodiscard]] constexpr iterator end() noexcept {
            return iterator(this -> M_impl._end);
        }

        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return const_iterator(this -> M_impl._end);
        }

        [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
            return const_iterator(this -> M_impl._begin);
        }

        [[nodiscard]] constexpr const_iterator cend() const noexcept {
            return const_iterator(this -> M_impl._end);
        }

        [[nodiscard]] constexpr reverse_iterator rbegin() noexcept {
            return reverse_iterator(this -> M_impl._end);
        }

        [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(this -> M_impl._end);
        }

        [[nodiscard]] constexpr reverse_iterator rend() noexcept {
            return reverse_iterator(this -> M_impl._begin);
        }

        [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(this -> M_impl._begin);
        }

        // *************************************************************************************
        // capacity of vector
        // empty
        [[nodiscard]] constexpr bool empty() const noexcept {
            return this -> M_impl._end == this -> M_impl._begin;
        }

        // size, return the number of element in vector
        [[nodiscard]] constexpr size_type size() const noexcept {
            if (this -> M_impl._begin) {
                const ptrdiff_t n = this->M_impl._end - this->M_impl._begin;
                return static_cast<size_type>(n);
            }
            return 0;
        }

        // max_size, return the maximum number of element that vector can contain
        [[nodiscard]] constexpr size_type max_size() const noexcept {
            return get_max_size(get_T_allocator());
        }

        // capacity
        [[nodiscard]] constexpr size_type capacity() const noexcept {
            if (this -> M_impl._begin) {
                return static_cast<size_type>(this->M_impl._end_of_storage - this->M_impl._begin);
            }
            return 0;
        }

        // reserve
        constexpr void reserve(size_type new_cap) {
            if (new_cap > max_size()) {
                THROW_LENGTH_ERROR_IF(true, "cannot create vector larger than max_size()");
            }
            if (new_cap > this -> capacity()) {
                reallocate(new_cap);
            }
        }

        // shrink_to_fit
        constexpr void shrink_to_fit() {
            if (this -> M_impl._end < this -> M_impl._end_of_storage) {
                const size_type old_size = size();
                pointer new_begin = reallocate(old_size);
                this -> M_impl._end = new_begin + old_size;
                this -> M_impl._end_of_storage = this -> M_impl._end;
            }
        }

    private:
        // guard for allocate and deallocate
        struct Guard_alloc {
            pointer _storage;
            size_type _len;
            Base& _vector;

            constexpr Guard_alloc(pointer ptr, const size_type len, Base& vec) : _storage(ptr), _len(len), _vector(vec) {}

            constexpr Guard_alloc(const Guard_alloc&) = delete;

            constexpr ~Guard_alloc() noexcept {
                if (_storage) _vector.M_deallocate(_storage, _len);
            }

            constexpr pointer release() noexcept {
                pointer res = _storage;
                _storage = pointer();
                return res;
            }
        };

        // guard for construct and destroy
        struct Guard_objects {
            T_alloc_type& _alloc;
            pointer _first;
            pointer _last;

            constexpr Guard_objects(T_alloc_type& alloc, pointer first) noexcept
                : _alloc(alloc), _first(first), _last(first) {}

            Guard_objects(const Guard_objects&) = delete;
            Guard_objects& operator=(const Guard_objects&) = delete;
            Guard_objects(Guard_objects&&) = delete;
            Guard_objects& operator=(Guard_objects&&) = delete;

            constexpr ~Guard_objects() noexcept {
                while (_last != _first) {
                    --_last;
                    Allocator_traits::destroy(_alloc, _last);
                }
            }

            template <typename... Args>
            constexpr void construct_next(Args&&... args) {
                Allocator_traits::construct(_alloc, _last, mystl::forward<Args>(args)...);
                ++_last;
            }

            constexpr void set_end(pointer last) noexcept {
                _last = last;
            }

            constexpr void release() noexcept {
                _first = _last;
            }
        };


    public:
        // *************************************************************************************
        // modifiers of vector
        // emplace_back
        template <typename... Args>
        constexpr reference emplace_back(Args&&... args) {
            if (this -> M_impl._end < this -> M_impl._end_of_storage) {
                Allocator_traits::construct(get_T_allocator(), this -> M_impl._end, mystl::forward<Args>(args)...);
                ++this -> M_impl._end;
                return back();
            }

            const auto old_cap = capacity();
            if (old_cap == max_size()) {
                throw_growth_length_error();
            }
            const size_type new_cap = old_cap == 0 ? 1 : (max_size() / 2 < old_cap ? max_size() : old_cap * 2);
            
            const size_type old_size = size();
            auto& alloc = get_T_allocator();

            Guard_alloc storage(M_allocate(new_cap), new_cap, *this);
            pointer new_begin = storage._storage;

            Guard_objects tail(alloc, new_begin + old_size);
            Guard_objects relocated(alloc, new_begin);

            tail.construct_next(mystl::forward<Args>(args)...);

            pointer relocated_end;
            if constexpr (is_nothrow_move_constructible_v<value_type> || !is_copy_constructible_v<value_type>) {
                relocated_end = mystl::uninitialized_move_a(this->M_impl._begin, this->M_impl._end, new_begin, alloc);
            } else {
                relocated_end = mystl::uninitialized_copy_a(this->M_impl._begin, this->M_impl._end, new_begin, alloc);
            }

            relocated.set_end(relocated_end);

            mystl::destroy_a(this->M_impl._begin, this->M_impl._end, alloc);
            M_deallocate(this->M_impl._begin, old_cap);

            this->M_impl._begin = new_begin;
            this->M_impl._end = new_begin + old_size + 1;
            this->M_impl._end_of_storage = new_begin + new_cap;

            relocated.release();
            tail.release();
            storage.release();

            return back();
        }

        // push_back
        constexpr void push_back(const value_type& value) {
            emplace_back(value);
        }

        constexpr void push_back(value_type&& value) {
            emplace_back(mystl::move(value));
        }

        // pop_back
        constexpr void pop_back() {
            MYSTL_DEBUG(!empty());
            --this -> M_impl._end;
            Allocator_traits::destroy(get_T_allocator(), this -> M_impl._end);
        }

        // resize, change the size of vector, if new size > old size, then fill the new element with value
        constexpr void resize(size_type new_size) {
            if (new_size > max_size()) {
                throw_growth_length_error();
            }
            if (new_size <= size()) {
                auto new_end = this -> M_impl._begin + new_size;
                mystl::destroy_a(new_end, this -> M_impl._end, get_T_allocator());
                this -> M_impl._end = new_end;
            }else if(new_size > size() && new_size <= capacity()) {
                auto new_end = this -> M_impl._begin + new_size;
                mystl::uninitialized_default_construct_a(this -> M_impl._end, new_end, get_T_allocator());
                this -> M_impl._end = new_end;
            }else {
                const size_type old_size = size();
                const size_type old_cap = capacity();
                const size_type new_cap = min(max_size(), max(old_cap * 2, new_size));
                auto& alloc = get_T_allocator();

                Guard_alloc storage(M_allocate(new_cap), new_cap, *this);
                pointer new_begin = storage._storage;

                Guard_objects tail(alloc, new_begin + old_size);
                Guard_objects relocated(alloc, new_begin);

                mystl::uninitialized_default_construct_a(tail._first, new_begin + new_size, alloc);
                tail.set_end(new_begin + new_size);

                pointer relocated_end;
                if constexpr (is_nothrow_move_constructible_v<value_type> || !is_copy_constructible_v<value_type>) {
                    relocated_end = mystl::uninitialized_move_a(this->M_impl._begin, this->M_impl._end, new_begin, alloc);
                } else {
                    relocated_end = mystl::uninitialized_copy_a(this->M_impl._begin, this->M_impl._end, new_begin, alloc);
                }
                relocated.set_end(relocated_end);

                mystl::destroy_a(this->M_impl._begin, this->M_impl._end, alloc);
                M_deallocate(this->M_impl._begin, old_cap);

                this->M_impl._begin = new_begin;
                this->M_impl._end = new_begin + new_size;
                this->M_impl._end_of_storage = new_begin + new_cap;

                relocated.release();
                tail.release();
                storage.release();
            }
        }

        // clear
        constexpr void clear() noexcept {
            mystl::destroy_a(this -> M_impl._begin, this -> M_impl._end, get_T_allocator());
            this -> M_impl._end = this -> M_impl._begin;
        }

        // emplace, construct a value before the position pos
        template <typename... Args>
        constexpr iterator emplace(const_iterator pos, Args&&... args) {
            MYSTL_DEBUG(pos >= begin() && pos <= end());
            const size_type n = pos - cbegin();
            if (this -> M_impl._end < this -> M_impl._end_of_storage) {
                if (pos == cend()) {
                    Allocator_traits::construct(get_T_allocator(), this -> M_impl._end, mystl::forward<Args>(args)...);
                    ++this -> M_impl._end;
                    return this -> M_impl._end - 1;
                }
                value_type insert_obj(mystl::forward<Args>(args)...);

                pointer insert_pos = this -> M_impl._begin + n;
                if constexpr (is_nothrow_move_constructible_v<value_type> || !is_copy_constructible_v<value_type>) {
                    Allocator_traits::construct(get_T_allocator(), this -> M_impl._end, mystl::move(*(this -> M_impl._end - 1)));
                }else{
                    Allocator_traits::construct(get_T_allocator(), this -> M_impl._end, *(this -> M_impl._end - 1));
                }
                ++this -> M_impl._end;
                if constexpr (is_nothrow_move_assignable_v<value_type> || !is_copy_assignable_v<value_type>) {
                    mystl::move_backward(insert_pos, this -> M_impl._end - 2, this -> M_impl._end - 1);
                } else {
                    mystl::copy_backward(insert_pos, this -> M_impl._end - 2, this -> M_impl._end - 1);
                }
                
                if constexpr (is_nothrow_move_assignable_v<value_type> || !is_copy_assignable_v<value_type>) {
                    *insert_pos = mystl::move(insert_obj);
                } else {
                    *insert_pos = insert_obj;
                }
                return insert_pos;
            }else {
                return reallocate_and_insert(n, 1, mystl::forward<Args>(args)...);
            }
        }

        // insert, insert a value before the position pos
        constexpr iterator insert(const_iterator pos, const value_type& value) {
            MYSTL_DEBUG(pos >= begin() && pos <= end());
            const size_type n = pos - cbegin();
            if (this -> M_impl._end < this -> M_impl._end_of_storage) {
                if (pos == cend()) {
                    Allocator_traits::construct(get_T_allocator(), this -> M_impl._end, value);
                    ++this -> M_impl._end;
                    return this -> M_impl._end - 1;
                }
                value_type insert_obj(value);

                pointer insert_pos = this -> M_impl._begin + n;
                if constexpr (is_nothrow_move_constructible_v<value_type> || !is_copy_constructible_v<value_type>) {
                    Allocator_traits::construct(get_T_allocator(), this -> M_impl._end, mystl::move(*(this -> M_impl._end - 1)));
                }else{
                    Allocator_traits::construct(get_T_allocator(), this -> M_impl._end, *(this -> M_impl._end - 1));
                }
                ++this -> M_impl._end;
                if constexpr (is_nothrow_move_assignable_v<value_type> || !is_copy_assignable_v<value_type>) {
                    mystl::move_backward(insert_pos, this -> M_impl._end - 2, this -> M_impl._end - 1);
                } else {
                    mystl::copy_backward(insert_pos, this -> M_impl._end - 2, this -> M_impl._end - 1);
                }

                if constexpr (is_nothrow_move_assignable_v<value_type> || !is_copy_assignable_v<value_type>) {
                    *insert_pos = mystl::move(insert_obj);
                } else {
                    *insert_pos = insert_obj;
                }
                return insert_pos;
            }else {
                return reallocate_and_insert(n, 1, value);
            }
        }

        constexpr iterator insert(const_iterator pos, value_type&& value) {
            MYSTL_DEBUG(pos >= begin() && pos <= end());
            const size_type n = pos - cbegin();
            if (this -> M_impl._end < this -> M_impl._end_of_storage) {
                if (pos == cend()) {
                    Allocator_traits::construct(get_T_allocator(), this -> M_impl._end, mystl::move(value));
                    ++this -> M_impl._end;
                    return this -> M_impl._end - 1;
                }
                value_type insert_obj(mystl::move(value));

                pointer insert_pos = this -> M_impl._begin + n;
                if constexpr (is_nothrow_move_constructible_v<value_type> || !is_copy_constructible_v<value_type>) {
                    Allocator_traits::construct(get_T_allocator(), this -> M_impl._end, mystl::move(*(this -> M_impl._end - 1)));
                }else{
                    Allocator_traits::construct(get_T_allocator(), this -> M_impl._end, *(this -> M_impl._end - 1));
                }
                ++this -> M_impl._end;
                if constexpr (is_nothrow_move_assignable_v<value_type> || !is_copy_assignable_v<value_type>){
                    mystl::move_backward(insert_pos, this -> M_impl._end - 2, this -> M_impl._end - 1);
                } else {
                    mystl::copy_backward(insert_pos, this -> M_impl._end - 2, this -> M_impl._end - 1);
                }

                if constexpr (is_nothrow_move_assignable_v<value_type> || !is_copy_assignable_v<value_type>) {
                    *insert_pos = mystl::move(insert_obj);
                } else {
                    *insert_pos = insert_obj;
                }
                return insert_pos;
            }else {
                return reallocate_and_insert(n, 1, mystl::move(value));
            }
        }

        constexpr iterator insert(const_iterator pos, size_type count, const value_type& value) {
            MYSTL_DEBUG(pos >= begin() && pos <= end());
            const size_type n = pos - cbegin();
            if (count == 0) return this -> M_impl._begin + n;
            if (count > max_size() - size()) {
                throw_growth_length_error();
            }
            if (count <= capacity() - size()) {
                value_type insert_obj(value);

                pointer insert_pos = this -> M_impl._begin + n;
                pointer old_end = this -> M_impl._end;
                const size_type after_elems = this -> M_impl._end - insert_pos;
                if (after_elems > count) {
                    if constexpr (is_nothrow_move_constructible_v<value_type> || !is_copy_constructible_v<value_type>) {
                        mystl::uninitialized_move_a(this -> M_impl._end - count, this -> M_impl._end,
                                                this -> M_impl._end, get_T_allocator());
                    }else {
                        mystl::uninitialized_copy_a(this -> M_impl._end - count, this -> M_impl._end,
                                                this -> M_impl._end, get_T_allocator());
                    }
                    this -> M_impl._end += count;
                    if constexpr (is_nothrow_move_assignable_v<value_type> || !is_copy_assignable_v<value_type>) {
                        mystl::move_backward(insert_pos, old_end - count, old_end);
                    } else {
                        mystl::copy_backward(insert_pos, old_end - count, old_end);
                    }
                    mystl::fill_n(insert_pos, count, insert_obj);
                }else {
                    Guard_objects appended(get_T_allocator(), old_end);
                    pointer extra_end = mystl::uninitialized_fill_n_a(old_end, count - after_elems, insert_obj, get_T_allocator());
                    appended.set_end(extra_end);
                    pointer new_end;
                    if constexpr (is_nothrow_move_constructible_v<value_type> || !is_copy_constructible_v<value_type>) {
                        new_end = mystl::uninitialized_move_a(insert_pos, old_end, extra_end, get_T_allocator());
                    }else{
                        new_end = mystl::uninitialized_copy_a(insert_pos, old_end, extra_end, get_T_allocator());
                    }
                    appended.set_end(new_end);
                    this -> M_impl._end = new_end;
                    appended.release();
                    mystl::fill_n(insert_pos, after_elems, insert_obj);
                }
                return insert_pos;
            }else {
                return reallocate_and_insert(n, count, value);
            }
        }

    private:
        template <typename Input_Iterator>
            requires mystl::is_input_iterator_v<Input_Iterator>
        constexpr iterator range_insert(const_iterator pos, Input_Iterator first, Input_Iterator last, forward_iterator_tag) {
            MYSTL_DEBUG(pos >= begin() && pos <= end());
            const size_type n = pos - cbegin();
            if (first == last) return this -> M_impl._begin + n;

            const size_type count = mystl::distance(first, last);
            if (count > max_size() - size()) {
                throw_growth_length_error();
            }
            if (count <= capacity() - size()) {
                pointer insert_pos = this -> M_impl._begin + n;
                pointer old_end = this -> M_impl._end;
                const size_type after_elems = this -> M_impl._end - insert_pos;
                if (after_elems > count) {
                    if constexpr (is_nothrow_move_constructible_v<value_type> || !is_copy_constructible_v<value_type>) {
                        mystl::uninitialized_move_a(this -> M_impl._end - count, this -> M_impl._end,
                                                this -> M_impl._end, get_T_allocator());
                    }else {
                        mystl::uninitialized_copy_a(this -> M_impl._end - count, this -> M_impl._end,
                                                this -> M_impl._end, get_T_allocator());
                    }
                    this -> M_impl._end += count;
                    if constexpr (is_nothrow_move_assignable_v<value_type> || !is_copy_assignable_v<value_type>) {
                        mystl::move_backward(insert_pos, old_end - count, old_end);
                    } else {
                        mystl::copy_backward(insert_pos, old_end - count, old_end);
                    }
                    mystl::copy(first, last, insert_pos);
                }else {
                    Guard_objects appended(get_T_allocator(), old_end);
                    auto mid = first;
                    mystl::advance(mid, after_elems);
                    pointer extra_end = mystl::uninitialized_copy_a(mid, last, old_end, get_T_allocator());
                    appended.set_end(extra_end);
                    pointer new_end;
                    if constexpr (is_nothrow_move_constructible_v<value_type> || !is_copy_constructible_v<value_type>) {
                        new_end = mystl::uninitialized_move_a(insert_pos, old_end, extra_end, get_T_allocator());
                    }else{
                        new_end = mystl::uninitialized_copy_a(insert_pos, old_end, extra_end, get_T_allocator());
                    }
                    appended.set_end(new_end);
                    this -> M_impl._end = new_end;
                    appended.release();
                    mystl::copy(first, mid, insert_pos);
                }
                return insert_pos;
            }else {
                return range_reallocate_and_insert(n, first, last);
            }
        }

        template <typename Input_Iterator>
            requires mystl::is_input_iterator_v<Input_Iterator>
        constexpr iterator range_insert(const_iterator pos, Input_Iterator first, Input_Iterator last, input_iterator_tag) {
            MYSTL_DEBUG(pos >= begin() && pos <= end());
            const size_type n = pos - cbegin();
            if (first == last) return this -> M_impl._begin + n;

            vector temp(first, last, get_T_allocator());
            return range_insert(pos, temp.begin(), temp.end(), forward_iterator_tag{});
        }     

    public:
        template <typename Input_Iterator>
            requires mystl::is_input_iterator_v<Input_Iterator>
        constexpr iterator insert(const_iterator pos, Input_Iterator first, Input_Iterator last){
            return range_insert(pos, first, last, iter_category<Input_Iterator>{});
        }

        constexpr iterator insert(const_iterator pos, initializer_list<value_type> list) {
            return insert(pos, list.begin(), list.end());
        }

        // erase, erase the element at position pos
        constexpr iterator erase(const_iterator pos) {
            MYSTL_DEBUG(pos >= begin() && pos < end());
            pointer erase_pos = this -> M_impl._begin + (pos - cbegin());
            if (erase_pos + 1 != this -> M_impl._end) {
                if constexpr (is_nothrow_move_assignable_v<value_type> || !is_copy_assignable_v<value_type>) {
                    mystl::move(erase_pos + 1, this -> M_impl._end, erase_pos);
                } else {
                    mystl::copy(erase_pos + 1, this -> M_impl._end, erase_pos);
                }
            }
            --this -> M_impl._end;
            Allocator_traits::destroy(get_T_allocator(), this -> M_impl._end);
            return erase_pos;
        }

        constexpr iterator erase(const_iterator first, const_iterator last) {
            MYSTL_DEBUG(first >= begin() && first <= end() && last >= begin() && last <= end() && first <= last);
            if (first == last) return const_cast<iterator>(first);
            pointer erase_first = this -> M_impl._begin + (first - cbegin());
            pointer erase_last = this -> M_impl._begin + (last - cbegin());
            pointer new_end = this -> M_impl._end - (erase_last - erase_first);
            if constexpr (is_nothrow_move_assignable_v<value_type> || !is_copy_assignable_v<value_type>) {
                mystl::move(erase_last, this -> M_impl._end, erase_first);
            } else {
                mystl::copy(erase_last, this -> M_impl._end, erase_first);
            }
            mystl::destroy_a(new_end, this -> M_impl._end, get_T_allocator());
            this -> M_impl._end = new_end;
            return erase_first;
        }

        // swap
        constexpr void swap(vector& other) noexcept {
            if (this == &other) return;
            if constexpr (Allocator_traits::propagate_on_container_swap::value) {
                using mystl::swap;
                swap(get_T_allocator(), other.get_T_allocator());
            }else {
                // As with std::vector, non-propagating allocators must compare equal.
                MYSTL_DEBUG(get_T_allocator() == other.get_T_allocator());
            }
            this -> M_impl._swap_data(other.M_impl);
        }


    private:
        // reallocate a new space and copy the data to there, sizeof(new space) = 2 * sizeof(old space)
        constexpr pointer reallocate() {
            const auto old_cap = static_cast<size_type>(this->M_impl._end_of_storage - this->M_impl._begin);
            if (old_cap == max_size()) {
                throw_growth_length_error();
            }
            const size_type new_cap = old_cap == 0 ? 1 : (max_size() / 2 < old_cap ? max_size() : old_cap * 2);
            return reallocate(new_cap);
        }

        constexpr pointer reallocate(const size_type new_cap) {
            pointer new_begin = allocate_and_copy(new_cap, this -> M_impl._begin, this -> M_impl._end);
            pointer new_end = new_begin + size();
            pointer new_end_of_storage = new_begin + new_cap;
            
            mystl::destroy_a(this -> M_impl._begin, this -> M_impl._end, get_T_allocator());
            const auto old_cap = static_cast<size_type>(this->M_impl._end_of_storage - this->M_impl._begin);
            M_deallocate(this -> M_impl._begin, old_cap);

            this -> M_impl._begin = new_begin;
            this -> M_impl._end = new_end;
            this -> M_impl._end_of_storage = new_end_of_storage;
            return new_begin;
        }

        template <typename... Args>
        constexpr pointer reallocate_and_insert(const size_type n, const size_type count, Args&&... args) {
            if(count == 0) return this -> M_impl._begin + n;
            if (count > max_size() - size()) {
                throw_growth_length_error();
            }
            const size_type old_size = size();
            const size_type old_cap = capacity();
            const size_type new_cap = min(max_size(), max(old_cap * 2, old_size + count));
            auto& alloc = get_T_allocator();

            Guard_alloc storage(this -> M_allocate(new_cap), new_cap, *this);
            pointer new_begin = storage._storage;
            pointer new_insert_pos = storage._storage + n;

            Guard_objects prefix(alloc, new_begin);
            Guard_objects new_constructed(alloc, new_insert_pos);
            Guard_objects suffix(alloc, new_insert_pos + count);

            for (size_type i = 0; i < count; ++i) {
                new_constructed.construct_next(mystl::forward<Args>(args)...);
            }
            
            pointer prefix_end;
            pointer suffix_end;
            if constexpr (is_nothrow_move_constructible_v<value_type> || !is_copy_constructible_v<value_type>) {
                prefix_end = mystl::uninitialized_move_a(this -> M_impl._begin, this -> M_impl._begin + n, new_begin, alloc);
                prefix.set_end(prefix_end);
                suffix_end = mystl::uninitialized_move_a(this -> M_impl._begin + n, this -> M_impl._end, new_insert_pos + count, alloc);
            }else {
                prefix_end = mystl::uninitialized_copy_a(this -> M_impl._begin, this -> M_impl._begin + n, new_begin, alloc);
                prefix.set_end(prefix_end);
                suffix_end = mystl::uninitialized_copy_a(this -> M_impl._begin + n, this -> M_impl._end, new_insert_pos + count, alloc);
            }
            suffix.set_end(suffix_end);

            mystl::destroy_a(this -> M_impl._begin, this -> M_impl._end, alloc);
            M_deallocate(this -> M_impl._begin, old_cap);

            this -> M_impl._begin = new_begin;
            this -> M_impl._end = suffix_end;
            this -> M_impl._end_of_storage = new_begin + new_cap;

            prefix.release();
            new_constructed.release();
            suffix.release();
            storage.release();
            
            return new_insert_pos;
        }

        template <typename Input_Iterator>
            requires mystl::is_input_iterator_v<Input_Iterator>
        constexpr pointer range_reallocate_and_insert(const size_type n, Input_Iterator first, Input_Iterator last) {
            const size_type count = mystl::distance(first, last);
            if (count == 0) return this -> M_impl._begin + n;
            if (count > max_size() - size()) {
                throw_growth_length_error();
            }
            const size_type old_size = size();
            const size_type old_cap = capacity();
            const size_type new_cap = min(max_size(), max(old_cap * 2, old_size + count));
            auto& alloc = get_T_allocator();

            Guard_alloc storage(this -> M_allocate(new_cap), new_cap, *this);
            pointer new_begin = storage._storage;
            pointer new_insert_pos = storage._storage + n;

            Guard_objects prefix(alloc, new_begin);
            Guard_objects new_constructed(alloc, new_insert_pos);
            Guard_objects suffix(alloc, new_insert_pos + count);

            mystl::uninitialized_copy_a(first, last, new_insert_pos, alloc);
            new_constructed.set_end(new_insert_pos + count);

            pointer prefix_end;
            pointer suffix_end;
            if constexpr (is_nothrow_move_constructible_v<value_type> || !is_copy_constructible_v<value_type>) {
                prefix_end = mystl::uninitialized_move_a(this -> M_impl._begin, this -> M_impl._begin + n, new_begin, alloc);
                prefix.set_end(prefix_end);
                suffix_end = mystl::uninitialized_move_a(this -> M_impl._begin + n, this -> M_impl._end, new_insert_pos + count, alloc);
            }else {
                prefix_end = mystl::uninitialized_copy_a(this -> M_impl._begin, this -> M_impl._begin + n, new_begin, alloc);
                prefix.set_end(prefix_end);
                suffix_end = mystl::uninitialized_copy_a(this -> M_impl._begin + n, this -> M_impl._end, new_insert_pos + count, alloc);
            }
            suffix.set_end(suffix_end);

            mystl::destroy_a(this -> M_impl._begin, this -> M_impl._end, alloc);
            M_deallocate(this -> M_impl._begin, old_cap);

            this -> M_impl._begin = new_begin;
            this -> M_impl._end = suffix_end;
            this -> M_impl._end_of_storage = new_begin + new_cap;

            prefix.release();
            new_constructed.release();
            suffix.release();
            storage.release();
            return new_insert_pos;
        }

    protected:
        template <typename Forward_Iterator>
        constexpr pointer allocate_and_copy(const size_type n, Forward_Iterator first, Forward_Iterator last) {
            Guard_alloc guard(this -> M_allocate(n), n, *this);
            if constexpr (is_nothrow_move_constructible_v<value_type> || !is_copy_constructible_v<value_type>) {
                mystl::uninitialized_move_a(first, last, guard._storage, get_T_allocator());
            }else {
                mystl::uninitialized_copy_a(first, last, guard._storage, get_T_allocator());
            }
            return guard.release();
        }



        constexpr void default_initialize(const size_type n) {
            this -> M_impl._end = uninitialized_default_construct_n_a(this -> M_impl._begin, n, get_T_allocator());
        }

        constexpr void fill_initialized(size_type n, const value_type& value) {
            this -> M_impl._end = uninitialized_fill_n_a(this -> M_impl._begin, n, value, get_T_allocator());
        }

        template <typename Iterator>
        constexpr void range_initialized_n(Iterator first, Iterator last, size_type n) {
            this -> M_impl._begin = this -> M_allocate(check_init_len(n, get_T_allocator()));
            this -> M_impl._end_of_storage = this -> M_impl._begin + n;
            this -> M_impl._end = mystl::uninitialized_copy_a(mystl::move(first),
                                        last, this -> M_impl._begin, get_T_allocator());
        }

        // range_initialized for two type of iterator
        template <typename Input_Iterator>
        constexpr void range_initialized(Input_Iterator first, Input_Iterator last, input_iterator_tag) {
            try {
                for (; first != last; ++first) {
                    emplace_back(*first);
                }
            }catch (...) {
                clear();
                throw;
            }
        }

        template <typename Forward_Iterator>
        constexpr void range_initialized(Forward_Iterator first, Forward_Iterator last, forward_iterator_tag) {
            range_initialized_n(first, last, mystl::distance(first, last));
        }

    private:
        constexpr size_type check_len(const size_type n, const char* s) const {
            THROW_LENGTH_ERROR_IF(max_size() - size() < n, s);
            const size_type len = size() + max(size(), n);
            return (len < size() || len > max_size()) ? max_size() : len;
        }

        static constexpr size_type check_init_len(const size_type n, const T_alloc_type& alloc) {
            THROW_LENGTH_ERROR_IF(n > get_max_size(alloc), "cannot create vector larger than max_size()");
            return n;
        }

        static constexpr size_type get_max_size(const T_alloc_type& alloc) noexcept {
            const size_t diff_max = std::numeric_limits<ptrdiff_t>::max() / sizeof(T);
            const size_t alloc_max = Allocator_traits::max_size(alloc);
            return min(diff_max, alloc_max);
        }

        constexpr void range_check(size_type idx) const {
            THROW_OUT_OF_RANGE_IF(idx >= this -> size(), "index out of range");
        }

        template <typename U>
        constexpr U* data_ptr(U* ptr) const noexcept {
            return ptr;
        }

        template <typename Pointer>
        constexpr typename pointer_traits<Pointer>::element_type* data_ptr(Pointer ptr) const noexcept {
            return empty() ? nullptr : mystl::to_address(ptr);
        }

        constexpr void fill_assign(size_type count, const value_type& value) {
            if (count > capacity()) {
                vector tmp(count, value, get_T_allocator());
                this -> M_impl._swap_data(tmp.M_impl);
            }else if (count > size()) {
                this -> M_impl._end = mystl::uninitialized_fill_n_a(this -> M_impl._end, count - size(), value, get_T_allocator());
                mystl::fill(this -> M_impl._begin, this -> M_impl._end, value);
            }else {
                auto new_end = mystl::fill_n(this -> M_impl._begin, count, value);
                mystl::destroy_a(new_end, this -> M_impl._end, get_T_allocator());
                this -> M_impl._end = new_end;
            }
        }

        template <typename Input_Iterator>
        constexpr void range_assign(Input_Iterator first, Input_Iterator last, input_iterator_tag) {
            clear();
            for (; first != last; ++first) {
                emplace_back(*first);
            }
        }

        template <typename Forward_Iterator>
        constexpr void range_assign(Forward_Iterator first, Forward_Iterator last, forward_iterator_tag) {
            const size_type n = mystl::distance(first, last);
            range_assign(first, last, n);
        }

        template <typename Forward_Iterator>
        constexpr void range_assign(Forward_Iterator first, Forward_Iterator last, size_type n) {
            if (n == 0) {
                clear();
                return;
            }
            if (n > capacity()) {
                vector tmp(first, last, get_T_allocator());
                this -> M_impl._swap_data(tmp.M_impl);
            }else if (n > size()) {
                auto mid = first;
                mystl::advance(mid, size());
                mystl::copy(first, mid, this -> M_impl._begin);
                this -> M_impl._end = mystl::uninitialized_copy_a(mid, last, this -> M_impl._end, get_T_allocator());
            }else {
                auto new_end = mystl::copy(first, last, this -> M_impl._begin);
                mystl::destroy_a(new_end, this -> M_impl._end, get_T_allocator());
                this -> M_impl._end = new_end;
            }
        }

        template <typename Input_Iterator>
        constexpr void range_move_assign(Input_Iterator first, Input_Iterator last, input_iterator_tag) {
            clear();
            for (; first != last; ++first) {
                emplace_back(mystl::move(*first));
            }
        }

        template <typename Forward_Iterator>
        constexpr void range_move_assign(Forward_Iterator first, Forward_Iterator last, forward_iterator_tag) {
            const size_type n = mystl::distance(first, last);
            range_move_assign(first, last, n);
        }

        template <typename Forward_Iterator>
        constexpr void range_move_assign(Forward_Iterator first, Forward_Iterator last, size_type n) {
            check_init_len(n, get_T_allocator());
            if (n == 0) {
                clear();
                return;
            }
            if (n > capacity()) {
                Guard_alloc storage(this -> M_allocate(n), n, *this);
                mystl::uninitialized_move_a(first, last, storage._storage, get_T_allocator());

                mystl::destroy_a(this -> M_impl._begin, this -> M_impl._end, get_T_allocator());
                this -> M_deallocate(this -> M_impl._begin, capacity());

                this -> M_impl._begin = storage.release();
                this -> M_impl._end = this -> M_impl._begin + n;
                this -> M_impl._end_of_storage = this -> M_impl._begin + n;
            }else if (n > size()) {
                auto mid = first;
                mystl::advance(mid, size());
                mystl::move(first, mid, this -> M_impl._begin);
                this -> M_impl._end = mystl::uninitialized_move_a(mid, last, this -> M_impl._end, get_T_allocator());
            }else {
                auto new_end = mystl::move(first, last, this -> M_impl._begin);
                mystl::destroy_a(new_end, this -> M_impl._end, get_T_allocator());
                this -> M_impl._end = new_end;
            }
        }

        constexpr void release_storage() noexcept {
            const size_type old_cap = capacity();
            clear();
            M_deallocate(this -> M_impl._begin, old_cap);
            this -> M_impl._begin = this -> M_impl._end = this -> M_impl._end_of_storage = pointer();
        }

        constexpr void move_assign(vector&& other, true_type) {
            release_storage();
            get_T_allocator() = mystl::move(other.get_T_allocator());
            this -> M_impl._swap_data(other.M_impl);
        }

        constexpr void move_assign(vector&& other, false_type) {
            if (get_T_allocator() == other.get_T_allocator()) {
                release_storage();
                this -> M_impl._swap_data(other.M_impl);
            }else {
                range_move_assign(other.begin(), other.end(), other.size());
                other.clear();
            }
        }

    }; // struct vector

    template <typename T, typename Alloc>
    constexpr void swap(vector<T, Alloc>& lhs, vector<T, Alloc>& rhs) 
        noexcept(noexcept(lhs.swap(rhs))) 
    {
        lhs.swap(rhs);
    }

    template <typename T, typename Alloc>
    constexpr bool operator==(const vector<T, Alloc>& lhs, const vector<T, Alloc>& rhs) {
        return lhs.size() == rhs.size() && mystl::equal(lhs.begin(), lhs.end(), rhs.begin());
    }

#undef MYSTL_DEBUG
#undef THROW_LENGTH_ERROR_IF
#undef THROW_OUT_OF_RANGE_IF
#undef THROW_RUNTIME_ERROR_IF
} // namespace mystl

#endif //VECTOR_H
