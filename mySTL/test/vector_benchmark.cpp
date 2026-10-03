#include "../src/vector.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;
enum class Operation { append, reserved_append, range_construct, copy_construct,
                       assign_reuse, insert_middle, erase_middle, resize_grow };
struct Case { const char* name; Operation operation; };
constexpr Case cases[] = {
    {"push_back/grow", Operation::append},
    {"push_back/reserved", Operation::reserved_append},
    {"range_construct", Operation::range_construct},
    {"copy_construct", Operation::copy_construct},
    {"assign/reuse", Operation::assign_reuse},
    {"insert/middle_batch", Operation::insert_middle},
    {"erase/middle_range", Operation::erase_middle},
    {"resize/grow", Operation::resize_grow},
};

// Make the writes observable to the compiler before stopping the clock.
// Hashing and destruction are deliberately outside the measured interval.
void keep_memory(const void* p) {
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : "g"(p) : "memory");
#else
    (void)p;
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

template<class Function>
double timed(Function f) {
    std::atomic_signal_fence(std::memory_order_seq_cst);
    const auto begin = Clock::now();
    f();
    std::atomic_signal_fence(std::memory_order_seq_cst);
    const auto end = Clock::now();
    return std::chrono::duration<double, std::micro>(end - begin).count();
}

std::uint64_t hash_value(int value) { return static_cast<std::uint32_t>(value); }
std::uint64_t hash_value(const std::string& value) {
    std::uint64_t hash = 1469598103934665603ull;
    for (unsigned char c : value) hash = (hash ^ c) * 1099511628211ull;
    return hash;
}
template<class V>
std::uint64_t checksum(const V& values) {
    std::uint64_t hash = values.size();
    for (const auto& value : values) hash = (hash ^ hash_value(value)) * 1099511628211ull;
    return hash;
}
struct Sample { double us; std::uint64_t hash; std::size_t size, capacity; };

template<class V, class T>
Sample run(Operation operation, const std::vector<T>& input) {
    const std::size_t n = input.size();
    std::optional<V> result;
    double us = 0;
    switch (operation) {
        case Operation::append:
        case Operation::reserved_append:
            result.emplace();
            if (operation == Operation::reserved_append) result->reserve(n);
            us = timed([&] {
                for (const auto& value : input) result->push_back(value);
                keep_memory(result->data());
            });
            break;
        case Operation::range_construct:
            us = timed([&] {
                result.emplace(input.data(), input.data() + n);
                keep_memory(result->data());
            });
            break;
        case Operation::copy_construct: {
            V source(input.data(), input.data() + n);
            us = timed([&] {
                result.emplace(source);
                keep_memory(result->data());
            });
            break;
        }
        case Operation::assign_reuse:
            result.emplace(input.data(), input.data() + n / 2);
            result->reserve(n);
            us = timed([&] {
                result->assign(input.data(), input.data() + n);
                keep_memory(result->data());
            });
            break;
        case Operation::insert_middle: {
            const auto count = std::max<std::size_t>(1, n / 16);
            result.emplace(input.data(), input.data() + n);
            result->reserve(n + count);
            us = timed([&] {
                result->insert(result->begin() + n / 2, count, input[0]);
                keep_memory(result->data());
            });
            break;
        }
        case Operation::erase_middle:
            result.emplace(input.data(), input.data() + n);
            us = timed([&] {
                result->erase(result->begin() + n / 4, result->begin() + n / 2);
                keep_memory(result->data());
            });
            break;
        case Operation::resize_grow:
            result.emplace(input.data(), input.data() + n / 2);
            us = timed([&] {
                result->resize(n);
                keep_memory(result->data());
            });
            break;
    }
    return {us, checksum(*result), result->size(), result->capacity()};
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2;
}

