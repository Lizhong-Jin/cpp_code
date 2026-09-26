#pragma once

#ifndef WRAPPERS_H
#define WRAPPERS_H

#include <cstddef>
#include <new>
#include <typeinfo>

#include "construct.h"
#include "functions.h"
#include "../exception.h"
#include "../cstring.h"
#include "../utility.h"

namespace mystl {
    // *************************************************************************************
    // Helper class
    // bad_function_call
    class bad_function_call final : public exception {
    public:
        [[nodiscard]] const char* what() const noexcept override {return "bad function call";}
    };

    // is_bind_expression

    // is_placeholder

    // *************************************************************************************
    // function
    template <typename>
    class function;

    template <typename R, typename... Args>
    class function<R(Args...)> {
    public:
        using result_type = R;

        // construct
        constexpr function() noexcept = default;
        explicit constexpr function(nullptr_t) noexcept {}
        constexpr function(const function& other) {
            if (other.v_ptr) {
                ptr = other.v_ptr->clone(other.ptr, buffer, on_heap);
                v_ptr = other.v_ptr;
                on_heap = other.on_heap;
            }
        }
        constexpr function(function&& other) noexcept {
            if (other.v_ptr) {
                if (other.on_heap) {
                    ptr = other.ptr;
                    on_heap = true;
                    v_ptr = other.v_ptr;
                }else {
                    ptr = other.v_ptr->move(other.ptr, buffer, on_heap);
                    v_ptr = other.v_ptr;
                    on_heap = false;
                }
                other.reset_to_empty();
            }
        }
        template <typename F>
        requires (!is_same_v<decay_t<F>, function>)
        explicit constexpr function(F&& f) {
            emplace_functor(forward<F>(f));
        }

        // destruct
        constexpr ~function() {
            if (v_ptr) v_ptr->destroy(ptr, on_heap);
        }

        // operator =
        constexpr function& operator=(nullptr_t) noexcept {
            if (v_ptr) v_ptr->destroy(ptr, on_heap);
            reset_to_empty();
            return *this;
        }

        constexpr function& operator=(const function& other) noexcept {
            if (this == &other) return *this;
            if (v_ptr) v_ptr->destroy(ptr, on_heap);
            reset_to_empty();
            if (other.v_ptr) {
                ptr = other.v_ptr->clone(other.ptr, buffer, on_heap);
                v_ptr = other.v_ptr;
            }
            return *this;
        }

        constexpr function& operator=(function&& other) noexcept {
            if (this == &other) return *this;
            if (v_ptr) v_ptr->destroy(ptr, on_heap);
            reset_to_empty();
            if (other.v_ptr) {
                if (other.on_heap) {
                    ptr = other.ptr;
                    v_ptr = other.v_ptr;
                    on_heap = true;
                    other.reset_to_empty();
                }else {
                    ptr = other.v_ptr->move(other.ptr, buffer, on_heap);
                    v_ptr = other.v_ptr;
                    on_heap = false;
                }
            }
            return *this;
        }

        template <typename F>
        requires (!is_same_v<decay_t<F>, function>)
        constexpr function& operator=(F&& f) {
            if (v_ptr) v_ptr->destroy(ptr, on_heap);
            reset_to_empty();
            emplace_functor(forward<F>(f));
            return *this;
        }

        // bool && call
        explicit constexpr operator bool() const noexcept {return v_ptr;}

        R operator()(Args... args) const {
            if (!v_ptr) throw bad_function_call();
            return v_ptr->invoke(ptr, forward<Args>(args)...);
        }

        // swap
        constexpr void swap(function& other) noexcept {
            using mystl::swap;
            if (!v_ptr && !other.v_ptr) return;
            if (!v_ptr) {*this = move(other); return;}
            if (!other.v_ptr) {other = move(*this); return;}
            if (on_heap && other.on_heap) {
                swap(ptr, other.ptr);
                swap(v_ptr, other.v_ptr);
            }else if (!on_heap && !other.on_heap) {
                alignas(buffer_align) std::byte temp[buffer_size];
                memcpy(temp, buffer, buffer_size);
                memcpy(buffer, other.buffer, buffer_size);
                memcpy(other.buffer, temp, buffer_size);
                swap(ptr, other.ptr);
                swap(v_ptr, other.v_ptr);
            }else {
                auto temp = move(other);
                other = move(*this);
                *this = move(temp);
                swap(on_heap, other.on_heap);
            }
        }

