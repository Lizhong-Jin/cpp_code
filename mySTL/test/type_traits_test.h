#pragma once

#ifndef TYPE_TRAITS_TEST_H
#define TYPE_TRAITS_TEST_H
#include <iostream>

#include "../src/type_traits.h"

namespace mystl_test {
    // **********************************************************************************
    // Primary type categories test
    void foo();

    inline void is_void_test() {
        using mystl::void_t;
        using mystl::is_void;
        using mystl::is_void_v;
        static_assert(
            is_void_v<void> == true &&
            is_void_v<const void> == true &&
            is_void_v<volatile void> == true &&
            is_void_v<const volatile void> == true &&
            is_void_v<void_t<>> == true &&
            is_void_v<void*> == false &&
            is_void_v<int> == false &&
            is_void_v<decltype(foo)> == false &&
            is_void_v<is_void<void>> == false
        );
        std::cout << "is_void test accomplished" << std::endl;
    }

    inline void is_null_pointer_test() {
        using mystl::is_null_pointer_v;
        static_assert(is_null_pointer_v<decltype(nullptr)>);
        static_assert(!is_null_pointer_v<int*>);
        std::cout << "is_null_pointer test accomplished" << std::endl;
    }



    inline void is_integral_test() {
        using mystl::is_integral_v;
        static_assert(
            is_integral_v<float> == false &&
            is_integral_v<int*> == false &&
            is_integral_v<int> == true &&
            is_integral_v<const int> == true &&
            is_integral_v<bool> == true &&
            is_integral_v<char> == true
        );
        std::cout << "is_integral test accomplished" << std::endl;
    }

    inline void is_floating_point_test() {
        using mystl::is_floating_point_v;
        std::cout << "is_floating test accomplished" << std::endl;
    }

    inline void is_pointer_test() {
        using mystl::is_pointer_v;
        static_assert(!is_pointer_v<decltype(nullptr)>);
        static_assert(is_pointer_v<int*>);
        std::cout << "is_pointer test accomplished" << std::endl;
    }
} // namespace mystl_test

inline void primary_type_test() {
    std::cout << "**********************************************************************************" << std::endl;
    std::cout << "primary type test start" << std::endl;
    std::cout << "**********************************************************************************" << std::endl;
    mystl_test::is_void_test();
    mystl_test::is_null_pointer_test();
    mystl_test::is_integral_test();
    mystl_test::is_floating_point_test();
    mystl_test::is_pointer_test();
    std::cout << "**********************************************************************************" << std::endl;
    std::cout << "primary type test accomplished" << std::endl;
    std::cout << "**********************************************************************************" << std::endl;
    std::cout << std::endl;
}

inline void type_traits_test() {
    primary_type_test();
}

#endif //TYPE_TRAITS_TEST_H
