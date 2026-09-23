#pragma once

#ifndef ALGORITHM_TEST_H
#define ALGORITHM_TEST_H

#include <algorithm>
#include <iostream>
#include <chrono>
#include <random>
#include <vector>

#include "../src/algorithm.h"

namespace mystl_test {
#define TINY_SIZE 20
#define SMALL_SIZE 2000
#define LARGE_SIZE 20000
#define ENORMOUS_SIZE 1000000
#define VARY_ENORMOUS_SIZE 10000000

    // **********************************************************************************
    // all_of && any_of && none_of

    // **********************************************************************************


    // **********************************************************************************
    // is_sorted
    constexpr bool is_sorted_correctness_test() {
        constexpr int test_time = 100;
        std::random_device rd;
        std::mt19937 gen(rd());
        int vec[SMALL_SIZE];
        std::uniform_int_distribution<int> dist(0, SMALL_SIZE * 5);
        for (int i = 0; i < test_time; ++i) {
            for (int & j : vec) {
                j = dist(gen);
            }
            std::ranges::sort(vec);
            if (mystl::is_sorted(vec, vec + SMALL_SIZE) != std::ranges::is_sorted(vec)) {
                return false;
            }
        }
        return true;
    }

    constexpr void is_sorted_efficiency_test() {
        constexpr int test_time = 10;
        std::random_device rd;
        std::mt19937 gen(rd());
        constexpr int data_len = ENORMOUS_SIZE;
        int vec[data_len];
        std::uniform_int_distribution<int> dist(0, data_len * 5);
        double time1 = 0.0, time2 = 0.0;
        for (int i = 0; i < test_time; ++i) {
            for (int & j : vec) {
                j = dist(gen);
            }
            std::ranges::sort(vec);
            auto start = std::chrono::high_resolution_clock::now();
            bool b1 = mystl::is_sorted(vec, vec + data_len);
            auto end = std::chrono::high_resolution_clock::now();
            time1 += std::chrono::duration<double, std::milli>(end - start).count();
            start = std::chrono::high_resolution_clock::now();
            bool b2 = std::is_sorted(vec, vec + data_len);
            end = std::chrono::high_resolution_clock::now();
            time2 += std::chrono::duration<double, std::milli>(end - start).count();
            if (b1 != b2) {
                std::cout << "wrong result" << std::endl;
                return;
            }
        }
        std::cout << "is_sorted efficiency test begin" << std::endl;
        std::cout << "data size: " << data_len << std::endl;
        std::cout << "test times: " << test_time << std::endl;
        std::cout << "mystl::is_sorted average time spent: " << time1 / test_time << "ms" << std::endl;
        std::cout << "  std::is_sorted average time spent: " << time2 / test_time << "ms" << std::endl;
        std::cout << "is_sorted efficiency test accomplished" << std::endl;
        std::cout << std::endl;
    }

    // is_sorted_until
    constexpr bool is_sorted_until_correctness_test() {
        constexpr int test_time = 100;
        std::random_device rd;
        std::mt19937 gen(rd());
        int vec[SMALL_SIZE];
        std::uniform_int_distribution<int> dist(0, SMALL_SIZE * 5);
        std::uniform_int_distribution<int> range(0, SMALL_SIZE - 1);
        for (int i = 0; i < test_time; ++i) {
            for (int & j : vec) {
                j = dist(gen);
            }
            std::sort(vec, vec + range(gen));
            if (mystl::is_sorted_until(vec, vec + SMALL_SIZE) != std::ranges::is_sorted_until(vec)) {
                return false;
            }
        }
        return true;
    }

    // sort
    constexpr bool sort_correctness_test() {
        constexpr int test_time = 100;
        std::random_device rd;
        std::mt19937 gen(rd());
        int vec1[SMALL_SIZE];
        int vec2[SMALL_SIZE];
        std::uniform_int_distribution<int> dist(0, SMALL_SIZE * 5);
        for (int i = 0; i < test_time; ++i) {
            for (int j = 0; j < SMALL_SIZE; ++j) {
                int random_element = dist(gen);
                vec1[j] = random_element;
                vec2[j] = random_element;
            }
            mystl::sort(vec1, vec1 + SMALL_SIZE);
            std::ranges::sort(vec2);
            for (int j = 0; j < SMALL_SIZE; ++j) {
                if (vec1[j] != vec2[j]) return false;
            }
        }
        return true;
    }

