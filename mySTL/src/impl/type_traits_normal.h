#pragma once

#ifndef TYPE_TRAITS_NORMAL_H
#define TYPE_TRAITS_NORMAL_H

#include <cstddef>

namespace mystl {

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#ifndef __has_extension
#define __has_extension(x) 0
#endif

    template <class T, T v>
    struct integral_constant {
        static constexpr T value = v;
        
        using value_type = T;
        using type = integral_constant;

        constexpr operator value_type() const noexcept {return value;}
        constexpr value_type operator()() const noexcept {return value;}
    };
    template <bool b>
    using bool_constant = integral_constant<bool, b>;
    typedef bool_constant<true> true_type;
    typedef bool_constant<false> false_type;

    // declval
    template <typename T>
    T&& declval() noexcept;

    // is_reference
    template <typename T> struct is_lvalue_reference      : false_type {};
    template <typename T> struct is_lvalue_reference<T&>  : true_type {};
    template <typename T> struct is_rvalue_reference      : false_type {};
    template <typename T> struct is_rvalue_reference<T&&> : true_type {};
    template <typename T> struct is_reference : bool_constant<is_lvalue_reference<T>::value || is_rvalue_reference<T>::value> {};

    template <typename T> inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;
    template <typename T> inline constexpr bool is_rvalue_reference_v = is_rvalue_reference<T>::value;
    template <typename T> inline constexpr bool is_reference_v = is_reference<T>::value;

    // remove_cv : remove const and volatile
    template <typename T> struct remove_const                   {using type = T;};
    template <typename T> struct remove_const<const T>          {using type = T;};
    template <typename T> struct remove_volatile                {using type = T;};
    template <typename T> struct remove_volatile<volatile T>    {using type = T;};
    template <typename T>
    struct remove_cv {using type = typename remove_const<typename remove_volatile<T>::type>::type;};
    template <typename T>
    using remove_cv_t = typename remove_cv<T>::type;

    // add_cv : add const and volatile
    template <typename T> struct add_const {using type = const T;};
    template <typename T> struct add_volatile {using type = volatile T;};;
    template <typename T>
    struct add_cv {using type = typename add_const<typename add_volatile<T>::type>::type;};
    template <typename T>
    using add_cv_t = typename add_cv<T>::type;

    // is_const && is_volatile
#if __has_builtin(__is_const)
    template <typename T> struct is_const : bool_constant<__is_const(T)> {};
#else
    template <typename T> struct is_const : false_type {};
    template <typename T> struct is_const<const T> : true_type {};
#endif

#if __has_builtin(__is_volatile)
    template <typename T> struct is_volatile : bool_constant<__is_volatile(T)> {};
#else
    template <typename T> struct is_volatile : false_type {};
    template <typename T> struct is_volatile<volatile T> : true_type {};
#endif

    template <typename T> inline constexpr bool is_const_v = is_const<T>::value;
    template <typename T> inline constexpr bool is_volatile_v = is_volatile<T>::value;

    // remove_reference
    template <typename T> struct remove_reference {using type=T;};
    template <typename T> struct remove_reference<T&> {using type=T;};
    template <typename T> struct remove_reference<T&&> {using type=T;};
    template <typename T> using remove_reference_t = typename remove_reference<T>::type;

    // remove_cv_ref : remove cv and reference
    template <typename T>
    struct remove_cv_ref {using type = remove_cv_t<remove_reference_t<T>>;};
    template <typename T>
    using remove_cv_ref_t = typename remove_cv_ref<T>::type;

    // add_lvalue_reference
    template <typename T, typename = void> struct add_lvalue_reference {using type = T;};
    template <typename T> struct add_lvalue_reference<T, decltype(void(static_cast<T&>(declval<T>())))> {using type = T&;};
    template <typename T> using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;

    // add_rvalue_reference
    template <typename T, typename = void> struct add_rvalue_reference {using type = T;};
    template <typename T> struct add_rvalue_reference<T, decltype(void(static_cast<T&&>(declval<T>())))> {using type = T&&;};
    template <typename T> using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;

    // remove_pointer
    template <typename T> struct remove_pointer {using type = T;};
    template <typename T> struct remove_pointer<T*> {using type = T;};
    template <typename T> struct remove_pointer<T* const> {using type = T;};
    template <typename T> struct remove_pointer<T* volatile> {using type = T;};
    template <typename T> struct remove_pointer<T* const volatile> {using type = T;};
    template <typename T> using remove_pointer_t = typename remove_pointer<T>::type;

    // add_pointer
    template <typename T, typename = void> struct add_pointer {using type = T;};
    template <typename T> struct add_pointer<T, decltype(void(static_cast<T*>(nullptr)))> {using type = T*;};
    template <typename T> using add_pointer_t = typename add_pointer<T>::type;

    // enable_if
    template <bool cond, typename T = void> struct enable_if {};
    template <typename T> struct enable_if<true, T> {using type = T;};
    template <bool cond, typename T = void> using enable_if_t = typename enable_if<cond, T>::type;

    // type_identity
    template <typename T> struct type_identity {using type = T;};
    template <typename T> using type_identity_t = typename type_identity<T>::type;

    // ***************************************************************************************************
    // _is_complete_or_unbounded
    template <typename> struct is_object;
    template <typename> struct is_unbounded_array;
    template <typename> struct is_void;
    template <typename> struct is_function;

