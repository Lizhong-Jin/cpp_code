#pragma once

#ifndef MEMORY_H
#define MEMORY_H

#include "impl/type_traits_normal.h"
#include "impl/construct.h"
#include "impl/allocator.h"
#include "impl/uninitialized.h"
#include "impl/pointer_traits.h"

namespace mystl {
    // default_delete
    template <typename T>
    struct default_delete {
        constexpr default_delete() noexcept = default;
        template <typename U>
        explicit default_delete(const default_delete<U>&) noexcept {}
        void operator()(const T* ptr) noexcept {
            delete ptr;
        }
    };

    template <typename T>
    struct default_delete<T[]> {
        constexpr default_delete() noexcept = default;
        template <typename U>
        explicit default_delete(const default_delete<U[]>&) noexcept {}
        void operator()(const T* ptr) noexcept {
            delete[] ptr;
        }
    };

    // detect Deleter::pointer
    // has_pointer_typedef
    template <typename Deleter>
    struct has_pointer_typedef {
    private:
        typedef char yes_type;
        struct no_type {char dummy[2];};
        template <typename U>
        static yes_type test(typename Deleter::pointer*);
        template <typename U>
        static no_type test(...);
    public:
        static constexpr bool value = sizeof(test<Deleter>(nullptr)) == sizeof(yes_type);
    };

    template <typename Deleter> static constexpr bool has_pointer_typedef_v = has_pointer_typedef<Deleter>::value;

    // delete_storage
    template <typename Deleter, bool B=is_empty_v<Deleter> && !is_final_v<Deleter> && !is_reference_v<Deleter>>
    struct delete_storage;

    template <typename Deleter>
    struct delete_storage<Deleter, true> : private Deleter {
        constexpr delete_storage() noexcept : Deleter() {}
        explicit constexpr delete_storage(const Deleter& d) noexcept : Deleter(d) {}
        explicit constexpr delete_storage(Deleter&& d) noexcept : Deleter(mystl::move(d)) {}
        Deleter& get() noexcept {return *this;}
        const Deleter& get() const noexcept {return *this;}
    };

    template <typename Deleter>
    struct delete_storage<Deleter, false> {
        Deleter _deleter_;
        constexpr delete_storage() noexcept : _deleter_() {}
        explicit constexpr delete_storage(const Deleter& d) noexcept : _deleter_(d) {}
        explicit constexpr delete_storage(Deleter&& d) noexcept : _deleter_(mystl::move(d)) {}
        Deleter& get() noexcept {return _deleter_;}
        const Deleter& get() const noexcept {return _deleter_;}
    };

    // *************************************************************************************
    // unique_ptr
    template <typename T, typename Deleter = default_delete<T>>
    class unique_ptr {
    public:
        using element_type = T;
        using deleter_type = Deleter;
        using pointer = conditional_t<has_pointer_typedef_v<Deleter>, typename Deleter::pointer, T*>;

        // construct
        constexpr unique_ptr() noexcept : _ptr_(pointer()), _del_() {}
        explicit constexpr unique_ptr(nullptr_t) noexcept : unique_ptr() {}
        explicit constexpr unique_ptr(pointer ptr) noexcept : _ptr_(ptr), _del_() {}
        constexpr unique_ptr(pointer ptr, const Deleter& d) noexcept : _ptr_(ptr), _del_(d) {}
        constexpr unique_ptr(pointer ptr, Deleter&& d) noexcept : _ptr_(ptr), _del_(mystl::move(d)) {}
        // disable copy construct
        constexpr unique_ptr(const unique_ptr&) = delete;
        constexpr unique_ptr& operator=(const unique_ptr&) = delete;

        // move
        constexpr unique_ptr(unique_ptr&& other) noexcept : _ptr_(other._ptr_), _del_(mystl::move(other.get_deleter())) {
            other._ptr_ = pointer();
        }
        constexpr unique_ptr& operator=(unique_ptr&& other) noexcept {
            if (this != &other) {
                reset();
                _ptr_ = other._ptr_;
                _del_ = mystl::move(other.get_deleter());
                other._ptr_ = pointer();
            }
            return *this;
        }

