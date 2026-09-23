#pragma once

#ifndef CONSTRUCT_H
#define CONSTRUCT_H

#include "../type_traits.h"
#include "../iterator.h"

namespace mystl {
    // addressof
    template <typename T>
    constexpr enable_if_t<is_object_v<T>, T*> addressof(T& arg) noexcept {
#if __has_builtin(__builtin_addressof)
        return _builtin_addressof(arg);
#else
        return reinterpret_cast<T*>(&const_cast<char&>(reinterpret_cast<const volatile char&>(arg)));
#endif
    }

    template <typename T>
    constexpr enable_if_t<!is_object_v<T>, T*> addressof(T& arg) noexcept {
        return &arg;
    }


    // align
    inline void* align(const size_t alignment, const size_t size, void*& ptr, size_t& space) noexcept {
        const auto p = reinterpret_cast<uintptr_t>(ptr);
        const uintptr_t aligned = (p + alignment - 1) & ~(alignment - 1);
        const size_t padding = aligned - p;
        if (padding + size > space) return nullptr;
        space -= padding;
        ptr = reinterpret_cast<void*>(aligned);
        return ptr;
    }

    // construct
    template <typename T>
    void construct(T* ptr) {
        ::new (static_cast<void *>(ptr)) T();
    }

    template <typename T, typename U>
    void construct(T* ptr, const U& val) {
        ::new (static_cast<void *>(ptr)) T(val);
    }

    template <typename T, typename... Args>
    void construct(T* ptr, Args&&... args) {
        ::new (static_cast<void *>(ptr)) T(forward<Args>(args)...);
    }

    // destroy_at
    template <typename T>
    void destroy_at_impl(T* ptr) noexcept {
        if constexpr (is_bounded_array_v<T>) {
            using Element = remove_extent_t<T>;
            for (size_t i = 0; i < extent_v<T>; ++i) {
                destroy_at(addressof((*ptr)[i]));
            }
        }else {
            ptr->~T();
        }
    }

    template <typename T>
    void destroy_at(T* ptr) noexcept {
        if constexpr (!is_trivially_destructible_v<remove_all_extent_t<T>>) {
            destroy_at_impl(ptr);
        }
    }

    // destroy
    template <typename Forward_Iterator>
    void destroy(Forward_Iterator first, Forward_Iterator last) noexcept {
        if constexpr (!is_trivially_destructible_v<iter_value_type<Forward_Iterator>>) {
            for (; first != last; ++first) destroy_at(addressof(*first));
        }
    }

    // destroy_n
    template <typename Forward_Iterator, typename Size>
    Forward_Iterator destroy_n_cat(Forward_Iterator first, Size n, true_type) noexcept {
        // for trivially destructible type, do nothing
        advance(first, n);
        return first;
    }

    template <typename Forward_Iterator, typename Size>
    Forward_Iterator destroy_n_cat(Forward_Iterator first, Size n, false_type) noexcept {
        for (; n>0; --n, ++first) destroy_at(addressof(*first));
        return first;
    }

    template <typename Forward_Iterator, typename Size>
    Forward_Iterator destroy_n(Forward_Iterator first, Size n) noexcept {
        return destroy_n_cat(first, n, bool_constant<is_trivially_destructible_v<iter_value_type<Forward_Iterator>>>{});
    }



} // namespace mystl

#endif //CONSTRUCT_H