    template <typename T> struct _maybe_complete_object_type
                                : bool_constant<is_object<T>::value && !is_unbounded_array<T>::value> {};

    template <typename T, typename = enable_if_t<_maybe_complete_object_type<T>::value>, size_t = sizeof(T)>
    constexpr true_type _is_complete_or_unbounded(type_identity<T>) {return {};}

    template <typename Type_Identity, typename Nested_Type = typename Type_Identity::type>
    constexpr bool_constant<is_unbounded_array<Nested_Type>::value || is_reference_v<Nested_Type>
                            || is_void<Nested_Type>::value || is_function<Nested_Type>::value>
                        _is_complete_or_unbounded(Type_Identity) {return {};}

    template <typename T>
    inline constexpr bool _is_complete_or_unbounded_v = decltype(_is_complete_or_unbounded(type_identity<T>{}))::value;

    // ***************************************************************************************************
    // ************************* primary type categories *************************************************
    // is_void
    template <typename T> struct is_void : false_type {};
    template <>           struct is_void<void> : true_type {};
    template <typename T> struct is_void<const T> : is_void<T> {};
    template <typename T> struct is_void<volatile T> : is_void<T> {};
    template <typename T> struct is_void<const volatile T> : is_void<T> {};
    template <typename T> inline constexpr bool is_void_v = is_void<T>::value;

    // is_cv_void
    template <typename T> struct is_cv_void : is_void<remove_cv_ref_t<T>> {};
    template <typename T> inline constexpr bool is_cv_void_v = is_cv_void<T>::value;

    // is_null_pointer
    template <typename T> struct is_null_pointer_impl : false_type {};
    template <>           struct is_null_pointer_impl<decltype(nullptr)> : true_type {};
    template <typename T> struct is_null_pointer : is_null_pointer_impl<remove_cv_t<T>> {};
    template <typename T> inline constexpr bool is_null_pointer_v = is_null_pointer<T>::value;

    // is_integral
    template <typename T> struct is_integral_base : false_type {};

    template <> struct is_integral_base<bool> : true_type {};
    template <> struct is_integral_base<char> : true_type {};
    template <> struct is_integral_base<signed char> : true_type {};
    template <> struct is_integral_base<unsigned char> : true_type {};
#if defined(__cpp_char8_t_)
    template <> struct is_integral_base<char8_t> : true_type {};
#endif
    template <> struct is_integral_base<char16_t> : true_type {};
    template <> struct is_integral_base<char32_t> : true_type {};
    template <> struct is_integral_base<wchar_t> : true_type {};

    template <> struct is_integral_base<short> : true_type {};
    template <> struct is_integral_base<unsigned short> : true_type {};
    template <> struct is_integral_base<int> : true_type {};
    template <> struct is_integral_base<unsigned int> : true_type {};
    template <> struct is_integral_base<long> : true_type {};
    template <> struct is_integral_base<unsigned long> : true_type {};
    template <> struct is_integral_base<long long> : true_type {};
    template <> struct is_integral_base<unsigned long long> : true_type {};
#if defined(__SIZEOF_INT128__)
    template <> struct is_integral_base<__int128> : true_type {};
    template <> struct is_integral_base<unsigned __int128> : true_type {};
#endif

    template <typename T> struct is_integral : is_integral_base<remove_cv_t<T>> {};
    template <typename T> inline constexpr bool is_integral_v = is_integral<T>::value;

    // is_floating_point
    template <typename T> struct is_floating_point_base : false_type {};

    template <> struct is_floating_point_base<float> : true_type {};
    template <> struct is_floating_point_base<double> : true_type {};
    template <> struct is_floating_point_base<long double> : true_type {};

#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__) || __has_extension(float128)
    template <> struct is_floating_point_base<__float128> : true_type {};
#endif

#if defined(__FLT16_MANT_DIG__)
    template <> struct is_floating_point_base<_Float16> : true_type {};
#endif

#if defined(__FLT32_MANT_DIG__)
    template <> struct is_floatint_point_base<_Float32> : true_type {};
#endif

    template <typename T> struct is_floating_point : is_floating_point_base<remove_cv_t<T>> {};
    template <typename T> inline constexpr bool is_floating_point_v = is_floating_point<T>::value;

    // is_array
    template <typename T> struct is_array :                         false_type {};
    template <typename T, unsigned int N> struct is_array<T[N]> :   true_type {};
    template <typename T> struct is_array<T[]> :                    true_type {};
    template <typename T> inline constexpr bool is_array_v = is_array<T>::value;

    // is_class
    namespace detail {
        template <typename T> static auto is_class_like(int T::*) -> true_type;
        template <typename> static auto is_class_like(...) -> false_type;
    }

    template <typename T>
    struct is_class : bool_constant<
#if __has_builtin(__is_class)
        __is_class(remove_cv_t<T>)
#else
            (detail::is_class_like_v<remove_cv_t<T>(nullptr)>::value
        #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER) || __has_builtin(__is_union)
            && !__is_union(remove_cv_t<T>)
        #endif
        )
#endif
    > {}; // is_class

    template <typename T> static constexpr bool is_class_v = is_class<T>::value;

    // is_union
    namespace detail {
        template <typename T> struct try_derive : T {};
        template <typename T> static auto can_derive_impl(int) -> decltype(sizeof(try_derive<T>), true_type{});
        template <typename T> static auto can_derive_impl(...) -> false_type;

