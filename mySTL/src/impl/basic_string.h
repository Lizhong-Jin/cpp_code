#pragma once

#ifndef BASIC_STRING_H
#define BASIC_STRING_H

#include <iostream>

#include "../memory.h"
#include "../iterator.h"
#include "../stdexcept.h"
#include "../functional.h"

namespace mystl {
#define MYSTL_DEBUG(expr) assert(expr)
#define THROW_LENGTH_ERROR_IF(expr, what) if((expr)) throw mystl::length_error(what)
#define THROW_OUT_OF_RANGE_IF(expr, what) if((expr)) throw mystl::out_of_range_error(what)
#define THROW_RUNTIME_ERROR_IF(expr, what) if((expr)) throw mystl::runtime_error(what)

    template <typename CharType>
    class char_traits {
        using char_type = CharType;
    };


#undef MYSTL_DEBUG
#undef THROW_LENGTH_ERROR_IF
#undef THROW_OUT_OF_RANGE_IF
#undef THROW_RUNTIME_ERROR_IF
} // namespace mystl

#endif //BASIC_STRING_H