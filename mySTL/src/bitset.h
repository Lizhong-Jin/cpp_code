#pragma once

#ifndef BITSET_H
#define BITSET_H

namespace mystl {
    template <size_t N>
    class bitset {
    private:
        using ValueType = unsigned long long;
        ValueType _M_w_[(N+63)>>6];
    public:
    };

} // namespace mystl

#endif //BITSET_H
