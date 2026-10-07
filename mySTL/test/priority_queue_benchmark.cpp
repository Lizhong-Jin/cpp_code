// Build the mystl_priority_queue_benchmark target in Release; see --help.
#include "../src/priority_queue.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
constexpr unsigned seed = 20261007, warmups = 2;
constexpr std::uint64_t hash_seed = 1469598103934665603ull;

// A total order makes tie handling identical for both implementations.
struct Task {
    std::uint64_t priority, id;
    std::array<std::uint64_t, 6> payload;
    Task(std::uint64_t p = 0, std::uint64_t i = 0) : priority(p), id(i) {
        for (std::size_t j = 0; j < payload.size(); ++j) payload[j] = (p ^ i) + j;
    }
    bool operator<(const Task& rhs) const {
        return priority < rhs.priority || (priority == rhs.priority && id < rhs.id);
    }
    bool operator==(const Task&) const = default;
};
static_assert(sizeof(Task) == 64);

template<class T, bool MinHeap>
struct Compare {
    bool operator()(const T& a, const T& b) const {
        if constexpr (MinHeap) return b < a;
        else return a < b;
    }
};

enum class Operation { range, push, reserved_push, emplace, pop, mixed };
struct Case { const char* name; Operation op; };
constexpr Case cases[] = {
    {"construct/range", Operation::range},
    {"push/grow", Operation::push},
    {"push/reserved", Operation::reserved_push},
    {"emplace/grow", Operation::emplace},
    {"pop/all", Operation::pop},
    {"mixed/pop_push", Operation::mixed},
};
struct Options {
    std::size_t size = 100000;
    unsigned repeats = 7;
    std::string type = "all", test = "all", heap = "all", order = "random", csv;
};

void keep_memory(const void* p) {
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : "g"(p) : "memory");
#else
    (void)p;
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}
template<class F>
double timed(F&& f) {
    std::atomic_signal_fence(std::memory_order_seq_cst);
    const auto start = Clock::now();
    f();
    std::atomic_signal_fence(std::memory_order_seq_cst);
    return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}
void observe(std::uint64_t& h, std::uint64_t value) { h = (h ^ value) * 1099511628211ull; }
void observe(std::uint64_t& h, int value) { observe(h, static_cast<std::uint64_t>(value)); }
void observe(std::uint64_t& h, const Task& value) {
    observe(h, value.priority); observe(h, value.id);
    for (auto word : value.payload) observe(h, word);
}

template<class Q>
struct Sample {
    std::optional<Q> queue;
    double us = 0;
    std::uint64_t removed_hash = hash_seed;
    std::size_t removed_count = 0;
};

template<class Q, class T>
Sample<Q> run(Operation op, const std::vector<T>& input) {
    Sample<Q> result;
    const auto n = input.size();
    if (op == Operation::range) {
        result.us = timed([&] {
            result.queue.emplace(input.data(), input.data() + n, typename Q::value_compare{});
            keep_memory(&*result.queue);
        });
        return result;
    }
    if (op == Operation::reserved_push) {
        typename Q::container_type storage;
        storage.reserve(n);
        result.queue.emplace(typename Q::value_compare{}, std::move(storage));
    } else if (op == Operation::pop || op == Operation::mixed) {
        const auto initial = op == Operation::pop ? n : n / 2;
        result.queue.emplace(input.data(), input.data() + initial, typename Q::value_compare{});
    } else {
        result.queue.emplace();
    }
    auto& q = *result.queue;
    result.us = timed([&] {
        switch (op) {
        case Operation::push:
        case Operation::reserved_push:
            for (const auto& v : input) q.push(v);
            break;
        case Operation::emplace:
            for (const auto& v : input) {
                if constexpr (std::is_same_v<T, int>) q.emplace(v);
                else q.emplace(v.priority, v.id);
            }
            break;
        case Operation::pop:
            while (!q.empty()) {
                observe(result.removed_hash, q.top());
                q.pop(); ++result.removed_count;
            }
            break;
        case Operation::mixed:
            for (std::size_t i = n / 2; i < n; ++i) {
                observe(result.removed_hash, q.top());
                q.pop(); ++result.removed_count;
                q.push(input[i]);
            }
            break;
        case Operation::range: break;
        }
        keep_memory(&q);
        keep_memory(&result.removed_hash);
    });
    return result;
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto i = values.size() / 2;
    return values.size() % 2 ? values[i] : (values[i - 1] + values[i]) / 2;
}