        template <typename T> struct can_derive : decltype(can_derive_impl<T>(0)) {};
    }

    template <typename T>
    struct is_union : bool_constant<
#if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER) || __has_builtin(__is_union)
        __is_union(remove_cv_t<T>)
#else
            (decltype(detail::is_class_like<remove_cv_t<T>>(nullptr))::value
                && !detail::can_derive<remove_cv_t<T>>::value
        #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER) || __has_builtin(__is_final)
                && !__is_final(remove_cv_t<T>)
        #endif
                )
#endif
        > {};

    template <typename T> inline constexpr bool is_union_v = is_union<T>::value;

    // is_function
    template <typename T> struct is_function
#if __has_builtin(__is_function)
        : bool_constant<__is_function(T)>
#else
        : false_type
#endif
    {};

#if !__has_builtin(__is_function)
#define MY_FN_SPEC_1(Q, NOEX) \
    template <class R, class... A> struct is_function<R(A...) Q NOEX> : true_type {}; \
    template <class R>             struct is_function<R(...)  Q NOEX> : true_type {};

#define MY_FN_SPEC_REF(NOEX) \
    MY_FN_SPEC_1(               , NOEX) \
    MY_FN_SPEC_1(const          , NOEX) \
    MY_FN_SPEC_1(volatile       , NOEX) \
    MY_FN_SPEC_1(const volatile , NOEX) \
    MY_FN_SPEC_1(&              , NOEX) \
    MY_FN_SPEC_1(const &        , NOEX) \
    MY_FN_SPEC_1(volatile &     , NOEX) \
    MY_FN_SPEC_1(const volatile &, NOEX) \
    MY_FN_SPEC_1(&&             , NOEX) \
    MY_FN_SPEC_1(const &&       , NOEX) \
    MY_FN_SPEC_1(volatile &&    , NOEX) \
    MY_FN_SPEC_1(const volatile &&, NOEX)

    MY_FN_SPEC_REF(/* noexcept off */);
#if defined(__cpp_noexcept_function_type)
#define MY_NOEX noexcept
    MY_FN_SPEC_REF(MY_NOEX)
#undef MY_NOEX
#endif

#undef MY_FN_SPEC_REF
#undef MY_FN_SPEC_1
#endif

    template <typename T> inline constexpr bool is_function_v = is_function<T>::value;

    // is_pointer
    template <typename> struct is_pointer_impl : false_type {};
    template <typename T> struct is_pointer_impl<T*> : true_type {};
    template <typename T> struct is_pointer : is_pointer_impl<remove_cv_t<T>> {};
    template <typename T> inline constexpr bool is_pointer_v = is_pointer<T>::value;

    // is_lvalue_reference (has already been accomplished earlier)

    // is_rvalue_reference (has already been accomplished earlier)

    // is_member_object_pointer
#if __has_builtin(__is_member_object_pointer)
    template <typename T> struct is_member_object_pointer : bool_constant<__is_member_object_pointer(T)> {};
#else
    template <typename t> struct is_member_object_pointer : false_type {};
    template <typename U, typename T> struct is_member_object_pointer<U T::*> : bool_constant<!is_function_v<U>> {};
#endif

    template <typename T> inline constexpr bool is_member_object_pointer_v = is_member_object_pointer<T>::value;

    // is_member_function_pointer
    template <typename T> struct is_member_function_pointer
#if __has_builtin(__is_member_function_pointer)
    : bool_constant<__is_member_function_pointer(T)>
#else
    : false_type
#endif
    {};

#if !__has_builtin(__is_member_function_pointer)
#define MY_MFP_SPEC_1(Q, NOEX) \
    template <typename R, typename C, typename... Arg> struct is_member_function_pointer<R(C::*)(Arg...) Q NOEX> : true_type {};

#define MY_MFP_SPEC_REF(NOEX) \
    MY_MFP_SPEC_1(               , NOEX) \
    MY_MFP_SPEC_1(const          , NOEX) \
    MY_MFP_SPEC_1(volatile       , NOEX) \
    MY_MFP_SPEC_1(const volatile , NOEX) \
    MY_MFP_SPEC_1(&              , NOEX) \
    MY_MFP_SPEC_1(const &        , NOEX) \
    MY_MFP_SPEC_1(volatile &     , NOEX) \
    MY_MFP_SPEC_1(const volatile &, NOEX) \
    MY_MFP_SPEC_1(&&             , NOEX) \
    MY_MFP_SPEC_1(const &&       , NOEX) \
    MY_MFP_SPEC_1(volatile &&    , NOEX) \
    MY_MFP_SPEC_1(const volatile &&, NOEX)

    MY_MFP_SPEC_REF(/* noexcept off */);
#if defined(__cpp_noexcept_function_type)
#define MY_NOEX noexcept
    MY_MFP_SPEC_REF(MY_NOEX)
#undef MY_NOEX
#endif

#undef MY_MFP_SPEC_REF
#undef MY_MFP_SPEC_1
#endif

    template <typename T> inline constexpr bool is_member_function_pointer_v = is_member_function_pointer<T>::value;

    // is_member_pointer
#if __has_builtin(__is_member_pointer)
    template <typename T> struct is_member_pointer : bool_constant<__is_member_pointer(T)> {};
#else
    template <typename T>
    struct is_member_pointer : bool_constant<is_member_object_pointer_v<T> || is_member_function_pointer_v<T>> {};
#endif