    constexpr void sort_efficiency_test() {
        constexpr int test_time = 10;
        std::random_device rd;
        std::mt19937 gen(rd());
        constexpr int data_len = ENORMOUS_SIZE;
        int vec1[data_len];
        int vec2[data_len];
        std::uniform_int_distribution<int> dist(0, data_len * 5);
        double time1 = 0.0, time2 = 0.0;
        for (int i = 0; i < test_time; ++i) {
            for (int j = 0; j < data_len; ++j) {
                int random_element = dist(gen);
                vec1[j] = random_element;
                vec2[j] = random_element;
            }
            auto start = std::chrono::high_resolution_clock::now();
            mystl::sort(vec1, vec1 + data_len);
            auto end = std::chrono::high_resolution_clock::now();
            time1 += std::chrono::duration<double, std::milli>(end - start).count();
            start = std::chrono::high_resolution_clock::now();
            std::sort(vec2, vec2 + data_len);
            end = std::chrono::high_resolution_clock::now();
            time2 += std::chrono::duration<double, std::milli>(end - start).count();
        }
        std::cout << "sort efficiency test begin" << std::endl;
        std::cout << "data size: " << data_len << std::endl;
        std::cout << "test times: " << test_time << std::endl;
        std::cout << "mystl::sort average time spent: " << time1 / test_time << "ms" << std::endl;
        std::cout << "  std::sort average time spent: " << time2 / test_time << "ms" << std::endl;
        std::cout << "sort efficiency test accomplished" << std::endl;
        std::cout << std::endl;
    }

    // partial_sort
    constexpr bool partial_sort_correctness_test() {
        constexpr int test_time = 100;
        std::random_device rd;
        std::mt19937 gen(rd());
        int vec1[SMALL_SIZE];
        int vec2[SMALL_SIZE];
        std::uniform_int_distribution<int> dist(0, SMALL_SIZE * 5);
        std::uniform_int_distribution<int> range(0, SMALL_SIZE - 1);
        for (int i = 0; i < test_time; ++i) {
            for (int j = 0; j < SMALL_SIZE; ++j) {
                int random_element = dist(gen);
                vec1[j] = random_element;
                vec2[j] = random_element;
            }
            int partial_len = range(gen);
            mystl::partial_sort(vec1, vec1 + partial_len, vec1 + SMALL_SIZE);
            std::ranges::partial_sort(vec2, vec2 + partial_len);
            for (int j = 0; j < partial_len; ++j) {
                if (vec1[j] != vec2[j]) return false;
            }
        }
        return true;
    }

    // partial_sort_copy
    constexpr bool partial_sort_copy_correctness_test() {
        constexpr int test_time = 100;
        std::random_device rd;
        std::mt19937 gen(rd());
        int vec1[SMALL_SIZE], dest1[SMALL_SIZE];
        int vec2[SMALL_SIZE], dest2[SMALL_SIZE];
        std::uniform_int_distribution<int> dist(0, SMALL_SIZE * 5);
        std::uniform_int_distribution<int> range(0, SMALL_SIZE - 1);
        for (int i = 0; i < test_time; ++i) {
            for (int j = 0; j < SMALL_SIZE; ++j) {
                int random_element = dist(gen);
                vec1[j] = random_element;
                vec2[j] = random_element;
            }
            int partial_len = range(gen);
            mystl::partial_sort_copy(vec1, vec1 + SMALL_SIZE, dest1, dest1 + partial_len);
            std::partial_sort_copy(vec2, vec2 + SMALL_SIZE, dest2, dest2 + partial_len);
            for (int j = 0; j < partial_len; ++j) {
                if (dest1[j] != dest2[j]) return false;
            }
        }
        return true;
    }

    // stable_sort
    constexpr bool stable_sort_correctness_test() {
        constexpr int test_time = 100;
        std::random_device rd;
        std::mt19937 gen(rd());
        mystl::pair<int, int> vec1[SMALL_SIZE];
        std::pair<int, int> vec2[SMALL_SIZE];
        std::uniform_int_distribution<int> dist(0, SMALL_SIZE * 5);
        auto comp1 = [](const mystl::pair<int, int>& a, const mystl::pair<int, int>& b) {
            return a.first < b.first;
        };
        auto comp2 = [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
            return a.first < b.first;
        };
        for (int i = 0; i < test_time; ++i) {
            for (int j = 0; j < SMALL_SIZE; ++j) {
                int random_element1 = dist(gen);
                int random_element2 = dist(gen);
                vec1[j] = mystl::make_pair(random_element1, random_element2);
                vec2[j] = std::make_pair(random_element1, random_element2);
            }
            mystl::stable_sort(vec1, vec1 + SMALL_SIZE, comp1);
            std::stable_sort(vec2, vec2 + SMALL_SIZE, comp2);
            for (int j = 0; j < SMALL_SIZE; ++j) {
                if (vec1[j].first != vec2[j].first || vec1[j].second != vec2[j].second) return false;
            }
        }
        return true;
    }

