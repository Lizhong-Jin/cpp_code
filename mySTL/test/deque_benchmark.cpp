// From the repository root:
// cmake -S mySTL -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DMYSTL_BUILD_BENCHMARKS=ON
// cmake --build build-release --target mystl_deque_benchmark
// ./build-release/mystl_deque_benchmark --size 100000 --repeats 7 --csv deque-benchmark.csv
// See --help for per-type and per-case filters. Benchmarks are not CTest tests.
#include "../src/deque.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
constexpr unsigned warmups = 2;
constexpr unsigned read_passes = 4;
constexpr std::size_t middle_limit = 8192;
constexpr std::size_t middle_cycles = 128;
constexpr unsigned seed = 20261007;

// Each mystl block contains one of these; compare the real default block
// policies rather than forcing std::deque to use the same block size.
struct LargeValue {
    std::array<std::uint64_t, 64> words{};
    explicit LargeValue(std::uint64_t value = 0) {
        for (std::size_t i = 0; i < words.size(); ++i) words[i] = value + i;
    }
    bool operator==(const LargeValue&) const = default;
};
static_assert(sizeof(LargeValue) == 512);

enum class Operation {
    range_construct, copy_construct, push_back, push_front, push_both,
    pop_back, pop_front, queue_cycle, iterate, random_read, middle_cycle,
    insert_fill, insert_range, erase_range, assign_reuse, resize_grow, resize_shrink
};
struct Case { const char* name; Operation operation; };
constexpr Case cases[] = {
    {"construct/range", Operation::range_construct},
    {"construct/copy", Operation::copy_construct},
    {"push_back", Operation::push_back},
    {"push_front", Operation::push_front},
    {"push/both_ends", Operation::push_both},
    {"pop_back", Operation::pop_back},
    {"pop_front", Operation::pop_front},
    {"queue/pop_push", Operation::queue_cycle},
    {"read/iterate", Operation::iterate},
    {"read/random", Operation::random_read},
    {"middle/insert_erase", Operation::middle_cycle},
    {"insert/fill_batch", Operation::insert_fill},
    {"insert/range_batch", Operation::insert_range},
    {"erase/middle_range", Operation::erase_range},
    {"assign/same_size", Operation::assign_reuse},
    {"resize/grow", Operation::resize_grow},
    {"resize/shrink", Operation::resize_shrink},
};

void keep_memory(const void* p) {
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : "g"(p) : "memory");
#else
    (void)p;
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

template<class Function>
double timed(Function&& f) {
    std::atomic_signal_fence(std::memory_order_seq_cst);
    const auto begin = Clock::now();
    f();
    std::atomic_signal_fence(std::memory_order_seq_cst);
    const auto end = Clock::now();
    return std::chrono::duration<double, std::micro>(end - begin).count();
}

std::uint64_t key(int value) { return static_cast<std::uint32_t>(value); }
std::uint64_t key(const LargeValue& value) { return value.words[0]; }
std::uint64_t key(const std::string& value) {
    if (value.empty()) return 0;
    return value.size() * 65536ull + static_cast<unsigned char>(value.front()) * 256ull
         + static_cast<unsigned char>(value.back());
}
void observe(std::uint64_t& hash, std::uint64_t value) {
    hash = (hash ^ value) * 1099511628211ull;
}

template<class D>
struct Sample {
    std::optional<D> values;
    double us = 0;
    std::uint64_t observed = 1469598103934665603ull;
};