    template <typename T> inline constexpr bool is_member_pointer_v = is_member_pointer<T>::value;

    // is_enum
#if __has_builtin(__is_member_function_pointer)
    template <typename T> struct is_enum : bool_constant<__is_enum(T)> {};
#else
    template <typename T> struct is_enum : bool_constant<
                                    !is_class_v<T> && !is_union_v<T> && !is_integral_v<T> && is_pointer_v<T>
                                    && !is_member_pointer_v<T>
                                    && !is_floating_point_v<T> && !is_reference_v<T> && !is_void_v<T>> {};
#endif

    template <typename T> inline constexpr bool is_enum_v = is_enum<T>::value;

    // **********************************************************************************************************
    // Supported operations

    // void_t
    template<typename...>
    struct make_void {
        using type = void;
    };

    template<typename... Ts>
    using void_t = typename make_void<Ts...>::type;

    // is_pair
    template<typename T1, typename T2>
    struct pair;

    template<typename T>
    struct is_pair : false_type {
    };

    template<typename T1, typename T2>
    struct is_pair<pair<T1, T2> > : true_type {};

    // is_default_constructible
    template <typename T, typename = void>
    struct is_default_constructible : false_type {};
    template <typename T>
    struct is_default_constructible<T, decltype(void(T()))> : true_type {};
    template <typename T>
    inline constexpr bool is_default_constructible_v = is_default_constructible<T>::value;

    // is_nothrow_default_constructible
    template <typename T, typename = void>
    struct is_nothrow_default_constructible : false_type {};
    template <typename T>
    struct is_nothrow_default_constructible<T, decltype(void(T())) > : bool_constant<noexcept(T())> {};
    template <typename T>
    inline constexpr bool is_nothrow_default_constructible_v = is_nothrow_default_constructible<T>::value;

    // is_constructible
    template <typename T, typename... Args>
    struct is_constructible {
    private:
        template <typename U, typename... A>
        static auto test(int) -> decltype(U(declval<A>()...), true_type{});

        template <typename, typename...>
        static false_type test(...);
    public:
        static constexpr bool value = decltype(test<T, Args...>(0))::value;
    };
    template <typename T, typename... Args>
    inline constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

    // is_nothrow_constructible
    template <typename T, typename... Args>
    struct is_nothrow_constructible {
    private:
        template <typename U, typename... A>
        static auto test(int) -> decltype(U(declval<A>()...), bool_constant<noexcept(U(declval<A>()...))>{});

        template <typename, typename...>
        static false_type test(...);
    public:
        static constexpr bool value = decltype(test<T, Args...>(0))::value;
    };
    template <typename T, typename... Args>
    inline constexpr bool is_nothrow_constructible_t = is_nothrow_constructible<T, Args...>::value;

    // is_copy_constructible
    template <typename T, typename = void>
    struct is_copy_constructible : false_type {};
    template <typename T>
    struct is_copy_constructible<T, decltype(void(T(declval<const T&>())))> : true_type {};
    template <typename T>
    inline constexpr bool is_copy_constructible_v = is_copy_constructible<T>::value;

    // is_nothrow_copy_constructible
    template <typename T>
    struct is_nothrow_copy_constructible {
    private:
        template <typename U>
        static auto test(int) -> decltype(U(declval<const U&>()), bool_constant<noexcept(U(declval<const U&>()))>{});
        template <typename>
        static false_type test(...);
    public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };
    template <typename T>
    inline constexpr bool is_nothrow_copy_constructible_v = is_nothrow_copy_constructible<T>::value;

    // is_move_constructible
    template <typename T, typename = void>
    struct is_move_constructible : false_type {};
    template <typename T>
    struct is_move_constructible<T, decltype(void(declval<T&&>()))> : true_type {};
    template <typename T>
    inline constexpr bool is_move_constructible_v = is_move_constructible<T>::value;

    // is_nothrow_move_constructible
    template <typename T>
    struct is_nothrow_move_constructible {
    private:
        template <typename U>
        static auto test(int) -> decltype(U(declval<U&&>()), bool_constant<noexcept(U(declval<U&&>()))>{});
        template <typename>
        static false_type test(...);
    public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };
    template <typename T>
    inline constexpr bool is_nothrow_move_constructible_v = is_nothrow_move_constructible<T>::value;

    // is_copy_assignable
    template <typename T, typename = void>
    struct is_copy_assignable : false_type {};
    template <typename T>
    struct is_copy_assignable<T, decltype(declval<T&>() = declval<const T&>())> : true_type {};
    template <typename T>
    inline constexpr bool is_copy_assignable_v = is_copy_assignable<T>::value;

    // is_nothrow_copy_assignable
    template <typename T>
    struct is_nothrow_copy_assignable {
    private:
        template <typename U>
        static auto test(int) -> decltype(declval<U&>() = declval<const U&>(),
                                bool_constant<noexcept(declval<U&>() = declval<const U&>())>{});
        template <typename>
        static false_type test(...);
    public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };
    template <typename T>
    inline constexpr bool is_nothrow_copy_assignable_v = is_nothrow_copy_assignable<T>::value;

    // is_move_assignable
    template <typename T, typename = void>
    struct is_move_assignable : false_type {};
    template <typename T>
    struct is_move_assignable<T, void_t<decltype(declval<T&>() = declval<T&&>())>> : true_type {};
    template <typename T>
    inline constexpr bool is_move_assignable_v = is_move_assignable<T>::value;

