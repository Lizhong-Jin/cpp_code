#pragma once

#ifndef INVOKE_TRAITS_H
#define INVOKE_TRAITS_H

#include "type_traits_normal.h"

namespace mystl {
    // success_type && failure_type (just for assist)
    template <typename T>   struct _success_type {using type = T;};
                            struct _failure_type {};

    // invoke functor type
    struct _invoke_member_function_ref {};
    struct _invoke_member_function_deref {};
    struct _invoke_member_object_ref {};
    struct _invoke_member_object_deref {};
    struct _invoke_other {};

    // associate a tag type with a specialization of _success_type
    template <typename T, typename Tag>
    struct _result_of_success : _success_type<T> {using invoke_type = Tag;};

    // result of member function, member object and other
    template <typename MemberPtr, typename Arg, typename... Args>
    struct _result_of_member_function_ref {
    private:
        template <typename Fp, typename Tp, typename... A>
        static constexpr _result_of_success<
            decltype((declval<Tp>().*declval<Fp>())(declval<A>()...)), _invoke_member_function_ref> test(int);
        template <typename...>
        static constexpr _failure_type test(...);
    public:
        using type = decltype(test<MemberPtr, Arg, Args...>(0));
    };

    template <typename MemberPtr, typename Arg, typename... Args>
    struct _result_of_member_function_deref {
    private:
        template <typename Fp, typename Tp, typename... A>
        static constexpr _result_of_success<
            decltype(((*declval<Tp>()).*declval<Fp>())(declval<A>()...)), _invoke_member_function_deref> test(int);
        template <typename...>
        static constexpr _failure_type test(...);
    public:
        using type = decltype(test<MemberPtr, Arg, Args...>(0));
    };

    template <typename MemberPtr, typename Arg, typename... Args>
    struct _result_of_member_function;

    template <typename R, typename Class, typename Arg, typename... Args>
    struct _result_of_member_function<R Class::*, Arg, Args...> {
        using Argval = remove_reference_t<Arg>;
        using MemberPtr = R Class::*;
        using type = typename conditional_t<is_base_of_v<Class, Argval>,
                    _result_of_member_function_ref<MemberPtr, Arg, Args...>,
                    _result_of_member_function_deref<MemberPtr, Arg, Args...>>::type;
    };

    template <typename MemberPtr, typename Arg>
    struct _result_of_member_object_ref {
    private:
        template <typename Fp, typename Tp>
        static constexpr _result_of_success<
            decltype(declval<Tp>().*declval<Fp>()), _invoke_member_object_ref> test(int);
        template <typename...>
        static constexpr _failure_type test(...);
    public:
        using type = decltype(test<MemberPtr, Arg>(0));
    };

    template <typename MemberPtr, typename Arg>
    struct _result_of_member_object_deref {
    private:
        template <typename Fp, typename Tp>
        static constexpr _result_of_success<
            decltype((*declval<Tp>()).*declval<Fp>()), _invoke_member_object_deref> test(int);
        template <typename...>
        static constexpr _failure_type test(...);
    public:
        using type = decltype(test<MemberPtr, Arg>(0));
    };

    template <typename MemberPtr, typename Arg>
    struct _result_of_member_object;

    template <typename R, typename Class, typename Arg>
    struct _result_of_member_object<R Class::*, Arg> {
        using Argval = remove_reference_t<Arg>;
        using MemberPtr = R Class::*;
        using type = typename conditional_t<is_same_v<Argval, Class> || is_base_of_v<Class, Argval>,
                    _result_of_member_object_ref<MemberPtr, Arg>,
                    _result_of_member_object_deref<MemberPtr, Arg>>::type;
    };

    // result of other
    template <typename Functor, typename... Args>
    struct _result_of_other {
    private:
        template <typename F, typename... A>
        static constexpr _result_of_success<decltype(declval<F>()(declval<A>()...)), _invoke_other> test(int);
        template <typename...>
        static constexpr _failure_type test(...);
    public:
        using type = decltype(test<Functor, Args...>(0));
    };

    // invoke_result_impl
    template <bool, bool, typename Functor, typename... Args>
    struct invoke_result_impl {using type = _failure_type;};

