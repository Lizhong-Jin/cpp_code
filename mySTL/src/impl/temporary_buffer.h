#pragma once

#ifndef TEMPORARY_BUFFER_H
#define TEMPORARY_BUFFER_H

#include <new>

#include "construct.h"
#include "../utility.h"

namespace mystl {
    namespace detail {
        // get_temp_buffer_impl
        template <typename T>
        T* get_temp_buffer_impl(const ptrdiff_t len) noexcept {
            if (len < 0 || static_cast<size_t>(len) > static_cast<size_t>(-1) / sizeof(T)) {
                return nullptr;
            }
#if __cpp_aligned_new >= 201606
            if (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
                return static_cast<T*>(::operator new(len * sizeof(T),
                                        static_cast<std::align_val_t>(alignof(T)), std::nothrow));
            }
#endif
            return static_cast<T*>(::operator new(len * sizeof(T), std::nothrow));
        }

        // release_temp_buffer_impl
        template <typename T>
        void release_temp_buffer_impl(T* ptr, size_t len __attribute__((__unused__))) noexcept {
            if (!ptr) return;
#if __cpp_sized_deallocation
    #define SIZED_DEALLOC(T, p, n) (p), (n) * sizeof(T)
#else
    #define SIZED_DEALLOC(T, p, n) (p)
#endif
#if __cpp_aligned_new >= 201606
            if (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
                ::operator delete(SIZED_DEALLOC(T, ptr, len), static_cast<std::align_val_t>(alignof(T)));
                return;
            }
#endif
            ::operator delete(SIZED_DEALLOC(T, ptr, len));
        }
#undef SIZED_DEALLOC
    }

    // get_temp_buffer
    template <typename T>
    pair<T*, ptrdiff_t> get_temp_buffer(ptrdiff_t len) noexcept {
        while (len > 0) {
            if (T* ptr = detail::get_temp_buffer_impl<T>(len)) {
                return make_pair(ptr, len);
            }
            len = len == 1 ? 0 : (len + 1) / 2;
        }
        return make_pair(nullptr, 0);
    }

    // release_temp_buffer
    template <typename T>
    void release_temp_buffer(T* ptr, size_t len) noexcept {
        detail::release_temp_buffer_impl(ptr, len);
    }

    template <typename T>
    void release_temp_buffer(T* ptr) noexcept {
#if __cpp_aligned_new >= 201606
        if (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
            ::operator delete(ptr, static_cast<std::align_val_t>(alignof(T)));
            return;
        }
#endif
        ::operator delete(ptr);
    }

    // uninitialized_construct_buffer
    template <bool>
    struct uninitialized_construct_buffer_dispatch {
        template <typename Pointer, typename Forward_Iterator>
        static void ucr(Pointer first, Pointer last, Forward_Iterator seed) {
            if (first == last) return;
            Pointer current = first;
            try {
                construct(addressof(*first), move(*seed));
                Pointer prev = current;
                for (++current; current != last; ++current, ++prev) {
                    construct(addressof(*current), move(*prev));
                }
                *seed = move(*prev);
            }catch (...){
                destroy(first, current);
                throw;
            }
        }
    };

    template <>
    struct uninitialized_construct_buffer_dispatch<true> {
        template <typename Pointer, typename Forward_Iterator>
        static void ucr(Pointer, Pointer, Forward_Iterator) {}
    };

    template <typename T, typename Forward_Iterator>
    constexpr void uninitialized_construct_buffer(T* first, T* last, Forward_Iterator seed) {
        uninitialized_construct_buffer_dispatch<is_trivially_constructible_v<T>>::ucr(first, last, seed);
    }

    // **********************************************************************************
    // temporary_buffer, create a temporary buffer for algorithms that may require extra memory
    template <typename Forward_Iterator, typename T>
    class temporary_buffer {
    public:
        using value_type = T;
        using pointer = T*;
        using size_type = ptrdiff_t;

        [[nodiscard]] size_type size() const noexcept {return M_impl.M_len;}

        [[nodiscard]] size_type requested_size() const noexcept {return original_length;}

        pointer begin() const noexcept {return M_impl.M_buffer;}

        pointer end() const noexcept {return M_impl.M_buffer + M_impl.M_len;}

        temporary_buffer(Forward_Iterator seed, size_type len) : original_length(len), M_impl(len) {
            uninitialized_construct_buffer(begin(), end(), seed);
        }

        temporary_buffer(const temporary_buffer&) = delete;

        void operator=(const temporary_buffer&) = delete;

        ~temporary_buffer() {
            destroy(M_impl.M_buffer, M_impl.M_buffer + M_impl.M_len);
        }

    protected:
        size_type original_length;
        struct impl {
            explicit impl(const size_type len) {
                pair<pointer, size_type> p = get_temp_buffer<value_type>(len);
                M_buffer = p.first;
                M_len = p.second;
            }

            ~impl() {
                release_temp_buffer(M_buffer, M_len);
            }

            size_type M_len;
            pointer M_buffer;
        } M_impl;
    };

} // namespace mystl

#endif //TEMPORARY_BUFFER_H