    // is_nothrow_move_assignable
    template <typename T>
    struct is_nothrow_move_assignable {
    private:
        template <typename U>
        static auto test(int) -> decltype(declval<U&>() = declval<U&&>(),
                                bool_constant<noexcept(declval<U&>() = declval<U&&>())>{});
        template <typename>
        static false_type test(...);
    public:
        static constexpr bool value = !is_void_v<T> && !is_const_v<T>
                                        && !is_reference_v<T> && decltype(test<T>(0))::value;
    };
    template <typename T>
    inline constexpr bool is_nothrow_move_assignable_v = is_nothrow_move_assignable<T>::value;

    // is_destructible
    template <typename T, typename = void>
    struct is_destructible_impl : false_type {};
    template <typename T>
    struct is_destructible_impl<T, decltype(void(declval<T&>().~T()))> : true_type {};
    template <typename T>
    struct is_destructible : bool_constant<!is_void_v<T> && !is_reference_v<T>
                                        && !is_function_v<T> && is_destructible_impl<T>::value> {};
    template <typename T>
    inline constexpr bool is_destructible_v = is_destructible<T>::value;

    // is_nothrow_destructible
    template <typename T>
    struct is_nothrow_destructible {
    private:
        template <typename U>
        static auto test(int) -> decltype(declval<U&>().~U(), bool_constant<noexcept(declval<U&>().~U())>{});
        template <typename>
        static false_type test(...);
    public:
        static constexpr bool value = !is_void_v<T> && !is_reference_v<T>
                                        && !is_function_v<T> && decltype(test<T>(0))::value;
    };
    template <typename T>
    inline constexpr bool is_nothrow_destructible_v = is_nothrow_destructible<T>::value;

    // is_convertible
    template <typename, typename, typename = void>
    struct is_convertible : false_type {};
    template <typename From, typename To>
    struct is_convertible<From, To, decltype(void(static_cast<To>(declval<From>())))> : true_type {};
    template <typename From, typename To>
    inline constexpr bool is_convertible_v = is_convertible<From, To>::value;

    // is_nothrow_convertible
    template <typename From, typename To>
    struct is_nothrow_convertible : bool_constant<is_convertible_v<From, To>
                                                && noexcept(static_cast<To>(declval<From>()))> {};
    template <typename From, typename To>
    inline constexpr bool is_nothrow_convertible_v = is_nothrow_convertible<From, To>::value;

    // is_same
    template <typename T, typename U> struct is_same : false_type {};
    template <typename T> struct is_same<T, T> : true_type {};
    template <typename T, typename U> inline constexpr bool is_same_v = is_same<T, U>::value;

    // ***************************************************************************************************
    // is_trivially......
    // dependent on the built-in functions of compiler
    // ****** compiler feature detection ******
#if defined(__clang__) || defined(__GNUC__)
    #define HAS_GCC_LIKE_BUILTINS 1
#else
    #define HAS_GCC_LIKE_BUILTINS 0
#endif

#if defined(__MSC_VER__)
    #define HAS_MSVC_BUILTINS 1
#else
    #define HAS_MSVC_BUILTINS 0
#endif

#if HAS_GCC_LIKE_BUILTINS || HAS_MSVC_BUILTINS
    #define MY_IS_TRIVIALLY_CONSTRUCTIBLE(T, ...)  __is_trivially_constructible(T, __VA_ARGS__)
    #define MY_IS_TRIVIALLY_ASSIGNABLE(To, From)   __is_trivially_assignable(To, From)
    #define MY_IS_TRIVIALLY_DESTRUCTIBLE(T)        __is_trivially_destructible(T)
    #define MY_IS_TRIVIALLY_COPYABLE(T)            __is_trivially_copyable(T)
#else
    #define MY_IS_TRIVIALLY_CONSTRUCTIBLE(T, ...)  0
    #define MY_IS_TRIVIALLY_COPY_CONSTRUCTIBLE(T)  0
    #define MY_IS_TRIVIALLY_ASSIGNABLE(To, From)   0
    #define MY_IS_TRIVIALLY_DESTRUCTIBLE(T)        0
    #define MY_IS_TRIVIALLY_COPYABLE(T)            0
#endif

    // is_trivially_constructible
    template <typename T, typename... Args>
    struct is_trivially_constructible : bool_constant<(MY_IS_TRIVIALLY_CONSTRUCTIBLE(T, Args...))> {};
    template <typename T, typename... Args>
    inline constexpr bool is_trivially_constructible_v = is_trivially_constructible<T, Args...>::value;

    // is_trivially_default_constructible
    template <typename T>
    struct is_trivially_default_constructible : is_trivially_constructible<T> {};
    template <typename T>
    inline constexpr bool is_trivially_default_constructible_v = is_trivially_default_constructible<T>::value;

    // is_trivially_copy_constructible
    template <typename T>
    struct is_trivially_copy_constructible : is_trivially_constructible<T> {};
    template <typename T>
    inline constexpr bool is_trivially_copy_constructible_v = is_trivially_copy_constructible<T>::value;

    // is_trivially_move_constructible
    template <typename T>
    struct is_trivially_move_constructible : bool_constant<(MY_IS_TRIVIALLY_CONSTRUCTIBLE(T, T&&))> {};
    template <typename T>
    inline constexpr bool is_trivially_move_constructible_v = is_trivially_move_constructible<T>::value;