    template <typename Functor, typename Arg, typename... Args>
    struct invoke_result_impl<true, false, Functor, Arg, Args...>
            : _result_of_member_function<decay_t<Functor>, unwrap_reference_t<Arg>, Args...> {};

    template <typename Functor, typename Arg>
    struct invoke_result_impl<false, true, Functor, Arg>
            : _result_of_member_object<decay_t<Functor>, unwrap_reference_t<Arg>> {};

    template <typename Functor, typename... Args>
    struct invoke_result_impl<false, false, Functor, Args...>
            : _result_of_other<Functor, Args...> {};

    // invoke_result
    template <typename Functor, typename... Args>
    struct invoke_result : invoke_result_impl<is_member_function_pointer_v<remove_reference_t<Functor>>,
                                is_member_object_pointer_v<remove_reference_t<Functor>>, Functor, Args...>::type {
        static_assert(_is_complete_or_unbounded_v<Functor>,
                        "Functor must be a complete class or unbounded array");
        static_assert((_is_complete_or_unbounded_v<Args> && ...),
                        "each argument type must be a complete class or unbounded array");
    };

    template <typename Functor, typename... Args>
    using invoke_result_t = typename invoke_result<Functor, Args...>::type;

    // result_of (has been removed after c++20)
    template <typename> struct result_of;
    template <typename Functor, typename... Args>
    struct result_of<Functor(Args...)> : invoke_result<Functor, Args...> {};
    template <typename Functor, typename... Args>
    using result_of_t = typename result_of<Functor(Args...)>::type;

    // is_invocable && is_nothrow_invocable && is_invocable_r && is_nothrow_invocable_r
    template <typename Result, typename Ret, bool = is_void_v<Ret>, typename = void>
    struct is_invocable_impl : false_type {};

    template <typename Result, typename Ret>
    struct is_invocable_impl<Result, Ret, true, void_t<typename Result::type>> : true_type {
        using nothrow_type = true_type;
    };

    template <typename Result, typename Ret>
    struct is_invocable_impl<Result, Ret, false, void_t<typename Result::type>> {
    private:
        using Result_type = typename Result::type;
        static Result_type get() noexcept;
        template <typename T>
        static void conv(type_identity_t<T>) noexcept;

        template <typename T, bool Nothrow = noexcept(conv<T>(get())),
                    typename = decltype(conv<T>(get())),
#if __has_builtin(__reference_converts_from_temporary)
                    bool Dangle = __reference_converts_from_temporary(T, Result_type)
#else
                    bool Dangle = false
#endif
        >
        static bool_constant<Nothrow && !Dangle> test(int);

        template <typename T, bool = false> static false_type test(...);
    public:
        using type = decltype(test<Ret, true>(0));
        using nothrow_type = decltype(test<Ret>(0));
    };

    template <typename Fn, typename Tp, typename... Args>
    constexpr bool _call_is_nothrow_impl(_invoke_member_function_ref) {
        using Up = unwrap_reference_t<Tp>;
        return noexcept((declval<Up>().*declval<Fn>())(declval<Args>()...));
    }
    template <typename Fn, typename Tp, typename... Args>
    constexpr bool _call_is_nothrow_impl(_invoke_member_function_deref) {
        return noexcept(((*declval<Tp>()).*declval<Fn>())(declval<Args>()...));
    }
    template <typename Fn, typename Tp>
    constexpr bool _call_is_nothrow_impl(_invoke_member_object_ref) {
        using Up = unwrap_reference_t<Tp>;
        return noexcept(declval<Up>().*declval<Fn>());
    }
    template <typename Fn, typename Tp>
    constexpr bool _call_is_nothrow_impl(_invoke_member_object_deref) {
        return noexcept((*declval<Tp>()).*declval<Fn>());
    }
    template <typename Fn, typename... Args>
    constexpr bool _call_is_nothrow_impl(_invoke_other) {
        return noexcept(declval<Fn>()(declval<Args>()...));
    }

    template <typename Result, typename Fn, typename... Args>
    struct _call_is_nothrow_total_impl : bool_constant<_call_is_nothrow_impl<Fn, Args...>(typename Result::invoke_type{})> {};