template<class D, class T>
Sample<D> run(Operation operation, const std::vector<T>& input, std::size_t n,
              const std::vector<std::size_t>& indices) {
    Sample<D> result;
    // The destination and source for copy construction outlive the clock.
    if (operation == Operation::range_construct) {
        result.us = timed([&] {
            result.values.emplace(input.data(), input.data() + n);
            keep_memory(&*result.values);
        });
        return result;
    }
    if (operation == Operation::copy_construct) {
        D source(input.data(), input.data() + n);
        result.us = timed([&] {
            result.values.emplace(source);
            keep_memory(&*result.values);
        });
        return result;
    }
    if (operation == Operation::push_back || operation == Operation::push_front
        || operation == Operation::push_both) {
        result.values.emplace();
    } else if (operation == Operation::assign_reuse) {
        result.values.emplace(n, T{});
    } else {
        const auto initial = operation == Operation::resize_grow ? n / 2 : n;
        result.values.emplace(input.data(), input.data() + initial);
    }
    auto& d = *result.values;
    keep_memory(&d);
    result.us = timed([&] {
        switch (operation) {
            case Operation::push_back:
                for (std::size_t i = 0; i < n; ++i) d.push_back(input[i]);
                break;
            case Operation::push_front:
                for (std::size_t i = 0; i < n; ++i) d.push_front(input[i]);
                break;
            case Operation::push_both:
                for (std::size_t i = 0; i < n; ++i) {
                    if (i % 2) d.push_front(input[i]);
                    else d.push_back(input[i]);
                }
                break;
            case Operation::pop_back:
                while (!d.empty()) { observe(result.observed, key(d.back())); d.pop_back(); }
                break;
            case Operation::pop_front:
                while (!d.empty()) { observe(result.observed, key(d.front())); d.pop_front(); }
                break;
            case Operation::queue_cycle:
                for (std::size_t i = 0; i < n; ++i) {
                    observe(result.observed, key(d.front()));
                    d.pop_front();
                    d.push_back(input[i]);
                }
                break;
            case Operation::iterate:
                for (unsigned pass = 0; pass < read_passes; ++pass) {
                    keep_memory(&d);
                    for (const auto& value : d) observe(result.observed, key(value));
                }
                break;
            case Operation::random_read:
                for (unsigned pass = 0; pass < read_passes; ++pass) {
                    keep_memory(&d);
                    for (auto i : indices) observe(result.observed, key(d[i]));
                }
                break;
            case Operation::middle_cycle:
                for (std::size_t i = 0; i < middle_cycles; ++i) {
                    auto it = d.insert(d.begin() + n / 2, input[i % n]);
                    observe(result.observed, key(*it));
                    observe(result.observed, static_cast<std::uint64_t>(it - d.begin()));
                    d.erase(it);
                }
                break;
            case Operation::insert_fill:
                d.insert(d.begin() + n / 2, n / 16, input[0]);
                break;
            case Operation::insert_range:
                d.insert(d.begin() + n / 2, input.data(), input.data() + n / 16);
                break;
            case Operation::erase_range:
                d.erase(d.begin() + n / 4, d.begin() + n / 2);
                break;
            case Operation::assign_reuse:
                d.assign(input.data(), input.data() + n);
                break;
            case Operation::resize_grow: d.resize(n); break;
            case Operation::resize_shrink: d.resize(n / 2); break;
            default: throw std::logic_error("unhandled benchmark case");
        }
        keep_memory(&d);
        keep_memory(&result.observed);
    });
    return result;
}

template<class A, class B>
void verify(const Sample<A>& a, const Sample<B>& b, const std::string& context) {
    if (a.observed != b.observed || a.values->size() != b.values->size())
        throw std::runtime_error("result mismatch: " + context);
    auto left = a.values->begin();
    auto right = b.values->begin();
    for (std::size_t i = 0; i < a.values->size(); ++i, ++left, ++right) {
        if (!(*left == *right))
            throw std::runtime_error("element mismatch: " + context + ", index=" + std::to_string(i));
    }
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto mid = values.size() / 2;
    return values.size() % 2 ? values[mid] : (values[mid - 1] + values[mid]) / 2;
}

struct Options {
    std::size_t size = 100000;
    unsigned repeats = 7;
    std::string type = "all", test = "all", csv_path;
};

template<class T>
void compare(const char* type, const std::vector<T>& input, const Options& options, std::ostream* csv) {
    for (const auto& test : cases) {
        if (options.test != "all" && options.test != test.name) continue;
        const auto n = test.operation == Operation::middle_cycle
                     ? std::min(input.size(), middle_limit) : input.size();
        std::vector<std::size_t> indices(n);
        std::iota(indices.begin(), indices.end(), 0);
        std::mt19937 generator(seed);
        std::shuffle(indices.begin(), indices.end(), generator);
        std::vector<double> mine, standard;
        for (unsigned trial = 0; trial < options.repeats + warmups; ++trial) {
            Sample<mystl::deque<T>> a;
            Sample<std::deque<T>> b;
            if (trial % 2 == 0) {
                a = run<mystl::deque<T>>(test.operation, input, n, indices);
                b = run<std::deque<T>>(test.operation, input, n, indices);
            } else {
                b = run<std::deque<T>>(test.operation, input, n, indices);
                a = run<mystl::deque<T>>(test.operation, input, n, indices);
            }
            verify(a, b, std::string(type) + "/" + test.name + ", trial=" + std::to_string(trial));
            if (trial < warmups) continue;
            mine.push_back(a.us);
            standard.push_back(b.us);
            if (csv) {
                *csv << type << ',' << test.name << ',' << n << ',' << trial - warmups + 1
                     << ',' << std::setprecision(12) << a.us << ',' << b.us
                     << ',' << a.values->size() << ',' << a.observed << '\n';
            }
        }
        const auto a = median(mine), b = median(standard);
        std::cout << std::left << std::setw(11) << type << std::setw(23) << test.name
                  << std::right << std::setw(9) << n << std::fixed << std::setprecision(3)
                  << std::setw(14) << a << std::setw(14) << b << std::setw(11) << (b > 0 ? a / b : 0)
                  << std::endl;
    }
}

