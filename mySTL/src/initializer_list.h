#pragma once

#ifndef INITIALIZER_LIST_H
#define INITIALIZER_LIST_H

#include <cstddef>

namespace mystl {
    template <typename T>
    class initializer_list {
    public:
        using value_type = T;
        using reference = const T&;
        using const_reference = const T&;
        using size_type = size_t;
        using iterator = const T*;
        using const_iterator = const T*;

        constexpr initializer_list() noexcept : _M_array(nullptr), _M_size(0) {}

        constexpr size_type size() const noexcept { return _M_size; }

        constexpr const_iterator begin() const noexcept { return _M_array; }

        constexpr const_iterator end() const noexcept { return begin() + size(); }
    private:
        iterator _M_array;
        size_type _M_size;

        constexpr initializer_list(const_iterator it, size_type l) : _M_array(it), _M_size(l) {}
    };

    template <typename T>
    constexpr const T* begin(initializer_list<T> ini_list) noexcept {return ini_list.begin();}

    template <typename T>
    constexpr const T* end(initializer_list<T> ini_list) noexcept {return ini_list.end();}
} // namespace mystl

#endif //INITIALIZER_LIST_H
