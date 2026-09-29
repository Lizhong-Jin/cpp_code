#pragma once

#ifndef EXCEPTION_H
#define EXCEPTION_H

namespace mystl {
    class exception {
    public:
        constexpr exception() noexcept = default;
        constexpr exception(const exception& other) noexcept = default;
        constexpr exception& operator=(const exception& other) noexcept = default;
        virtual constexpr ~exception() noexcept = default;
        [[nodiscard]] virtual const char* what() const noexcept {
            return "mystl::exception";
        }
    };
} // namespace mystl

#endif //EXCEPTION_H