    // is_trivially_assignable
    template <typename To, typename From>
    struct is_trivially_assignable : bool_constant<(MY_IS_TRIVIALLY_ASSIGNABLE(To, From))> {};
    template <typename To, typename From>
    inline constexpr bool is_trivially_assignable_v = is_trivially_assignable<To, From>::value;

    // is_trivially_copy_assignable
    template <typename T>
    struct is_trivially_copy_assignable : is_trivially_assignable<add_lvalue_reference_t<T>, const T&> {};
    template <typename T>
    inline constexpr bool is_trivially_copy_assignable_v = is_trivially_copy_assignable<T>::value;

    // is_trivially_move_assignable
    template <typename T>
    struct is_trivially_move_assignable : is_trivially_assignable<add_lvalue_reference_t<T>, T&&> {};
    template <typename T>
    inline constexpr bool is_trivially_move_assignable_v = is_trivially_move_assignable<T>::value;

    // is_trivially_destructible
    template <typename T>
    struct is_trivially_destructible : bool_constant<(MY_IS_TRIVIALLY_DESTRUCTIBLE(T))> {};
    template <typename T>
    inline constexpr bool is_trivially_destructible_v = is_trivially_destructible<T>::value;

    // is_trivially_copyable
    template <typename T>
    struct is_trivially_copyable : bool_constant<(MY_IS_TRIVIALLY_COPYABLE(T))> {};
    template <typename T>
    inline constexpr bool is_trivially_copyable_v = is_trivially_copyable<T>::value;



    // ***************************************************************************************************
    // ************************ composite type categories ************************************************
    // is_arithmetic
    template <typename T>
    struct is_arithmetic : bool_constant<is_integral_v<T> || is_floating_point_v<T>> {};
    template <typename T> inline constexpr bool is_arithmetic_v = is_arithmetic<T>::value;

    // is_fundamental
    template <typename T>
    struct is_fundamental : bool_constant<is_arithmetic_v<T> || is_void_v<T> || is_null_pointer_v<T>> {};
    template <typename T> inline constexpr bool is_fundamental_v = is_fundamental<T>::value;

    // is_compound
    template <typename T> struct is_compound : bool_constant<!is_fundamental_v<T>> {};
    template <typename T> inline constexpr bool is_compound_v = is_compound<T>::value;

    // is_reference (has been accomplished before)

    // is_member_pointer (has been accomplished before)

    // is_scalar
    template <typename T>
    struct is_scalar : bool_constant<is_arithmetic_v<T> || is_enum_v<T> || is_pointer_v<T>
                                    || is_member_pointer_v<T> || is_null_pointer_v<T>> {};
    template <typename T> inline constexpr bool is_scalar_v = is_scalar<T>::value;

    // is_object
    template <typename T>
    struct is_object : bool_constant<is_scalar_v<T> || is_array_v<T> || is_union_v<T> || is_class_v<T>> {};
    template <typename T> inline constexpr bool is_object_v = is_object<T>::value;

    // ***************************************************************************************************
    // type properties
    // is_const && is_volatile (has been accomplished before)

    // is_final
#if __has_builtin(__is_final)
    template <typename T> struct is_final : bool_constant<__is_final(T)> {};
#else
    template <typename T> struct is_final : false_type {};
#endif

    template <typename T> inline constexpr bool is_final_v = is_final<T>::value;

    // is_empty
#if __has_builtin(__is_empty)
    template <typename T> struct is_empty : bool_constant<__is_empty(T)> {};
#else
    namespace detail {
        template <typename T, bool = is_class_v<T> && !is_final_v<T>> struct is_empty_impl : false_type {};
        template <typename T> struct is_empty_impl<T, true> {
        private:
            struct one {char c;};
            struct two : T {char c;};
        public:
            static constexpr bool value = sizeof(one) == sizeof(two);
        };
    }
    template <typename T> struct is_empty : detail::is_empty_impl<T> {};
#endif

    template <typename T> inline constexpr bool is_empty_v = is_empty<T>::value;

    // is_standard_layout
#if __has_builtin(__is_standard_layout)
    template <typename T> struct is_standard_layout : bool_constant<__is_standard_layout(T)> {};
#else
    namespace detail {
        template <typename T> struct is_standard_layout_impl
            : bool_constant<!is_void_v<T> && !is_reference_v<T> && !is_class_v<T> && !is_function_v<T> && !is_union_v<T>> {};
    }
    template <typename T> struct is_standard_layout : detail::is_standard_layout_impl<remove_cv_t<T>> {};
#endif

    template <typename T> inline constexpr bool is_standard_layout_v = is_standard_layout<T>::value;

    // is_signed
    template <typename T, bool = is_arithmetic_v<T>> struct is_signed : bool_constant<T(-1)<T(0)> {};
    template <typename T> struct is_signed<T, false> : false_type {};
    template <typename T> inline constexpr bool is_signed_v = is_signed<T>::value;

    // is_unsigned
    template <typename T, bool = is_arithmetic_v<T>> struct is_unsigned : bool_constant<T(0)<T(-1)> {};
    template <typename T> struct is_unsigned<T, false> : false_type {};
    template <typename T> inline constexpr bool is_unsigned_v = is_unsigned<T>::value;