    // nth_element
    constexpr bool nth_element_correctness_test() {
        constexpr int test_time = 100;
        std::random_device rd;
        std::mt19937 gen(rd());
        int vec[SMALL_SIZE];
        std::uniform_int_distribution<int> dist(0, SMALL_SIZE * 5);
        std::uniform_int_distribution<int> range(0, SMALL_SIZE - 1);
        auto comp = mystl::less<>{};
        for (int i = 0; i < test_time; ++i) {
            for (int j = 0; j < SMALL_SIZE; ++j) {
                vec[j] = dist(gen);
            }
            int nth_pos = range(gen);
            mystl::nth_element(vec, vec + nth_pos, vec + SMALL_SIZE, comp);
            for (int j = 0; j < SMALL_SIZE; ++j) {
                if (j < nth_pos && comp(*(vec + nth_pos), *(vec + j))) return false;
                if (j > nth_pos && comp(*(vec + j), *(vec + nth_pos))) return false;
            }
        }
        return true;
    }

    // **********************************************************************************
    // lower_bound
    constexpr bool lower_bound_correctness_test() {
        constexpr int test_time = 10;
        std::random_device rd;
        std::mt19937 gen(rd());
        int vec[LARGE_SIZE];
        int target[SMALL_SIZE];
        std::uniform_int_distribution<int> dist(0, SMALL_SIZE * 10);
        std::uniform_int_distribution<int> range(-SMALL_SIZE, SMALL_SIZE * 11);
        for (int i = 0; i < test_time; ++i) {
            for (int& x: vec) x = dist(gen);
            std::sort(vec, vec + LARGE_SIZE);
            for (int& x: target) x = range(gen);
            for (const int x: target) {
                auto it1 = mystl::lower_bound(vec, vec + LARGE_SIZE, x);
                auto it2 = std::lower_bound(vec, vec + LARGE_SIZE, x);
                if (it1 != it2) return false;
            }
        }
        return true;
    }

    // upper_bound
    constexpr bool upper_bound_correctness_test() {
        constexpr int test_time = 10;
        std::random_device rd;
        std::mt19937 gen(rd());
        int vec[LARGE_SIZE];
        int target[SMALL_SIZE];
        std::uniform_int_distribution<int> dist(0, SMALL_SIZE * 10);
        std::uniform_int_distribution<int> range(-SMALL_SIZE, SMALL_SIZE * 11);
        for (int i = 0; i < test_time; ++i) {
            for (int& x: vec) x = dist(gen);
            std::sort(vec, vec + LARGE_SIZE);
            for (int& x: target) x = range(gen);
            for (const int x: target) {
                auto it1 = mystl::upper_bound(vec, vec + LARGE_SIZE, x);
                auto it2 = std::upper_bound(vec, vec + LARGE_SIZE, x);
                if (it1 != it2 ) return false;
            }
        }
        return true;
    }

    // binary_search
    constexpr bool binary_search_correctness_test() {
        constexpr int test_time = 10;
        std::random_device rd;
        std::mt19937 gen(rd());
        int vec[LARGE_SIZE];
        int target[SMALL_SIZE];
        std::uniform_int_distribution<int> dist(0, SMALL_SIZE * 10);
        std::uniform_int_distribution<int> range(0, SMALL_SIZE * 10);
        for (int i = 0; i < test_time; ++i) {
            for (int& x: vec) x = dist(gen);
            std::sort(vec, vec + LARGE_SIZE);
            for (int& x: target) x = range(gen);
            for (const int x: target) {
                bool b1 = mystl::binary_search(vec, vec + LARGE_SIZE, x);
                bool b2 = std::binary_search(vec, vec + LARGE_SIZE, x);
                if (b1 != b2) return false;
            }
        }
        return true;
    }

