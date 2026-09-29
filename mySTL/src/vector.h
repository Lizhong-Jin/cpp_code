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
#include "impl/basic_string.h"
#include "impl/basic_string.h"

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

        constexpr allocator_type get_allocator() const noexcept {return allocator_type();}

    protected:
        using Base::M_impl;
        using Base::M_allocate;
        using Base::M_deallocate;
        using Base::get_T_allocator;

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

        constexpr vector(const vector& other) : Base(other.size(), other.get_T_allocator()) {
            this -> M_impl._end = mystl::uninitialized_copy_a(other.begin(), other.end(),
                                                                this->M_impl._begin, get_T_allocator());
        }

        constexpr vector(vector&& other) noexcept = default;

        constexpr vector(const vector& other, const type_identity_t<allocator_type>& alloc)
            : Base(other.size(), alloc) {
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
                this -> M_create_storage(other.size());
                this -> M_impl._end = mystl::uninitialized_move_a(other.begin(), other.end(),
                                                                this -> M_impl._begin, get_T_allocator() );
                other.clear();
            }
        }

    public:
        constexpr vector(vector&& other, const type_identity_t<allocator_type>& alloc)
        noexcept(noexcept(vector(declval<vector&&>(), declval<const allocator_type&>(),
            declval<>(Allocator_traits::is_always_equal))))
            : vector(mystl::move(other), alloc, Allocator_traits::is_always_equal) {}

        constexpr vector(initializer_list<value_type> list, const allocator_type& alloc = allocator_type()) : Base(alloc) {
            range_initialized_n(list.begin(), list.end(), list.size());
        }

        template <typename Input_Iterator>
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
        constexpr vector& operator=(const vector& other) {}


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
            return reverse_iterator(this -> M_impl._begin);
        }

        [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(this -> M_impl._begin);
        }

        [[nodiscard]] constexpr reverse_iterator rend() noexcept {
            return reverse_iterator(this -> M_impl._end);
        }

        [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(this -> M_impl._end);
        }


        // *************************************************************************************
        // emplace_back
        template <typename... Args>
        constexpr reference emplace_back(Args&&... args) {
            if (this -> M_impl._end == this -> M_impl._end_of_storage) {
                reallocate();
            }
            Allocator_traits::construct(get_T_allocator(), this -> M_impl._end, mystl::forward<Args>(args)...);
            ++this -> M_impl._end;
            return back();
        }

        // push_back


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
                const ptrdiff_t n = this->M_impl._end_of_storage - this->M_impl._begin;
                return static_cast<size_type>(n);
            }
            return 0;
        }

        // reserve
        constexpr void reserve(size_type new_cap) {
            if (new_cap > this -> capacity()) {
                reallocate(new_cap);
            }
        }

    private:
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

        // reallocate a new space and copy the data to there, sizeof(new space) = 2 * sizeof(old space)
        constexpr pointer reallocate() {
            const auto old_cap = static_cast<size_type>(this->M_impl._end_of_storage - this->M_impl._begin);
            const size_type new_cap = old_cap == 0 ? 1 : (max_size() / 2 < old_cap ? max_size() : old_cap * 2);
            return reallocate(new_cap);
        }

        constexpr pointer reallocate(const size_type new_cap) {
            pointer new_begin = allocate_and_copy(new_cap, this -> M_impl._begin, this -> M_impl._end);
            pointer new_end = new_begin + size();
            pointer new_end_of_storage = new_begin + new_cap;
            
            const auto old_cap = static_cast<size_type>(this->M_impl._end_of_storage - this->M_impl._begin);
            M_deallocate(this -> M_impl._begin, old_cap);

            this -> M_impl._begin = new_begin;
            this -> M_impl._end = new_end;
            this -> M_impl._end_of_storage = new_end_of_storage;
            return new_begin;
        }

    protected:
        template <typename Forward_Iterator>
        constexpr pointer allocate_and_copy(const size_type n, Forward_Iterator first, Forward_Iterator last) {
            Guard_alloc guard(this -> M_allocate(n), n, *this);
            mystl::uninitialized_copy_a(first, last, guard._storage, get_T_allocator());
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
            this -> M_impl._begin = this -> M_allocate(check_init_len(n));
            this -> M_impl._end_of_storage = this -> M_impl._begin + n;
            this -> M_impl._end = uninitialized_copy_a(mystl::move(first),
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
            range_initialized_n(first, last, distance(first, last));
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

        constexpr void move_assign(vector&& other, true_type) {
            this -> M_impl._swap_data(other.M_impl);
        }

        constexpr void move_assign(vector&& other, false_type) {
            if (this -> get_T_allocator() == other.get_T_allocator()) {
                move_assign(move(other), true_type());
            }else {
                
            }
        }

    }; // struct vector

#undef MYSTL_DEBUG
#undef THROW_LENGTH_ERROR_IF
#undef THROW_OUT_OF_RANGE_IF
#undef THROW_RUNTIME_ERROR_IF
} // namespace mystl

#endif //VECTOR_H