    // is_polymorphic
    namespace detail {
        template <typename T> true_type detect_is_polymorphic(
            decltype(dynamic_cast<const volatile void*>(static_cast<T*>(nullptr))));
        template <typename t> false_type detect_is_polymorphic(...);
    }

    template <typename T> struct is_polymorphic : decltype(detail::detect_is_polymorphic<T>(nullptr)) {};
    template <typename T> inline constexpr bool is_polymorphic_v = is_polymorphic<T>::value;

    // is_abstract
#if __has_builtin(__is_abstract)
    template <typename T> struct is_abstract : bool_constant<__is_abstract(T)> {};
#else
    template <typename T> struct is_abstract : false_type {};
#endif

    template <typename T> inline constexpr bool is_abstract_v = is_abstract<T>::value;

    // is_bounded_array
    template <typename T> struct is_bounded_array : false_type {};
    template <typename T, size_t N> struct is_bounded_array<T[N]> : true_type {};
    template <typename T> inline constexpr bool is_bounded_array_v = is_bounded_array<T>::value;

    // is_unbounded_array
    template <typename T> struct is_unbounded_array : false_type {};
    template <typename T> struct is_unbounded_array<T[]> : true_type {};
    template <typename T> inline constexpr bool is_unbounded_array_v = is_unbounded_array<T>::value;

    // ***************************************************************************************************
    // property queries
    // alignment_of
#if __has_builtin(alignof)
    template <typename T> struct alignment_of {static constexpr size_t value = alignof(T);};
#else
    template <typename T> struct alignment_of {
    private:
        struct alignment_of_helper {char c; T t;};
    public:
        static constexpr size_t value = sizeof(alignment_of_helper)-sizeof(T);
    };
#endif

    template <typename T> inline constexpr size_t alignment_of_v = alignment_of<T>::value;

    // rank
    template <typename T> struct rank : integral_constant<size_t, 0> {};
    template <typename T, size_t N> struct rank<T[N]> : integral_constant<size_t, 1+rank<T>::value> {};
    template <typename T> struct rank<T[]> : integral_constant<size_t, 1+rank<T>::value> {};
    template <typename T> inline constexpr size_t rank_v = rank<T>::value;

    // extent
    template <typename T> struct extent {static constexpr unsigned int value = 0;};
    template <typename T, unsigned int N> struct extent<T[N]> {static constexpr unsigned int value = N;};
    template <typename T> struct extent<T[]> {static constexpr unsigned int value = 0;};
    template <typename T> inline constexpr unsigned int extent_v = extent<T>::value;

    // ***************************************************************************************************
    // array
    // remove_extent
    template <typename T> struct remove_extent {using type = T;};
    template <typename T, size_t N> struct remove_extent<T[N]> {using type = T;};
    template <typename T> struct remove_extent<T[]> {using type = T;};
    template <typename T> using remove_extent_t = typename remove_extent<T>::type;

    // remove_all_extent
    template <typename T> struct remove_all_extent {using type = T;};
    template <typename T, size_t N> struct remove_all_extent<T[N]> {using type = typename remove_all_extent<T>::type;};
    template <typename T> struct remove_all_extent<T[]> {using type = typename remove_all_extent<T>::type;};
    template <typename T> using remove_all_extent_t = typename remove_all_extent<T>::type;

    // ***************************************************************************************************
    // type relationships
    // is_same (has been accomplished before)

    // is_base_of
    namespace detail {
        template <typename B> true_type test_ptr_conv(const volatile B*);
        template <typename > false_type test_ptr_conv(const volatile void*);

        template <typename B, typename D>
        auto test_is_base_of(int) -> decltype(test_ptr_conv<B>(static_cast<D*>(nullptr)));
        template <typename, typename >
        auto test_is_base_of(...) -> false_type;
    }

#if __has_builtin(__is_base_of)
    template <typename Base, typename Derived>
    struct is_base_of : bool_constant<__is_base_of(Base, Derived)> {};
#else
    template <typename Base, typename Derived>
    struct is_base_of : bool_constant<is_class_v<Base> && is_class_v<Derived>
                                    && decltype(detail::test_is_base_of<Base, Derived>(0))::value> {};
#endif

    template <typename Base, typename Derived>
    inline constexpr bool is_base_of_v = is_base_of<Base, Derived>::value;

    // is_convertible && is_nothrow_convertible (has been accomplished before)

    // is_invocable && is_invocable_r && is_nothrow_invocable && is_nothrow_invocable_r
    // accomplished in invoke_traits.h

    // ***************************************************************************************************
    // Miscellaneous transformations
    // conditional
    template <bool B, typename T, typename F> struct conditional {using type = T;};
    template <typename T, typename F> struct conditional<false, T, F> {using type = F;};
    template <bool B, typename T, typename F> using conditional_t = typename conditional<B, T, F>::type;

    // decay
    template <typename T> struct decay {
    private:
        using U = remove_reference_t<T>;
    public:
        using type = conditional_t<is_array_v<U>, add_pointer_t<remove_extent_t<U>>,
                                conditional_t<is_function_v<U>, add_pointer_t<U>, remove_cv_t<U>>>;
    };
    template <typename T> using decay_t = typename decay<T>::type;

    // enable_if (has been accomplished before)

    // void_t (has been accomplished before)

    // common_type
    template <typename...> struct common_type {};
    template <typename T> struct common_type<T> : common_type<T, T> {};