template<class T>
void compare(const char* type, const std::vector<T>& input, unsigned repeats, std::ostream* csv) {
    for (const auto& test : cases) {
        std::vector<double> mine, standard;
        std::size_t my_capacity = 0, std_capacity = 0;
        // Two paired warmups, then alternate which implementation runs first.
        for (unsigned trial = 0; trial < repeats + 2; ++trial) {
            Sample a{}, b{};
            if (trial % 2 == 0) {
                a = run<mystl::vector<T>>(test.operation, input);
                b = run<std::vector<T>>(test.operation, input);
            } else {
                b = run<std::vector<T>>(test.operation, input);
                a = run<mystl::vector<T>>(test.operation, input);
            }
            if (a.hash != b.hash || a.size != b.size) {
                throw std::runtime_error(std::string("result mismatch: ") + type + "/" + test.name);
            }
            if (trial < 2) continue;
            mine.push_back(a.us);
            standard.push_back(b.us);
            my_capacity = a.capacity; std_capacity = b.capacity;
            if (csv) {
                *csv << type << ',' << test.name << ',' << input.size() << ',' << trial - 1
                     << ',' << std::setprecision(9) << a.us << ',' << b.us << ','
                     << a.capacity << ',' << b.capacity << ',' << a.hash << '\n';
            }
        }
        const double a = median(mine), b = median(standard);
        std::cout << std::left << std::setw(9) << type << std::setw(23) << test.name
                  << std::right << std::fixed << std::setprecision(3)
                  << std::setw(14) << a << std::setw(14) << b
                  << std::setw(12) << (b > 0 ? a / b : 0)
                  << std::setw(13) << my_capacity << std::setw(13) << std_capacity << '\n';
    }
}

std::size_t number(const std::string& text, const char* option, std::size_t low, std::size_t high) {
    std::size_t consumed = 0;
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error(std::string("invalid ") + option);
    auto n = std::stoull(text, &consumed);
    if (consumed != text.size() || n < low || n > high)
        throw std::runtime_error(std::string(option) + " out of supported range");
    return static_cast<std::size_t>(n);
}

int main(int argc, char** argv) {
    try {
        std::size_t size = 200000;
        unsigned repeats = 9;
        std::string csv_path;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help") {
                std::cout << "Usage: mystl_vector_benchmark [--size 16..1000000] [--repeats 3..99] [--csv FILE]\n"
                             "Defaults: size=200000, repeats=9. Use a Release build without sanitizers.\n";
                return 0;
            }
            if (i + 1 == argc) throw std::runtime_error("missing option value");
            if (option == "--size") size = number(argv[++i], "size", 16, 1000000);
            else if (option == "--repeats") repeats = static_cast<unsigned>(number(argv[++i], "repeats", 3, 99));
            else if (option == "--csv") csv_path = argv[++i];
            else throw std::runtime_error("unknown option: " + option);
        }
        std::ofstream csv;
        if (!csv_path.empty()) {
            csv.open(csv_path);
            if (!csv) throw std::runtime_error("cannot open CSV: " + csv_path);
            csv << "type,case,n,trial,mystl_us,std_us,mystl_capacity,std_capacity,checksum\n";
        }
        std::cout << "n=" << size << ", repeats=" << repeats << ", warmup=2, clock=steady_clock\n";
#if defined(__clang__)
        std::cout << "compiler=Clang " << __clang_version__ << '\n';
#elif defined(__GNUC__)
        std::cout << "compiler=GCC " << __VERSION__ << '\n';
#endif
#if defined(_LIBCPP_VERSION)
        std::cout << "stdlib=libc++ " << _LIBCPP_VERSION << '\n';
#elif defined(__GLIBCXX__)
        std::cout << "stdlib=libstdc++ " << __GLIBCXX__ << '\n';
#endif
#ifdef NDEBUG
        std::cout << "build=Release/NDEBUG\n";
#else
        std::cout << "build=Debug (not suitable for performance conclusions)\n";
#endif
        std::cout << "Median microseconds; ratio=mystl/std (>1 means mystl takes longer).\n"
                     "Setup/reserve, checksums and final destruction excluded; growth allocations included.\n"
                  << std::left << std::setw(9) << "type" << std::setw(23) << "case"
                  << std::right << std::setw(14) << "mystl_us" << std::setw(14) << "std_us"
                  << std::setw(12) << "ratio" << std::setw(13) << "mystl_cap" << std::setw(13) << "std_cap" << '\n';
        std::vector<int> integers(size);
        for (std::size_t i = 0; i < size; ++i) integers[i] = static_cast<int>((i * 48271u) % 1000003u);
        compare("int", integers, repeats, csv.is_open() ? &csv : nullptr);
        std::vector<std::string> strings;
        strings.reserve(size);
        for (std::size_t i = 0; i < size; ++i) {
            auto value = std::to_string(i);
            value.append(80 - value.size(), static_cast<char>('a' + i % 26));
            strings.push_back(std::move(value));
        }
        compare("string80", strings, repeats, csv.is_open() ? &csv : nullptr);
        if (csv.is_open()) {
            csv.flush();
            if (!csv) throw std::runtime_error("failed writing CSV");
        }
        std::cout << "All paired result checksums matched.\n";
    } catch (const std::exception& error) {
        std::cerr << "benchmark error: " << error.what() << '\n';
        return 1;
    }
}