template<class T, bool MinHeap>
void compare(const char* type, const std::vector<T>& input, const Options& opts, std::ostream* csv) {
    using C = Compare<T, MinHeap>;
    using Mine = mystl::priority_queue<T, mystl::vector<T>, C>;
    using Standard = std::priority_queue<T, std::vector<T>, C>;
    const char* heap = MinHeap ? "min" : "max";
    // Independent sorted oracle for all cases that retain every input element.
    auto sorted = input;
    std::sort(sorted.begin(), sorted.end(), [](const T& a, const T& b) { return C{}(b, a); });
    for (const auto& test : cases) {
        if (opts.test != "all" && opts.test != test.name) continue;
        std::vector<double> mine, standard;
        for (unsigned trial = 0; trial < opts.repeats + warmups; ++trial) {
            Sample<Mine> a;
            Sample<Standard> b;
            if (trial % 2 == 0) {
                a = run<Mine>(test.op, input); b = run<Standard>(test.op, input);
            } else {
                b = run<Standard>(test.op, input); a = run<Mine>(test.op, input);
            }
            auto fail = [&] { throw std::runtime_error(std::string("result mismatch: ") + type + "/" + heap + "/" + test.name); };
            const auto remaining = a.queue->size();
            const auto expected_size = test.op == Operation::pop ? 0 :
                test.op == Operation::mixed ? input.size() / 2 : input.size();
            const auto expected_removed = test.op == Operation::pop ? input.size() :
                test.op == Operation::mixed ? input.size() - input.size() / 2 : 0;
            if (remaining != expected_size || b.queue->size() != expected_size ||
                a.removed_count != expected_removed || b.removed_count != expected_removed ||
                a.removed_hash != b.removed_hash) fail();
            if (test.op == Operation::pop) {
                auto expected_hash = hash_seed;
                for (const auto& value : sorted) observe(expected_hash, value);
                if (a.removed_hash != expected_hash) fail();
            }
            // Drain after timing: compare complete values, not the non-unique heap array layout.
            for (std::size_t i = 0; !a.queue->empty(); ++i) {
                if (b.queue->empty() || !(a.queue->top() == b.queue->top())) fail();
                if (test.op != Operation::mixed && !(a.queue->top() == sorted[i])) fail();
                a.queue->pop(); b.queue->pop();
            }
            if (!b.queue->empty()) fail();
            if (trial < warmups) continue;
            mine.push_back(a.us); standard.push_back(b.us);
            if (csv) *csv << type << ',' << heap << ',' << opts.order << ',' << test.name << ','
                << input.size() << ',' << trial - warmups + 1 << ',' << std::setprecision(9)
                << a.us << ',' << b.us << ',' << remaining << ',' << a.removed_count << ',' << a.removed_hash << '\n';
        }
        const auto a = median(mine), b = median(standard);
        std::cout << std::left << std::setw(9) << type << std::setw(6) << heap << std::setw(21) << test.name
                  << std::right << std::fixed << std::setprecision(3) << std::setw(13) << a << std::setw(13) << b
                  << std::setw(11) << (b > 0 ? a / b : 0) << '\n';
    }
}
std::size_t number(const std::string& value, const char* option, std::size_t low, std::size_t high) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error(std::string("invalid ") + option);
    const auto n = std::stoull(value);
    if (n < low || n > high) throw std::runtime_error(std::string(option) + " out of supported range");
    return static_cast<std::size_t>(n);
}
void help() {
    std::cout << "Usage: mystl_priority_queue_benchmark [options]\n"
        "  --size 16..1000000    default 100000\n  --repeats 3..99       default 7\n"
        "  --type all|int|task64\n  --heap all|max|min\n"
        "  --order random|ascending|descending|duplicates (default random)\n"
        "  --case all";
    for (const auto& c : cases) std::cout << '|' << c.name;
    std::cout << "\n  --csv FILE\nUse Release without sanitizers for performance measurements.\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        Options o;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") { help(); return 0; }
            if (i + 1 == argc) throw std::runtime_error("missing value for " + arg);
            const std::string value = argv[++i];
            if (arg == "--size") o.size = number(value, "size", 16, 1000000);
            else if (arg == "--repeats") o.repeats = static_cast<unsigned>(number(value, "repeats", 3, 99));
            else if (arg == "--type") o.type = value;
            else if (arg == "--heap") o.heap = value;
            else if (arg == "--order") o.order = value;
            else if (arg == "--case") o.test = value;
            else if (arg == "--csv") { if (value.empty()) throw std::runtime_error("empty CSV path"); o.csv = value; }
            else throw std::runtime_error("unknown option: " + arg);
        }
        if (o.type != "all" && o.type != "int" && o.type != "task64") throw std::runtime_error("unknown type");
        if (o.heap != "all" && o.heap != "min" && o.heap != "max") throw std::runtime_error("unknown heap");
        if (o.order != "random" && o.order != "ascending" && o.order != "descending" && o.order != "duplicates")
            throw std::runtime_error("unknown order");
        if (o.test != "all" && std::none_of(std::begin(cases), std::end(cases), [&](const Case& c){return o.test == c.name;}))
            throw std::runtime_error("unknown case");
        std::ofstream csv;
        if (!o.csv.empty()) {
            csv.open(o.csv);
            if (!csv) throw std::runtime_error("cannot open CSV: " + o.csv);
            csv << "type,heap,order,case,n,trial,mystl_us,std_us,remaining,removed,removed_hash\n";
        }
        std::cout << "n=" << o.size << ", repeats=" << o.repeats << ", warmup=" << warmups
                  << ", seed=" << seed << ", order=" << o.order << ", clock=steady_clock\n";
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
        std::cout << "build=Debug (use only for correctness checks)\n";
