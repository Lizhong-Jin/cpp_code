#pragma once

#ifndef EXCEPTION_H
#define EXCEPTION_H

namespace mystl {
    class exception {
    public:
        constexpr exception() noexcept;
        constexpr exception(const exception& other) noexcept;
        constexpr exception& operator=(const exception& other) noexcept = default;
        virtual ~exception() noexcept;
        [[nodiscard]] virtual const char* what() const noexcept {
            return "mystl::exception";
        }
    };
} // namespace mystl

#endif //EXCEPTION_H