        // destruct
        constexpr ~unique_ptr() {reset();}

        // observers
        constexpr element_type& operator*() const noexcept {return *_ptr_;}
        constexpr pointer operator->() const noexcept {return _ptr_;}
        constexpr pointer get() const noexcept {return _ptr_;}
        explicit constexpr operator bool() const noexcept {return _ptr_ != pointer();}

        constexpr deleter_type& get_deleter() noexcept {return _del_.get();}
        constexpr const deleter_type& get_deleter() const noexcept {return _del_.get();}

        constexpr pointer release() noexcept {
            pointer temp = _ptr_;
            _ptr_ = pointer();
            return temp;
        }

        constexpr void reset(pointer p = pointer()) noexcept {
            pointer temp = _ptr_;
            _ptr_ = p;
            if (temp != pointer()) {
                get_deleter()(temp);
            }
        }

        constexpr void swap(unique_ptr& other) noexcept {
            mystl::swap(_ptr_, other._ptr_);
            deleter_type temp = mystl::move(get_deleter());
            get_deleter() = mystl::move(other.get_deleter());
            other.get_deleter() = mystl::move(temp);
        }

    private:
        pointer _ptr_;
        delete_storage<Deleter> _del_;
    };

    // specialization for array
    template <typename T, typename Deleter>
    class unique_ptr<T[], Deleter> {
    public:
        using element_type = T;
        using deleter_type = Deleter;
        using pointer = conditional_t<has_pointer_typedef_v<Deleter>, typename Deleter::pointer, T*>;

        // construct
        constexpr unique_ptr() noexcept : _ptr_(pointer()), _del_() {}
        explicit constexpr unique_ptr(nullptr_t) noexcept : unique_ptr() {}
        explicit constexpr unique_ptr(pointer ptr) noexcept : _ptr_(ptr), _del_() {}
        constexpr unique_ptr(pointer ptr, const Deleter& d) noexcept : _ptr_(ptr), _del_(d) {}
        constexpr unique_ptr(pointer ptr, Deleter&& d) noexcept : _ptr_(ptr), _del_(mystl::move(d)) {}
        // disable copy construct
        constexpr unique_ptr(const unique_ptr&) = delete;
        constexpr unique_ptr& operator=(const unique_ptr&) = delete;

        // move
        constexpr unique_ptr(unique_ptr&& other) noexcept : _ptr_(other._ptr_), _del_(move(other.get_deleter())) {
            other._ptr_ = pointer();
        }
        constexpr unique_ptr& operator=(unique_ptr&& other) noexcept {
            if (this != &other) {
                reset();
                _ptr_ = other._ptr_;
                _del_ = mystl::move(other.get_deleter());
                other._ptr_ = pointer();
            }
            return *this;
        }

        // destruct
        constexpr ~unique_ptr() {reset();}

        // observers
        constexpr element_type& operator[](size_t i) const noexcept {return _ptr_[i];}
        constexpr pointer get() const noexcept {return _ptr_;}
        explicit constexpr operator bool() const noexcept {return _ptr_ != pointer();}

        constexpr deleter_type& get_deleter() noexcept {return _del_.get();}
        constexpr const deleter_type& get_deleter() const noexcept {return _del_.get();}

        constexpr pointer release() noexcept {
            pointer temp = _ptr_;
            _ptr_ = pointer();
            return temp;
        }

        constexpr void reset(pointer p = pointer()) noexcept {
            pointer temp = _ptr_;
            _ptr_ = p;
            if (temp != pointer()) {
                get_deleter()(temp);
            }
        }

        constexpr void swap(unique_ptr& other) noexcept {
            mystl::swap(_ptr_, other._ptr_);
            deleter_type temp = mystl::move(get_deleter());
            get_deleter() = mystl::move(other.get_deleter());
            other.get_deleter() = move(temp);
        }

    private:
        pointer _ptr_;
        delete_storage<Deleter> _del_;
    };

} // namespace mystl

#endif //MEMORY_H
