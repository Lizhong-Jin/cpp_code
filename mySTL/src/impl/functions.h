#pragma once

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "../utility.h"
#include "../type_traits.h"

namespace mystl {
    // invoke_forward, in place of mystl::forward
    // invert a reference_wrapper to a lvalue_reference
    template <typename T, typename U = unwrap_reference_t<T>>
    constexpr U&& invoke_forward(remove_reference_t<T>& type) {
        return static_cast<U&&>(type);
    }

    // invoke_impl
    template <typename R, typename Functor, typename... Args>
    constexpr R invoke_impl(_invoke_other, Functor&& f, Args&&... args) {
        return forward<Functor>(f)(forward<Args>(args)...);
    }

    template <typename R, typename MemberFun, typename T, typename... Args>
    constexpr R invoke_impl(_invoke_member_function_ref, MemberFun&& f, T&& t, Args&&... args) {
        return (invoke_forward<T>(t).*f)(forward<Args>(args)...);
    }

    template <typename R, typename MemberFun, typename T, typename... Args>
    constexpr R invoke_impl(_invoke_member_function_deref, MemberFun&& f, T&& t, Args&&... args) {
        return ((*forward<T>(t)).*f)(forward<Args>(args)...);
    }

    template <typename R, typename MemberPtr, typename T>
    constexpr R invoke_impl(_invoke_member_object_ref, MemberPtr&& ptr, T&& t) {
        return invoke_forward<T>(t).*ptr;
    }

    template <typename R, typename MemberPtr, typename T>
    constexpr R invoke_impl(_invoke_member_object_deref, MemberPtr&& ptr, T&& t) {
        return (*forward<T>(t)).*ptr;
    }

    // invoke
    template <typename Functor, typename... Args>
    constexpr invoke_result_t<Functor, Args...> invoke(Functor&& fn, Args&&... args)
    noexcept(is_nothrow_invocable_v<Functor, Args...>) {
        using Result = invoke_result<Functor, Args...>;
        using Result_type = typename Result::type;
        using Invoke_type = typename Result::invoke_type;
        return invoke_impl<Result_type>(Invoke_type{}, forward<Functor>(fn), forward<Args>(args)...);
    }

    // invoke_r
    template <typename R, typename Functor, typename... Args>
    constexpr enable_if_t<is_invocable_r_v<Functor, Args...>, R> invoke_r(Functor&& fn, Args&&... args)
    noexcept(is_nothrow_invocable_r_v<R, Functor, Args...>) {
        using Result = invoke_result<Functor, Args...>;
        using Result_type = typename Result::type;
        using Invoke_type = typename Result::invoke_type;
        if constexpr(is_void_v<R>) {
            invoke_impl<Result_type>(Invoke_type{}, forward<Functor>(fn), forward<Args>(args)...);
        }else {
            return invoke_impl<Result_type>(Invoke_type{}, forward<Functor>(fn), forward<Args>(args)...);
        }
    }

    // ref && cref
    // forward declaration of reference_wrapper
    template <typename T> class reference_wrapper;

    template <typename T>
    inline constexpr reference_wrapper<T> ref(T& t) noexcept {return reference_wrapper<T>(t);}
    template <typename T>
    inline constexpr reference_wrapper<T> ref(reference_wrapper<T> t) noexcept {return t;}
    template <typename T>
    void ref(T&&) = delete;

    template <typename T>
    inline constexpr reference_wrapper<const T> cref(T& t) noexcept {return reference_wrapper<const T>(t);}
    template <typename T>
    inline constexpr reference_wrapper<const T> cref(reference_wrapper<T> t) noexcept {return t.get();}
    template <typename T>
    void cref(T&&) = delete;


} // namespace mystl

#endif //FUNCTIONS_H