        // operator ==
        [[nodiscard]] constexpr bool operator==(nullptr_t) const noexcept {
            return v_ptr == nullptr;
        }

        // nonstandard operator ==
        [[nodiscard]] constexpr bool operator!=(const function& other) const noexcept {
            if (v_ptr == nullptr && other.v_ptr == nullptr) return true;
            if (v_ptr == other.v_ptr && v_ptr->target_type()() == typeid(void(*)(Args...))) {
                auto a = static_cast<void(**)(Args...)>(v_ptr->raw_ptr(ptr));
                auto b = static_cast<void(**)(Args...)>(v_ptr->raw_ptr(other.ptr));
                return *a == *b;
            }
            return false;
        }

        // target_info
        [[nodiscard]] constexpr const std::type_info& target_type() const noexcept {
            return v_ptr ? v_ptr->target_type() : typeid(void);
        }

        // target
        template <typename T>
        constexpr T* target() noexcept {
            if (!v_ptr) return nullptr;
            if (v_ptr == get_vtable<T>()) {
                return reinterpret_cast<T*>(v_ptr->raw_ptr());
            }
            return nullptr;
        }

        template <typename T>
        constexpr const T* target() const noexcept {
            if (!v_ptr) return nullptr;
            if (v_ptr == get_vtable<T>()) {
                return reinterpret_cast<const T*>(v_ptr->raw_ptr());
            }
            return nullptr;
        }

    private:
        // virtual table
        struct vtable_t {
            R                       (*invoke)(void*, Args&&...) noexcept;
            void                    (*destroy)(void*, bool on_heap) noexcept;
            void*                   (*clone)(const void* src, void* dest, bool& dest_on_heap);
            void*                   (*move)(void* src, void* dest, bool& dest_on_heap) noexcept;
            const std::type_info&   (*target_type)() noexcept;
            void*                   (*raw_ptr)(void*) noexcept;
        };

        static constexpr size_t buffer_size = 3 * sizeof(void*);
        static constexpr size_t buffer_align = alignof(max_align_t);

        alignas(buffer_align) std::byte buffer[buffer_size]{};
        void* ptr = nullptr;
        bool on_heap = false;
        const vtable_t* v_ptr= nullptr;

        template <typename F>
        static constexpr bool is_small() noexcept {
            return sizeof(F) <= buffer_size && alignof(F) <= buffer_align && is_nothrow_move_constructible_v<F>;
        }

        template <typename F>
        static constexpr F* as_local(void* buf) noexcept {
            return std::launder(reinterpret_cast<F*>(buf));
        }

        template <typename F>
        static constexpr const F* as_local(const void* buf) noexcept {
            return std::launder(reinterpret_cast<const F*>(buf));
        }

        template <typename F>
        static constexpr R invoke_impl(void* self, Args&&... args)
        noexcept(is_nothrow_invocable_r_v<R, F&, Args...>) {
            if constexpr (is_void_v<R>) {
                invoke(*reinterpret_cast<F*>(self), forward<Args>(args)...);
            }else {
                return invoke(*reinterpret_cast<F*>(self), forward<Args>(args)...);;
            }
        }

        template <typename F>
        static constexpr void destroy_impl(void* self, bool on_heap) noexcept {
            if (self == nullptr) return;
            if (on_heap) {
                delete reinterpret_cast<F*>(self);
            }else {
                reinterpret_cast<F*>(self)->~F();
            }
        }

        template <typename F>
        static constexpr void* clone_impl(const void* src, void* dest, bool& dest_on_heap) {
            auto s = reinterpret_cast<const F*>(src);
            if constexpr (is_small<F>()) {
                new (dest) F(*s);
                dest_on_heap = false;
                return as_local<F>(dest);
            }else {
                F* ptr = new F(*s);
                dest_on_heap = true;
                return ptr;
            }
        }