    template <typename Fn, typename... Args>
    struct _call_is_nothrow : _call_is_nothrow_total_impl<invoke_result<Fn, Args...>, Fn, Args...> {};

    // Only inspect the call expression after invocation and result conversion are valid.
    template <typename Ret, typename Fn, typename Enable, typename... Args>
    struct _nothrow_invocable_result : false_type {};

    template <typename Ret, typename Fn, typename... Args>
    struct _nothrow_invocable_result<Ret, Fn,
        enable_if_t<is_invocable_impl<invoke_result<Fn, Args...>, Ret>::type::value>, Args...>
        : bool_constant<_call_is_nothrow<Fn, Args...>::value &&
            is_invocable_impl<invoke_result<Fn, Args...>, Ret>::nothrow_type::value> {};

    // is_invocable
    template <typename Functor, typename... Args>
    struct is_invocable
#if __has_builtin(__is_invocable)
        : bool_constant<__is_invocable(Functor, Args...)>
#else
        : is_invocable_impl<invoke_result<Functor, Args...>, void>::type
#endif
    {
        static_assert(_is_complete_or_unbounded_v<Functor>,
                        "Functor must be a complete class or unbounded array");
        static_assert((_is_complete_or_unbounded_v<Args> && ...),
                        "each argument type must be a complete class or unbounded array");
    };

    template <typename Functor, typename... Args>
    inline constexpr bool is_invocable_v = is_invocable<Functor, Args...>::value;

    // is_invocable_r
    template <typename Ret, typename Functor, typename... Args>
    struct is_invocable_r
#if __has_builtin(__is_invocable_r)
        : bool_constant<__is_invocable_r(Ret, Functor, Args...)>
#else
        : is_invocable_impl<invoke_result<Functor, Args...>, Ret>::type
#endif
    {
        static_assert(_is_complete_or_unbounded_v<Functor>,
                        "Functor must be a complete class or unbounded array");
        static_assert((_is_complete_or_unbounded_v<Args> && ...),
                        "each argument type must be a complete class or unbounded array");
        static_assert(_is_complete_or_unbounded_v<Ret>,
                        "Ret must be a complete class or unbounded array");
    };

    template <typename Ret, typename Functor, typename... Args>
    inline constexpr bool is_invocable_r_v = is_invocable_r<Ret, Functor, Args...>::value;

    // is_nothrow_invocable
    template <typename Functor, typename... Args>
    struct is_nothrow_invocable
#if __has_builtin(__is_nothrow_invocable)
        : bool_constant<__is_nothrow_invocable(Functor, Args...)>
#else
        : _nothrow_invocable_result<void, Functor, void, Args...>
#endif
    {
        static_assert(_is_complete_or_unbounded_v<Functor>,
                        "Functor must be a complete class or unbounded array");
        static_assert((_is_complete_or_unbounded_v<Args> && ...),
                        "each argument type must be a complete class or unbounded array");
    };

    template <typename Functor, typename... Args>
    inline constexpr bool is_nothrow_invocable_v = is_nothrow_invocable<Functor, Args...>::value;

    // is_nothrow_invocable_r
    template <typename Ret, typename Functor, typename... Args>
    struct is_nothrow_invocable_r
#if __has_builtin(__is_nothrow_invocable_r)
        : bool_constant<__is_nothrow_invocable_r(Ret, Functor, Args...)>
#else
        : _nothrow_invocable_result<Ret, Functor, void, Args...>
#endif
    {
        static_assert(_is_complete_or_unbounded_v<Functor>,
                        "Functor must be a complete class or unbounded array");
        static_assert((_is_complete_or_unbounded_v<Args> && ...),
                        "each argument type must be a complete class or unbounded array");
        static_assert(_is_complete_or_unbounded_v<Ret>,
                        "Ret must be a complete class or unbounded array");
    };

    template <typename Ret, typename Functor, typename... Args>
    inline constexpr bool is_nothrow_invocable_r_v = is_nothrow_invocable_r<Ret, Functor, Args...>::value;



} // namespace_mystl

#endif //INVOKE_TRAITS_H
