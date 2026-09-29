#include "algorithm_test.h"

int main() {
    // Report failures through the exit code so CTest can detect regressions.
    struct Test { const char* name; bool (*run)(); };
    const Test tests[] = {
        {"is_sorted", mystl_test::is_sorted_correctness_test},
        {"is_sorted_until", mystl_test::is_sorted_until_correctness_test},
        {"sort", mystl_test::sort_correctness_test},
        {"partial_sort", mystl_test::partial_sort_correctness_test},
        {"partial_sort_copy", mystl_test::partial_sort_copy_correctness_test},
        {"stable_sort", mystl_test::stable_sort_correctness_test},
        {"nth_element", mystl_test::nth_element_correctness_test},
        {"lower_bound", mystl_test::lower_bound_correctness_test},
        {"upper_bound", mystl_test::upper_bound_correctness_test},
        {"binary_search", mystl_test::binary_search_correctness_test},
        {"equal_range", mystl_test::equal_range_correctness_test},
    };
    bool passed = true;
    for (const auto& test : tests) {
        const bool result = test.run();
        std::cout << test.name << ": " << (result ? "PASS" : "FAIL") << '\n';
        passed = result && passed;
    }
    return passed ? 0 : 1;
}