#endif
        std::cout << "Median microseconds; ratio=mystl/std (>1 means mystl takes longer).\n"
            "Setup/reserve, validation and final destruction excluded; operation allocations included.\n"
            "pop/mixed include top reads and identical rolling hashes of removed values.\n"
            "mixed: preload floor(N/2), then pop+push for each remaining input.\n"
            << std::left << std::setw(9) << "type" << std::setw(6) << "heap" << std::setw(21) << "case"
            << std::right << std::setw(13) << "mystl_us" << std::setw(13) << "std_us" << std::setw(11) << "ratio" << '\n';
        std::mt19937 rng(seed);
        std::vector<int> integers(o.size);
        for (auto& v : integers) v = static_cast<int>(rng() % (o.order == "duplicates" ? 16u : 1000003u));
        if (o.order == "ascending") std::sort(integers.begin(), integers.end());
        if (o.order == "descending") std::sort(integers.begin(), integers.end(), std::greater<int>{});
        auto dispatch = [&]<class T>(const char* name, const std::vector<T>& values) {
            if (o.heap != "min") compare<T, false>(name, values, o, csv.is_open() ? &csv : nullptr);
            if (o.heap != "max") compare<T, true>(name, values, o, csv.is_open() ? &csv : nullptr);
        };
        if (o.type != "task64") dispatch("int", integers);
        if (o.type != "int") {
            std::vector<Task> tasks; tasks.reserve(o.size);
            for (std::size_t i = 0; i < o.size; ++i) tasks.emplace_back(integers[i], i);
            if (o.order == "ascending") std::sort(tasks.begin(), tasks.end());
            if (o.order == "descending") std::sort(tasks.begin(), tasks.end(), [](const Task& a,const Task& b){return b<a;});
            dispatch("task64", tasks);
        }
        if (csv.is_open()) { csv.flush(); if (!csv) throw std::runtime_error("failed writing CSV"); }
        std::cout << "All result sizes, removed traces and remaining values matched.\n";
    } catch (const std::exception& error) {
        std::cerr << "benchmark error: " << error.what() << '\n'; return 1;
    }
}