    // equal_range
    constexpr bool equal_range_correctness_test() {
        constexpr int test_time = 10;
        std::random_device rd;
        std::mt19937 gen(rd());
        int vec[LARGE_SIZE];
        int target[SMALL_SIZE];
        std::uniform_int_distribution<int> dist(0, SMALL_SIZE * 10);
        std::uniform_int_distribution<int> range(0, SMALL_SIZE * 10);
        for (int i = 0; i < test_time; ++i) {
            for (int& x: vec) x = dist(gen);
            std::sort(vec, vec + LARGE_SIZE);
            for (int& x: target) x = range(gen);
            for (const int x: target) {
                auto p1 = mystl::equal_range(vec, vec + LARGE_SIZE, x);
                auto p2 = std::equal_range(vec, vec + LARGE_SIZE, x);
                if (p1.first != p2.first || p1.second != p2.second) return false;
            }
        }
        return true;
    }



} // namespace mystl_test

constexpr void sort_algorithm_correctness_test() {
    std::cout << "*****************************************************************" << std::endl;
    std::cout << " sort algorithm correctness test start" << std::endl;
    std::cout << "*****************************************************************" << std::endl;

    std::cout << "is_sorted correctness test: " << std::boolalpha <<
                            mystl_test::is_sorted_correctness_test() << std::endl;
    std::cout << "is_sorted_until correctness test: " << std::boolalpha <<
                            mystl_test::is_sorted_until_correctness_test() << std::endl;
    std::cout << "sort correctness test: " << std::boolalpha <<
                            mystl_test::sort_correctness_test() << std::endl;
    std::cout << "partial_sort correctness test: " << std::boolalpha <<
                            mystl_test::partial_sort_correctness_test() << std::endl;
    std::cout << "partial_sort_copy correctness test: " << std::boolalpha <<
                            mystl_test::partial_sort_copy_correctness_test() << std::endl;
    std::cout << "stable_sort correctness test: " << std::boolalpha <<
                            mystl_test::stable_sort_correctness_test() << std::endl;
    std::cout << "nth_element correctness test: " << std::boolalpha <<
                            mystl_test::nth_element_correctness_test() << std::endl;

    std::cout << "*****************************************************************" << std::endl;
    std::cout << " sort algorithm correctness test accomplished" << std::endl;
    std::cout << "*****************************************************************" << std::endl;
    std::cout << std::endl;
}

constexpr void sort_algorithm_efficiency_test() {
    std::cout << "*****************************************************************" << std::endl;
    std::cout << " sort algorithm efficiency test start" << std::endl;
    std::cout << "*****************************************************************" << std::endl;

    mystl_test::is_sorted_efficiency_test();
    mystl_test::sort_efficiency_test();

    std::cout << "*****************************************************************" << std::endl;
    std::cout << " sort algorithm efficiency test accomplished" << std::endl;
    std::cout << "*****************************************************************" << std::endl;
    std::cout << std::endl;
}

constexpr void binary_search_algorithm_correctness_test() {
    std::cout << "*****************************************************************" << std::endl;
    std::cout << " binary search algorithm correctness test start" << std::endl;
    std::cout << "*****************************************************************" << std::endl;

    std::cout << "lower_bound correctness test: " << std::boolalpha <<
                            mystl_test::lower_bound_correctness_test() << std::endl;
    std::cout << "upper_bound correctness test: " << std::boolalpha <<
                            mystl_test::upper_bound_correctness_test() << std::endl;
    std::cout << "binary_search correctness test: " << std::boolalpha <<
                            mystl_test::binary_search_correctness_test() << std::endl;
    std::cout << "equal_range correctness test: " << std::boolalpha <<
                            mystl_test::equal_range_correctness_test() << std::endl;

    std::cout << "*****************************************************************" << std::endl;
    std::cout << " binary search algorithm correctness test accomplished" << std::endl;
    std::cout << "*****************************************************************" << std::endl;
}

constexpr void binary_search_algorithm_efficiency_test() {
    std::cout << "*****************************************************************" << std::endl;
    std::cout << " binary search algorithm efficiency test start" << std::endl;
    std::cout << "*****************************************************************" << std::endl;



    std::cout << "*****************************************************************" << std::endl;
    std::cout << " binary search algorithm efficiency test accomplished" << std::endl;
    std::cout << "*****************************************************************" << std::endl;
    std::cout << std::endl;
}

constexpr void algorithm_correctness_test() {
    sort_algorithm_correctness_test();
    binary_search_algorithm_correctness_test();
}

constexpr void algorithm_efficiency_test() {
    sort_algorithm_efficiency_test();
    //binary_search_algorithm_efficiency_test();
}
#endif //ALGORITHM_TEST_H