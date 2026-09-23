#pragma once

#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <random>
#include <type_traits>

using namespace std;

template <class T>
vector<vector<T>> generate_random_matrix(int rows, int cols, T low, T high) {
    random_device rint;
    mt19937_64 gen(rint());
    // 整型使用 uniform_int_distribution
    if constexpr (is_integral_v<T>) {
        uniform_int_distribution<T> dis(low, high);
        vector<vector<T>> mat(rows, vector<T>(cols));
        for (int i = 0; i < rows; ++i)
            for (int j = 0; j < cols; ++j)
                mat[i][j] = dis(gen);
        return mat;
    }

    // 浮点型使用 uniform_real_distribution
    else if constexpr (is_floating_point_v<T>) {
        uniform_real_distribution<T> dis(low, high);
        vector<vector<T>> mat(rows, vector<T>(cols));
        for (int i = 0; i < rows; ++i)
            for (int j = 0; j < cols; ++j)
                mat[i][j] = dis(gen);
        return mat;
    }

    // 其他类型编译时报错
    else {
        static_assert(is_arithmetic_v<T>, "T must be an arithmetic type");
    }
};

template <typename T>
void matrix_test(int N, T) {}