        template <typename F>
        static constexpr void* move_impl(void* src, void* dest, bool& dest_on_heap) noexcept {
            auto s = reinterpret_cast<const F*>(src);
            if constexpr (is_small<F>()) {
                new (dest) F(move(*s));
                s->~F();
                dest_on_heap = false;
                return as_local<F>(dest);
            }else {
                dest_on_heap = true;
                return s;
            }
        }

        template <typename F>
        static constexpr const std::type_info& target_type_impl() noexcept {
            return typeid(F);
        }

        template <typename F>
        static constexpr void* raw_ptr_impl(void* self) noexcept {
            return reinterpret_cast<void*>(reinterpret_cast<F*>(self));
        }

        template <typename F>
        struct vtable_holder {
            static inline constexpr vtable_t table = {
                &invoke_impl<F>,
                &destroy_impl<F>,
                &clone_impl<F>,
                &move_impl<F>,
                &target_type_impl<F>,
                &raw_ptr_impl<F>
            };
        };

        template <typename F>
        static constexpr const vtable_t* get_vtable() noexcept {
            return &vtable_holder<F>::table;
        }

        constexpr void reset_to_empty() noexcept {
            v_ptr = nullptr;
            ptr = nullptr;
            on_heap = false;
        }

        template <typename F>
        constexpr void emplace_functor(F&& f) {
            using functor_t = decay_t<F>;
            static_assert(is_invocable_r_v<R, functor_t, Args...>,
                            "Function target must be callable with the signature R(Args...)");
            if constexpr (is_small<functor_t>()) {
                new (buffer) functor_t(forward<F>(f));
                ptr = as_local<functor_t>(buffer);
                on_heap = false;
            }else {
                new (buffer) functor_t(forward<F>(f));
                on_heap = true;
            }
            v_ptr = get_vtable<functor_t>();
        }
    };

    // swap
    template <typename R, typename... Args>
    void swap(function<R(Args...)>& lhs, function<R(Args...)>& rhs) noexcept {
        lhs.swap(rhs);
    }

    // operator ==
    template <typename R, typename... Args>
    bool operator==(const function<R(Args...)>& func, nullptr_t) noexcept {
        return func == nullptr;
    }

    // nonstandard operator ==
    template <typename R, typename... Args>
    bool operator==(const function<R(Args...)>& lhs, const function<R(Args...)>& rhs) noexcept {
        return lhs == rhs;
    }

    // *************************************************************************************
    // reference_wrapper
    template <typename T>
    class reference_wrapper {
    private:
        T* ptr;
        static constexpr T* _fun(T& _r) noexcept {return addressof(_r);}
        static constexpr void _fun(T&&) = delete;
        template <typename U, typename U2 = remove_cv_ref_t<U>>
        using _not_same = enable_if_t<!is_same_v<reference_wrapper, U2>>;

    public:

        template <typename U, typename = _not_same<U>, typename = enable_if_t<is_lvalue_reference_v<U&&>>>
        explicit constexpr reference_wrapper(U&& u_ref) noexcept(noexcept(reference_wrapper::_fun(declval<U>())))
        : ptr(reference_wrapper::_fun(forward<U>(u_ref))) {}

        constexpr reference_wrapper(const reference_wrapper& other) = default;

        reference_wrapper& operator=(const reference_wrapper& other) = default;

        explicit constexpr operator T&() const noexcept {
            return this->get();
        }

        constexpr T& get() const noexcept {
            return *ptr;
        }

        template <typename... Args>
        constexpr invoke_result_t<T&, Args...> operator()(Args... args) const
        noexcept(is_nothrow_invocable_v<T&, Args...>) {
            return invoke(*ptr, forward<Args>(args)...);
        }

        // operator ==
        [[nodiscard]] constexpr bool operator==(const reference_wrapper& other) const noexcept {
            return this->get() == other.get();
        }

    };

    template <typename T>
    reference_wrapper(T&) -> reference_wrapper<T>;






} // namespace mystl

#endif //WRAPPERS_H
