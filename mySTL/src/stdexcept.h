#pragma once

#ifndef STDEXCEPT_H
#define STDEXCEPT_H

#include "exception.h"

namespace mystl {
    // logic_error
    class logic_error : public exception {
    public:
        explicit constexpr logic_error(const char* msg) noexcept : _msg_(msg) {}
        [[nodiscard]] constexpr const char* what() const noexcept override {
            return _msg_;
        }
    protected:
        const char *_msg_;
    };

    // runtime_error
    class runtime_error : public exception {
    public:
        explicit constexpr runtime_error(const char* msg) noexcept : _msg_(msg) {}
        [[nodiscard]] constexpr const char* what() const noexcept override {
            return _msg_;
        }
    protected:
        const char* _msg_;
    };

    // invalid_error
    class invalid_error : public logic_error {
    public:
        explicit constexpr invalid_error(const char* what_arg) noexcept : logic_error(what_arg) {}
    };

    // domain_error
    class domain_error : public logic_error {
    public:
        explicit constexpr domain_error(const char* what_arg) noexcept : logic_error(what_arg) {}
    };

    // length_error
    class length_error : public logic_error {
    public:
        explicit constexpr length_error(const char* what_arg) noexcept : logic_error(what_arg) {}
    };

    // out_of_range
    class out_of_range : public logic_error {
    public:
        explicit constexpr out_of_range(const char* what_arg) noexcept : logic_error(what_arg) {}
    };

    // range_error
    class range_error : public runtime_error {
    public:
        explicit constexpr range_error(const char* what_arg) noexcept : runtime_error(what_arg) {}
    };

    // overflow_error
    class overflow_error : public runtime_error {
    public:
        explicit constexpr overflow_error(const char* what_arg) noexcept : runtime_error(what_arg) {}
    };

    // underflow_error
    class underflow_error : public runtime_error {
    public:
        explicit constexpr underflow_error(const char* what_arg) noexcept : runtime_error(what_arg) {}
    };

} // namespace mystl

#endif //STDEXCEPT_H
