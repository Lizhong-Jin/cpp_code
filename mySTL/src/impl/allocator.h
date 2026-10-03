#pragma once

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <new> // using standard library here

#include "construct.h"

namespace mystl {
    template <typename T>
    class allocator {
    public:
        typedef T           value_type;
        typedef T*          pointer;
        typedef const T*    const_pointer;
        typedef void*       void_pointer;
        typedef const void* const_void_pointer;
        typedef T&          reference;
        typedef const T&    const_reference;
        typedef size_t      size_type;
        typedef ptrdiff_t   difference_type;

        typedef true_type   propagate_on_container_move_assignment;
        typedef true_type   is_always_equal;

        template <typename U>
        struct rebind {
            typedef allocator<U> other;
        };

        static constexpr T* allocate() {return allocate(1);}

        static constexpr T* allocate(const size_type n) {
            if (n==0) return nullptr;
            if (n > static_cast<size_type>(-1) / sizeof(T)) {
                throw std::bad_alloc();
            }
            if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
                return static_cast<T*>(::operator new(sizeof(T) * n, std::align_val_t(alignof(T))));
            } else {
                return static_cast<T*>(::operator new(sizeof(T) * n));
            }
        }

        static constexpr void deallocate(T* ptr) {
            if (ptr==nullptr) return;
            if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
                ::operator delete(ptr, std::align_val_t(alignof(T)));
            } else {
                ::operator delete(ptr);
            }
        }

        static constexpr void deallocate(T* ptr, size_type) {
            deallocate(ptr);
        }

        static constexpr void construct(T* ptr) {mystl::construct(ptr);}

        static constexpr void construct(T* ptr, const T& value) {mystl::construct(ptr, value);}

        static constexpr void construct(T* ptr, T&& value) {mystl::construct(ptr, mystl::move(value));}

        template <typename... Args>
        static constexpr void construct(T* ptr, Args&&... args) {mystl::construct(ptr, mystl::forward<Args>(args)...);}

        static constexpr void destroy(T* ptr) {mystl::destroy_at(ptr);}

        static constexpr void destroy(T* first, T* last) {mystl::destroy(first, last);}

    };

    template <typename T1, typename T2>
    constexpr bool operator==(const allocator<T1>&, const allocator<T2>&) noexcept {
        return true;
    }

    template <typename T1, typename T2>
    constexpr bool operator!=(const allocator<T1>&, const allocator<T2>&) noexcept {
        return false;
    }

    namespace detail {
        // has_construct
        template <typename, typename = void, typename...>
        struct has_construct : false_type {};
        template <typename Alloc, typename T, typename... Args>
        struct has_construct<Alloc, void_t<decltype(declval<Alloc&>().construct(declval<T*>(), declval<Args>()...))>, T, Args...>
                                : true_type {};

        // has_destroy
        template <typename, typename, typename = void>
        struct has_destroy : false_type {};
        template <typename Alloc, typename T>
        struct has_destroy<Alloc, T, void_t<decltype(declval<Alloc&>().destroy(declval<T*>()))>> : true_type {};

        // has_max_size
        template <typename, typename = void>
        struct has_max_size : false_type {};
        template <typename Alloc>
        struct has_max_size<Alloc, void_t<decltype(declval<const Alloc&>().max_size())>> : true_type {};

        // has_rebind
        template <typename, typename, typename = void>
        struct alloc_has_rebind : false_type {};
        template <typename Alloc, typename T>
        struct alloc_has_rebind<Alloc, T, void_t<typename Alloc::template rebind<T>::other>> : true_type {};

        template <typename Alloc, typename = void>
        struct alloc_copy_propagation { using type = false_type; };
        template <typename Alloc>
        struct alloc_copy_propagation<Alloc, void_t<typename Alloc::propagate_on_container_copy_assignment>> {
            using type = typename Alloc::propagate_on_container_copy_assignment;
        };
        template <typename Alloc, typename = void>
        struct alloc_move_propagation { using type = false_type; };
        template <typename Alloc>
        struct alloc_move_propagation<Alloc, void_t<typename Alloc::propagate_on_container_move_assignment>> {
            using type = typename Alloc::propagate_on_container_move_assignment;
        };
        template <typename Alloc, typename = void>
        struct alloc_swap_propagation { using type = false_type; };
        template <typename Alloc>
        struct alloc_swap_propagation<Alloc, void_t<typename Alloc::propagate_on_container_swap>> {
            using type = typename Alloc::propagate_on_container_swap;
        };
        template <typename Alloc, typename = void>
        struct alloc_always_equal { using type = is_empty<Alloc>; };
        template <typename Alloc>
        struct alloc_always_equal<Alloc, void_t<typename Alloc::is_always_equal>> {
            using type = typename Alloc::is_always_equal;
        };
    }

    // *************************************************************************************
    // allocator_traits
    template <typename Alloc>
    struct allocator_traits {
        using allocator_type        = Alloc;
        using value_type            = typename Alloc::value_type;
        using pointer               = typename Alloc::pointer;
        using const_pointer         = typename Alloc::const_pointer;
        using void_pointer          = typename Alloc::void_pointer;
        using const_void_pointer    = typename Alloc::const_void_pointer;
        using difference_type       = typename Alloc::difference_type;
        using size_type             = typename Alloc::size_type;

        using propagate_on_container_copy_assignment = typename detail::alloc_copy_propagation<Alloc>::type;
        using propagate_on_container_move_assignment = typename detail::alloc_move_propagation<Alloc>::type;
        using propagate_on_container_swap            = typename detail::alloc_swap_propagation<Alloc>::type;
        using is_always_equal                        = typename detail::alloc_always_equal<Alloc>::type;

        template <typename T>
        using rebind_alloc = typename Alloc::template rebind<T>::other;

        template <typename T>
        using rebind_traits = allocator_traits<rebind_alloc<T>>;

        static constexpr pointer allocate(Alloc& alloc, size_type n) {
            return alloc.allocate(n);
        }

        static constexpr void deallocate(Alloc& alloc, pointer p, size_type n) {
            alloc.deallocate(p, n);
        }

        template <typename T, typename... Args>
        static constexpr void construct(Alloc& alloc, T* p, Args&&... args) {
            if constexpr (detail::has_construct<Alloc, void, T, Args...>::value) {
                alloc.construct(p, mystl::forward<Args>(args)...);
            }else {
                ::new(static_cast<void*>(p)) T(mystl::forward<Args>(args)...);
            }
        }

        template <typename T>
        static constexpr void destroy(Alloc& alloc, T* p) {
            if constexpr (detail::has_destroy<Alloc, T>::value) {
                alloc.destroy(p);
            }else {
                mystl::destroy_at<T>(p);
            }
        }

        static constexpr size_type max_size(const Alloc& alloc) {
            if constexpr (detail::has_max_size<Alloc>::value) {
                return alloc.max_size();
            }else {
                return static_cast<size_type>(-1) / sizeof(value_type);
            }
        }

        static constexpr Alloc select_on_container_copy_construction(const Alloc& alloc) {
            if constexpr (requires { alloc.select_on_container_copy_construction(); }) {
                return alloc.select_on_container_copy_construction();
            } else {
                return alloc;
            }
        }
    };

    // destroy with allocator
    template <typename T, typename Allocator>
    constexpr void destroy_at_a(T* ptr, Allocator& alloc) noexcept {
        allocator_traits<Allocator>::destroy(alloc, ptr);
    }

    template <typename Forward_Iterator, typename Allocator>
    constexpr void destroy_a(Forward_Iterator first, Forward_Iterator last, Allocator& alloc) noexcept {
        for (; first != last; ++first) {
            allocator_traits<Allocator>::destroy(alloc, mystl::addressof(*first));
        }
    }

} // namespace mystl

#endif //ALLOCATOR_H