    namespace detail {
        template <typename...> using void_t = void;
        template <typename T1, typename T2>
        using conditional_result_t = decltype(false ? declval<T1>() : declval<T2>());

        template <typename, typename, typename = void>
        struct decay_conditional_result {};
        template <typename T1, typename T2>
        struct decay_conditional_result<T1, T2, void_t<conditional_result_t<T1, T2>>>
                : decay<conditional_result_t<T1, T2>> {};

        template <typename T1, typename T2, typename = void>
        struct common_type_2_impl : decay_conditional_result<const T1&, const T2&> {};
        template <typename T1, typename T2>
        struct common_type_2_impl<T1, T2, void_t<conditional_result_t<T1, T2>>> : decay_conditional_result<T1, T2> {};
    }

    template <typename T1, typename T2>
    struct common_type<T1, T2> : conditional_t<is_same_v<T1, decay_t<T1>> && is_same_v<T2, decay_t<T2>>,
                                detail::common_type_2_impl<T1, T2>,
                                common_type<decay_t<T1>, decay_t<T2>>> {};

    namespace detail {
        template <typename AlwaysVoid, typename T1, typename T2, typename... Args>
        struct common_type_multi_impl {};
        template <typename T1, typename T2, typename... Args>
        struct common_type_multi_impl<void_t<typename  common_type<T1, T2>::type>, T1, T2, Args...>
                : common_type<typename common_type<T1, T2>::type, Args...> {};
    }

    template <typename T1, typename T2, typename... Args>
    struct common_type<T1, T2, Args...> : detail::common_type_multi_impl<void, T1, T2, Args...> {};

    template <typename... T> using common_type_t = typename common_type<T...>::type;

    // result_of && invoke_result (has been accomplished in invoke_traits.h)

    // type_identity (has been accomplished before)

    // unwrap_reference
    // forward declaration of reference_wrapper
    template <typename T> class reference_wrapper;

    template <typename T> struct unwrap_reference {using type = T;};
    template <typename T> struct unwrap_reference<reference_wrapper<T>> {using type = T&;};
    template <typename T> using unwrap_reference_t = typename unwrap_reference<T>::type;

    // unwrap_ref_decay
    template <typename T> struct unwrap_ref_decay : unwrap_reference<decay_t<T>> {};
    template <typename T> using unwrap_ref_decay_t = typename unwrap_ref_decay<T>::type;

    // ***************************************************************************************************
    // sign modifiers
    // make_signed && make_unsigned
    namespace detail {
        template <typename T> struct make_signed_impl {};
        template <typename T> struct make_unsigned_impl {};

        template <> struct make_signed_impl<unsigned char> {using type = signed char;};
        template <> struct make_signed_impl<unsigned short> {using type = short;};
        template <> struct make_signed_impl<unsigned int> {using type = int;};
        template <> struct make_signed_impl<unsigned long> {using type = long;};
        template <> struct make_signed_impl<unsigned long long> {using type = long long;};

        template <> struct make_unsigned_impl<signed char> {using type = unsigned char;};
        template <> struct make_unsigned_impl<short> {using type = unsigned short;};
        template <> struct make_unsigned_impl<int> {using type = unsigned int;};
        template <> struct make_unsigned_impl<long> {using type = unsigned long;};
        template <> struct make_unsigned_impl<long long> {using type = unsigned long long;};

        template <> struct make_signed_impl<char> {using type = signed char;};
        template <> struct make_unsigned_impl<char> {using type = unsigned char;};
        template <> struct make_signed_impl<wchar_t> {using type = wchar_t;};
        template <> struct make_unsigned_impl<wchar_t> {using type = wchar_t;};
#ifdef __cpp_char8_t
        template <> struct make_signed_impl<char8_t> {using type = char8_t;};
        template <> struct make_unsigned_impl<char8_t> {using type = char8_t;};
#endif
        template <> struct make_signed_impl<char16_t> {using type = char16_t;};
        template <> struct make_unsigned_impl<char16_t> {using type = char16_t;};
        template <> struct make_signed_impl<char32_t> {using type = char32_t;};
        template <> struct make_unsigned_impl<char32_t> {using type = char32_t;};
    }

    // make_signed
    template <typename T> struct make_signed {
    private:
        using U = remove_cv_t<T>;
        static_assert(is_integral_v<U> && is_same_v<U, bool>,
                    "make_signed<T> requires T to be an integral type excluding bool");
        using base_type = typename detail::make_signed_impl<U>::type;
    public:
        using type = conditional_t<is_const_v<T>, const base_type,
                                conditional_t<is_volatile_v<T>, volatile base_type, base_type>>;
    };

    template <typename T> using make_signed_t = typename make_signed<T>::type;

    // make_unsigned
    template <typename T> struct make_unsigned {
    private:
        using U = remove_cv_t<T>;
        static_assert(is_integral_v<U> && is_same_v<U, bool>,
                    "make_unsigned<T> requires T to be an integral type excluding bool");
        using base_type = typename detail::make_unsigned_impl<U>::type;
    public:
        using type = conditional_t<is_const_v<T>, const base_type,
                                conditional_t<is_volatile_v<T>, volatile base_type, base_type>>;
    };

    template <typename T> using make_unsigned_t = typename make_unsigned<T>::type;

    // ***************************************************************************************************
} // namespace mystl

#endif //NORMAL_TYPE_TRAITS_H