std::size_t number(const std::string& text, const char* name, std::size_t low, std::size_t high) {
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error(std::string("invalid ") + name);
    const auto value = std::stoull(text);
    if (value < low || value > high) throw std::runtime_error(std::string(name) + " out of range");
    return static_cast<std::size_t>(value);
}
} // namespace

int main(int argc, char** argv) {
    try {
        Options options;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") {
                std::cout << "Usage: mystl_deque_benchmark [--size 16..1000000] [--repeats 3..99]\n"
                             "       [--type all|int|string80|large512] [--case NAME] [--csv FILE]\n"
                             "Defaults: size=100000, repeats=7, type=all, case=all; warmups=2.\n"
                             "Use a Release build without sanitizers. Cases:\n";
                for (const auto& test : cases) std::cout << "  " << test.name << '\n';
                return 0;
            }
            if (i + 1 == argc) throw std::runtime_error("missing value for " + arg);
            const std::string value = argv[++i];
            if (arg == "--size") options.size = number(value, "size", 16, 1000000);
            else if (arg == "--repeats") options.repeats = static_cast<unsigned>(number(value, "repeats", 3, 99));
            else if (arg == "--type") options.type = value;
            else if (arg == "--case") options.test = value;
            else if (arg == "--csv") options.csv_path = value;
            else throw std::runtime_error("unknown option: " + arg);
        }
        if (options.type != "all" && options.type != "int" && options.type != "string80" && options.type != "large512")
            throw std::runtime_error("unknown type: " + options.type);
        if (options.test != "all" && std::none_of(std::begin(cases), std::end(cases),
            [&](const Case& c) { return options.test == c.name; }))
            throw std::runtime_error("unknown case: " + options.test);
        std::ofstream csv;
        if (!options.csv_path.empty()) {
            csv.open(options.csv_path);
            if (!csv) throw std::runtime_error("cannot open CSV: " + options.csv_path);
            csv << "type,case,n,trial,mystl_us,std_us,result_size,observation\n";
        }
        std::cout << "n=" << options.size << ", repeats=" << options.repeats << ", warmup=" << warmups
                  << ", seed=" << seed << ", read_passes=" << read_passes
                  << ", middle_cycles=" << middle_cycles << ", middle_n_limit=" << middle_limit << '\n';
#if defined(__clang__)
        std::cout << "compiler=Clang " << __clang_version__ << '\n';
#elif defined(__GNUC__)
        std::cout << "compiler=GCC " << __VERSION__ << '\n';
#elif defined(_MSC_VER)
        std::cout << "compiler=MSVC " << _MSC_VER << '\n';
#endif
#if defined(_LIBCPP_VERSION)
        std::cout << "stdlib=libc++ " << _LIBCPP_VERSION << '\n';
#elif defined(__GLIBCXX__)
        std::cout << "stdlib=libstdc++ " << __GLIBCXX__ << '\n';
#elif defined(_MSVC_STL_VERSION)
        std::cout << "stdlib=MSVC-STL " << _MSVC_STL_VERSION << '\n';
#endif
#ifdef NDEBUG
        std::cout << "build=NDEBUG (use Release optimizations for timing)\n";
#else
        std::cout << "build=Debug: use results for validation, not performance conclusions\n";
#endif
        std::cout << "Median microseconds; ratio=mystl/std (>1 means mystl takes longer).\n"
                     "Setup, full verification and final destruction excluded; operation allocations included.\n"
                     "Read/pop/queue/middle loops include lightweight observation of values.\n"
                  << std::left << std::setw(11) << "type" << std::setw(23) << "case"
                  << std::right << std::setw(9) << "n" << std::setw(14) << "mystl_us"
                  << std::setw(14) << "std_us" << std::setw(11) << "ratio" << '\n';
        auto* output = csv.is_open() ? &csv : nullptr;
        if (options.type == "all" || options.type == "int") {
            std::vector<int> input(options.size);
            for (std::size_t i = 0; i < input.size(); ++i) input[i] = static_cast<int>((i * 48271ull) % 1000003ull);
            compare("int", input, options, output);
        }
        if (options.type == "all" || options.type == "string80") {
            std::vector<std::string> input;
            input.reserve(options.size);
            for (std::size_t i = 0; i < options.size; ++i) {
                auto value = std::to_string(i);
                value.append(80 - value.size(), static_cast<char>('a' + i % 26));
                input.push_back(std::move(value));
            }
            compare("string80", input, options, output);
        }
        if (options.type == "all" || options.type == "large512") {
            std::vector<LargeValue> input;
            input.reserve(options.size);
            for (std::size_t i = 0; i < options.size; ++i) input.emplace_back(i);
            compare("large512", input, options, output);
        }
        if (csv.is_open()) {
            csv.flush();
            if (!csv) throw std::runtime_error("failed writing CSV");
        }
        std::cout << "All paired element and observation checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << "benchmark error: " << error.what() << '\n';
        return 1;
    }
}
