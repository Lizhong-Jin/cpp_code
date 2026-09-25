#include "Thread_Pool_benchmark.hpp"
#include "Thread_Pool.hpp"
#include "Thread_Pool_baseline.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {
using Clock = std::chrono::steady_clock;
// Each implementation gets the first position in exactly half the measured rounds.
constexpr int measured_rounds = 6;
constexpr std::size_t tiny_tasks = 20000;
constexpr std::size_t cpu_tasks = 1000;
constexpr std::size_t latency_tasks = 1000;
constexpr std::size_t cpu_iterations = 20000;
constexpr std::size_t stress_tasks = 512;
constexpr std::size_t stress_iterations = 200000;
constexpr int stress_rounds = 3;
constexpr std::size_t concurrent_tasks = 100000;
constexpr std::size_t concurrent_iterations = 32;
constexpr std::size_t concurrent_producers[] = {4, 8, 16};
constexpr std::size_t producer_window = 256;
constexpr int concurrent_rounds = 3;
volatile std::uint64_t observed_checksum = 0;
static_assert(!std::is_same_v<ThreadPool, baseline::ThreadPool>,
              "Benchmarks must use distinct implementation types");

double milliseconds(Clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2;
}

// The runtime seed and consumed result keep this work observable in release builds.
std::uint64_t cpu_work(std::uint64_t value, std::size_t iterations = cpu_iterations) {
    for (std::size_t i = 0; i < iterations; ++i) {
        value ^= value << 13;
        value ^= value >> 7;
        value ^= value << 17;
    }
    return value;
}

void verify(std::uint64_t actual, std::uint64_t expected) {
    observed_checksum = actual;
    if (actual != expected) throw std::runtime_error("benchmark checksum mismatch");
}

struct BatchResult {
    double total_ms;
    double submit_ns;
};

struct LatencyResult {
    double p50_us;
    double p95_us;
    double p99_us;
};

template <class Result>
struct Comparison {
    Result baseline;
    Result current;
};

// Shared templated harness: no virtual dispatch in timed code. Only one
// implementation executes tasks at a time; the other pool remains idle.
template <class Pool>
class BatchSampler {
public:
    BatchSampler(std::size_t workers, std::size_t count) : pool_(workers, count) {
        tasks_.reserve(count);
        totals_.reserve(measured_rounds);
        submissions_.reserve(measured_rounds);
    }

    template <class Work>
    void sample(std::size_t count, Work work, std::uint64_t expected, bool warmup) {
        tasks_.clear();
        const auto start = Clock::now();
        for (std::size_t i = 0; i < count; ++i)
            tasks_.push_back(pool_.submit(work, i));
        const auto submitted = Clock::now();
        std::uint64_t checksum = 0;
        for (auto& task : tasks_) checksum += task.get();
        const auto end = Clock::now();
        verify(checksum, expected);
        if (!warmup) {
            totals_.push_back(milliseconds(end - start));
            submissions_.push_back(milliseconds(submitted - start) * 1e6 / count);
        }
    }

    BatchResult result() const { return {median(totals_), median(submissions_)}; }

private:
    Pool pool_;
    std::vector<std::future<std::uint64_t>> tasks_;
    std::vector<double> totals_, submissions_;
};

template <class Baseline, class Current>
void paired_rounds(Baseline run_baseline, Current run_current) {
    run_baseline(true);
    run_current(true);
    for (int round = 0; round < measured_rounds; ++round) {
        if (round % 2 == 0) {
            run_baseline(false);
            run_current(false);
        } else {
            run_current(false);
            run_baseline(false);
        }
    }
}

template <class Work>
Comparison<BatchResult> batch_benchmark(std::size_t workers, std::size_t count,
                                        Work work, std::uint64_t expected) {
    // Queue capacity accommodates the entire batch for both implementations.
    BatchSampler<baseline::ThreadPool> base(workers, count);
    BatchSampler<ThreadPool> current(workers, count);
    paired_rounds([&](bool warmup) { base.sample(count, work, expected, warmup); },
                  [&](bool warmup) { current.sample(count, work, expected, warmup); });
    return {base.result(), current.result()};
}

double serial_benchmark(std::uint64_t seed, std::uint64_t& expected) {
    std::vector<double> samples;
    for (int round = 0; round <= measured_rounds; ++round) {
        std::uint64_t checksum = 0;
        const auto start = Clock::now();
        for (std::size_t i = 0; i < cpu_tasks; ++i) checksum += cpu_work(seed + i);
        const auto end = Clock::now();
        if (round == 0) expected = checksum;
        verify(checksum, expected);
        if (round != 0) samples.push_back(milliseconds(end - start));
    }
    return median(samples);
}

template <class Pool>
class LatencySampler {
public:
    explicit LatencySampler(std::size_t workers) : pool_(workers, 1) {
        samples_.reserve(latency_tasks * measured_rounds);
    }

    void sample(bool warmup) {
        for (std::size_t i = 0; i < latency_tasks; ++i) {
            const auto start = Clock::now();
            auto task = pool_.submit([i] { return i; });
            const auto value = task.get();
            const auto end = Clock::now();
            verify(value, i);
            if (!warmup) samples_.push_back(milliseconds(end - start) * 1000.0);
        }
    }

    LatencyResult result() {
        std::sort(samples_.begin(), samples_.end());
        const auto percentile = [&](double fraction) {
            return samples_[static_cast<std::size_t>(std::ceil(fraction * samples_.size())) - 1];
        };
        return {percentile(0.50), percentile(0.95), percentile(0.99)};
    }

private:
    Pool pool_;
    std::vector<double> samples_;
};

Comparison<LatencyResult> latency_benchmark(std::size_t workers) {
    LatencySampler<baseline::ThreadPool> base(workers);
    LatencySampler<ThreadPool> current(workers);
    paired_rounds([&](bool warmup) { base.sample(warmup); },
                  [&](bool warmup) { current.sample(warmup); });
    return {base.result(), current.result()};
}

std::string ratio(double reference, double value) {
    if (reference <= 0 || value <= 0) return "n/a";
    std::ostringstream text;
    text << std::fixed << std::setprecision(3) << reference / value << 'x';
    return text.str();
}

void batch_header(bool cpu) {
    std::cout << std::right << std::setw(8) << "Workers" << std::setw(11) << "Impl"
              << std::setw(13) << "Batch(ms)" << std::setw(15) << "Tasks/s"
              << std::setw(16) << "Submit(ns/task)" << std::setw(14) << "Vs baseline";
    if (cpu) std::cout << std::setw(13) << "Vs serial";
    std::cout << '\n' << std::string(cpu ? 90 : 77, '-') << std::endl;
}

void print_batch(std::size_t workers, std::size_t count,
                 const Comparison<BatchResult>& comparison, double serial_ms = 0) {
    const auto row = [&](const char* name, const BatchResult& result) {
        std::cout << std::setw(8) << workers << std::setw(11) << name
                  << std::setw(13) << result.total_ms
                  << std::setw(15) << count * 1000.0 / result.total_ms
                  << std::setw(16) << result.submit_ns
                  << std::setw(14) << ratio(comparison.baseline.total_ms, result.total_ms);
        if (serial_ms > 0) std::cout << std::setw(13) << ratio(serial_ms, result.total_ms);
        std::cout << '\n';
    };
    row("baseline", comparison.baseline);
    row("current", comparison.current);
    std::cout << std::endl;
}

void print_latency(std::size_t workers, const Comparison<LatencyResult>& comparison) {
    const auto row = [&](const char* name, const LatencyResult& result) {
        std::cout << std::setw(8) << workers << std::setw(11) << name
                  << std::setw(12) << result.p50_us << std::setw(12) << result.p95_us
                  << std::setw(12) << result.p99_us
                  << std::setw(12) << ratio(comparison.baseline.p50_us, result.p50_us)
                  << std::setw(12) << ratio(comparison.baseline.p95_us, result.p95_us)
                  << std::setw(12) << ratio(comparison.baseline.p99_us, result.p99_us) << '\n';
    };
    row("baseline", comparison.baseline);
    row("current", comparison.current);
    std::cout << std::endl;
}

std::uint64_t stress_work(std::uint64_t seed) {
    return cpu_work(seed, stress_iterations);
}

// No worker reuse: every submit launches a new OS thread. A reused slot is
// joined before replacement, so at most `workers` threads can be alive.
class ThreadPerTask {
public:
    ThreadPerTask(std::size_t workers, std::size_t) : threads_(workers) {}
    ~ThreadPerTask() {
        for (auto& thread : threads_) if (thread.joinable()) thread.join();
    }
    ThreadPerTask(const ThreadPerTask&) = delete;
    ThreadPerTask& operator=(const ThreadPerTask&) = delete;

    template <class Work>
    std::future<std::uint64_t> submit(Work work, std::uint64_t seed) {
        auto& thread = threads_[next_];
        if (thread.joinable()) thread.join();
        std::packaged_task<std::uint64_t()> task([work, seed] { return work(seed); });
        auto future = task.get_future();
        thread = std::thread(std::move(task));
        next_ = (next_ + 1) % threads_.size();
        return future;
    }

private:
    std::vector<std::thread> threads_;
    std::size_t next_ = 0;
};

using StressValues = std::vector<std::uint64_t>;

void verify_stress(const StressValues& values, const StressValues& expected) {
    // Check every task, not just a sum that might hide missing/duplicate results.
    for (std::size_t i = 0; i < values.size(); ++i) verify(values[i], expected[i]);
}

double stress_serial(std::uint64_t seed, const StressValues& expected) {
    StressValues values(stress_tasks);
    const auto start = Clock::now();
    for (std::size_t i = 0; i < stress_tasks; ++i) values[i] = stress_work(seed + i);
    const auto end = Clock::now();
    verify_stress(values, expected);
    return milliseconds(end - start);
}

template <class Executor>
double stress_parallel(std::size_t workers, std::uint64_t seed, const StressValues& expected) {
    StressValues values(stress_tasks);
    const std::size_t window = std::min(workers, stress_tasks);
    const auto start = Clock::now();
    {
        Executor executor(workers, window);
        std::vector<std::future<std::uint64_t>> pending(window);
        // Identical rolling window for all executors, avoiding both unlimited
        // thread creation and a full-wave barrier. FIFO collection may wait on
        // a slower slot; this is a controlled concurrency comparison.
        for (std::size_t i = 0; i < stress_tasks; ++i) {
            auto& slot = pending[i % window];
            if (slot.valid()) values[i - window] = slot.get();
            slot = executor.submit(stress_work, seed + i);
        }
        for (std::size_t i = stress_tasks - window; i < stress_tasks; ++i)
            values[i] = pending[i % window].get();
    } // Include executor destruction, task cleanup and all thread joins.
    const auto end = Clock::now();
    verify_stress(values, expected);
    return milliseconds(end - start);
}

void stress_benchmark(const std::vector<std::size_t>& workers, std::uint64_t seed) {
    std::cout << "\n[4/5] Heavy CPU stress | " << stress_tasks << " tasks/run, "
              << stress_iterations << " iterations/task\n"
              << "Same rolling window = concurrency limit for all parallel implementations.\n"
              << "End-to-end: includes pool/thread creation, submission, results and all joins.\n"
              << "1 warmup + " << stress_rounds << " measured runs; median and min/max reported.\n"
              << "Parallel run order rotates; each implementation runs first once.\n"
              << "Every task result is checked against direct execution, outside timing.\n" << std::flush;

    StressValues expected(stress_tasks);
    for (std::size_t i = 0; i < stress_tasks; ++i) expected[i] = stress_work(seed + i);
    std::vector<double> serial_samples;
    stress_serial(seed, expected); // Warmup.
    for (int round = 0; round < stress_rounds; ++round)
        serial_samples.push_back(stress_serial(seed, expected));
    const double serial_ms = median(serial_samples);
    std::cout << "Serial (no threads): " << serial_ms << " ms, "
              << stress_tasks * 1000.0 / serial_ms << " tasks/s; min/max "
              << *std::min_element(serial_samples.begin(), serial_samples.end()) << " / "
              << *std::max_element(serial_samples.begin(), serial_samples.end()) << " ms.\n"
              << std::right << std::setw(7) << "Limit" << std::setw(17) << "Impl"
              << std::setw(12) << "Total(ms)" << std::setw(12) << "Min(ms)"
              << std::setw(12) << "Max(ms)" << std::setw(13) << "Tasks/s"
              << std::setw(12) << "Threads/run" << std::setw(12) << "Vs baseline"
              << std::setw(12) << "Vs trivial" << std::setw(12) << "Vs serial"
              << '\n' << std::string(121, '-') << std::endl;

    for (const auto count : workers) {
        using Run = double (*)(std::size_t, std::uint64_t, const StressValues&);
        const std::array<Run, 3> runs{{stress_parallel<baseline::ThreadPool>,
                                      stress_parallel<ThreadPool>, stress_parallel<ThreadPerTask>}};
        const std::array<const char*, 3> names{{"baseline", "current", "trivial"}};
        std::array<std::vector<double>, 3> samples;
        for (auto run : runs) run(count, seed, expected); // One warmup per implementation.
        for (int round = 0; round < stress_rounds; ++round) {
            for (std::size_t step = 0; step < runs.size(); ++step) {
                const auto index = (static_cast<std::size_t>(round) + step) % runs.size();
                samples[index].push_back(runs[index](count, seed, expected));
            }
        }
        const double baseline_ms = median(samples[0]);
        const double trivial_ms = median(samples[2]);
        for (std::size_t i = 0; i < runs.size(); ++i) {
            const double total_ms = median(samples[i]);
            std::cout << std::setw(7) << count << std::setw(17) << names[i]
                      << std::setw(12) << total_ms
                      << std::setw(12) << *std::min_element(samples[i].begin(), samples[i].end())
                      << std::setw(12) << *std::max_element(samples[i].begin(), samples[i].end())
                      << std::setw(13) << stress_tasks * 1000.0 / total_ms
                      << std::setw(12) << (i == 2 ? stress_tasks : count)
                      << std::setw(12) << ratio(baseline_ms, total_ms)
                      << std::setw(12) << ratio(trivial_ms, total_ms)
                      << std::setw(12) << ratio(serial_ms, total_ms) << '\n';
        }
        std::cout << std::endl;
    }
    std::cout << "trivial = a new std::thread per task, joined before its slot is reused.\n"
              << "Heavy work can dominate thread overhead; reuse need not give a large speedup.\n"
              << "This is a bounded heavy-load test, not an unlimited-thread or long-duration soak test.\n";
}

// Thread-safe no-pool control for concurrent producers. Slots bound live task
// threads, but never reuse a thread to execute another task. A completed slot
// is handed to one submitter; it joins the old thread outside the shared lock.
class ConcurrentThreadPerTask {
public:
    ConcurrentThreadPerTask(std::size_t workers, std::size_t) : threads_(workers) {
        available_.reserve(workers);
        for (std::size_t i = 0; i < workers; ++i) available_.push_back(i);
    }
    ~ConcurrentThreadPerTask() {
        // All producers are joined before the executor is destroyed.
        for (auto& thread : threads_) if (thread.joinable()) thread.join();
    }
    ConcurrentThreadPerTask(const ConcurrentThreadPerTask&) = delete;
    ConcurrentThreadPerTask& operator=(const ConcurrentThreadPerTask&) = delete;

    template <class Work>
    std::future<std::uint64_t> submit(Work work, std::uint64_t seed) {
        std::packaged_task<std::uint64_t()> task([work, seed] { return work(seed); });
        auto future = task.get_future();
        std::size_t slot;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            available_cv_.wait(lock, [&] { return !available_.empty(); });
            slot = available_.back();
            available_.pop_back();
        }
        try {
            if (threads_[slot].joinable()) threads_[slot].join();
            // A tiny task may finish before std::thread construction returns.
            // Do not let it publish its slot until the handle is installed.
            std::lock_guard<std::mutex> lock(mutex_);
            threads_[slot] = std::thread([this, slot, task = std::move(task)]() mutable {
                task(); // packaged_task captures user exceptions in the future.
                release(slot);
            });
        } catch (...) {
            release(slot);
            throw;
        }
        return future;
    }

private:
    void release(std::size_t slot) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            available_.push_back(slot); // Reserved at construction; cannot reallocate.
        }
        available_cv_.notify_one();
    }
    std::mutex mutex_;
    std::condition_variable available_cv_;
    std::vector<std::size_t> available_;
    std::vector<std::thread> threads_;
};

std::uint64_t concurrent_work(std::uint64_t seed) {
    return cpu_work(seed, concurrent_iterations);
}

template <class Executor>
double concurrent_run(std::size_t workers, std::size_t producer_count,
                      std::uint64_t seed, const StressValues& expected) {
    StressValues values(concurrent_tasks);
    const auto start = Clock::now();
    {
        // Even if all producers fill their windows, the pool cannot overflow.
        Executor executor(workers, producer_count * producer_window);
        std::promise<void> release;
        auto ready = release.get_future().share();
        std::vector<std::thread> producers;
        std::vector<std::exception_ptr> errors(producer_count);
        std::vector<std::size_t> completed(producer_count, 0);
        producers.reserve(producer_count);
        try {
            for (std::size_t p = 0; p < producer_count; ++p) {
                producers.emplace_back([&, p, ready] {
                    try {
                        ready.wait(); // No producer can submit before all are created.
                        const std::size_t begin = concurrent_tasks * p / producer_count;
                        const std::size_t end = concurrent_tasks * (p + 1) / producer_count;
                        const std::size_t window = std::min(producer_window, end - begin);
                        std::vector<std::future<std::uint64_t>> pending(window);
                        std::size_t local_completed = 0;
                        for (std::size_t i = begin; i < end; ++i) {
                            auto& slot = pending[(i - begin) % window];
                            if (slot.valid()) {
                                values[i - window] = slot.get();
                                ++local_completed;
                            }
                            slot = executor.submit(concurrent_work, seed + i);
                        }
                        for (std::size_t i = end - window; i < end; ++i) {
                            values[i] = pending[(i - begin) % window].get();
                            ++local_completed;
                        }
                        completed[p] = local_completed;
                    } catch (...) {
                        errors[p] = std::current_exception();
                    }
                });
            }
        } catch (...) {
            // Release and join already-created producers if thread creation fails.
            release.set_value();
            for (auto& producer : producers) producer.join();
            throw;
        }
        release.set_value();
        for (auto& producer : producers) producer.join();
        for (auto error : errors) if (error) std::rethrow_exception(error);
        std::size_t total = 0;
        for (auto count : completed) total += count;
        if (total != concurrent_tasks) throw std::runtime_error("concurrent task count mismatch");
    } // Include producer and executor lifetimes, including every task thread join.
    const auto end = Clock::now();
    verify_stress(values, expected); // Single-threaded validation avoids polluting the workload.
    return milliseconds(end - start);
}

void concurrent_benchmark(std::size_t workers, std::uint64_t seed) {
    std::cout << "\n[5/5] High-volume light tasks | " << concurrent_tasks << " tasks/run, "
              << concurrent_iterations << " iterations/task\n"
              << "Fixed task concurrency limit: " << workers << ".\n"
              << "Rolling window: " << producer_window << " tasks/producer; pool queue = producers * window.\n"
              << "Producers share one executor and start behind one gate; total task count stays fixed.\n"
              << "trivial creates a thread for each task; submit waits when all task-thread slots are busy.\n"
              << "End-to-end includes producer/executor creation, submissions, results and all joins.\n"
              << "1 warmup + " << concurrent_rounds << " measured runs, rotating implementation order.\n"
              << "Task threads excludes the common producer threads. Timings are medians; results checked per task.\n"
              << std::right << std::setw(10) << "Producers" << std::setw(12) << "Max flight"
              << std::setw(11) << "Impl" << std::setw(12) << "Total(ms)"
              << std::setw(12) << "Min(ms)" << std::setw(12) << "Max(ms)"
              << std::setw(15) << "Tasks/s" << std::setw(14) << "Task threads"
              << std::setw(13) << "Vs baseline" << std::setw(12) << "Vs trivial"
              << '\n' << std::string(123, '-') << std::endl;
    StressValues expected(concurrent_tasks);
    for (std::size_t i = 0; i < concurrent_tasks; ++i) expected[i] = concurrent_work(seed + i);
    for (const std::size_t producers : concurrent_producers) {
        using Run = double (*)(std::size_t, std::size_t, std::uint64_t, const StressValues&);
        const std::array<Run, 3> runs{{concurrent_run<baseline::ThreadPool>,
                                      concurrent_run<ThreadPool>, concurrent_run<ConcurrentThreadPerTask>}};
        const std::array<const char*, 3> names{{"baseline", "current", "trivial"}};
        std::array<std::vector<double>, 3> samples;
        for (auto run : runs) run(workers, producers, seed, expected);
        for (int round = 0; round < concurrent_rounds; ++round) {
            for (std::size_t step = 0; step < runs.size(); ++step) {
                const auto index = (static_cast<std::size_t>(round) + step) % runs.size();
                samples[index].push_back(runs[index](workers, producers, seed, expected));
            }
        }
        const double baseline_ms = median(samples[0]);
        const double trivial_ms = median(samples[2]);
        for (std::size_t i = 0; i < runs.size(); ++i) {
            const double total_ms = median(samples[i]);
            std::cout << std::setw(10) << producers << std::setw(12) << producers * producer_window
                      << std::setw(11) << names[i] << std::setw(12) << total_ms
                      << std::setw(12) << *std::min_element(samples[i].begin(), samples[i].end())
                      << std::setw(12) << *std::max_element(samples[i].begin(), samples[i].end())
                      << std::setw(15) << concurrent_tasks * 1000.0 / total_ms
                      << std::setw(14) << (i == 2 ? concurrent_tasks : workers)
                      << std::setw(13) << ratio(baseline_ms, total_ms)
                      << std::setw(12) << ratio(trivial_ms, total_ms) << '\n';
        }
        std::cout << std::endl;
    }
    std::cout << "Max flight is a configured cap, not a measured peak. This is bounded offered load;\n"
              << "it measures submission contention and scheduling, not queue-full rejection or open-loop latency.\n";
}
} // namespace

int run_thread_pool_benchmarks() {
    const auto flags = std::cout.flags();
    const auto precision = std::cout.precision();
    int status = 0;
    try {
        const unsigned int hardware = std::thread::hardware_concurrency();
        const std::size_t limit = std::min(8u, hardware == 0 ? 1u : hardware);
        std::vector<std::size_t> workers{1};
        for (std::size_t count = 2; count <= limit; count *= 2) workers.push_back(count);
        if (workers.back() != limit) workers.push_back(limit);
        std::cout << std::fixed << std::setprecision(3)
                  << "\n=== Performance comparison: current vs baseline ===\n"
                  << "Current  : Thread_Pool.hpp (ThreadPool)\n"
                  << "Baseline : Thread_Pool_baseline.hpp (baseline::ThreadPool)\n"
                  << "Hardware : " << hardware << " threads; tested workers:";
        for (auto count : workers) std::cout << ' ' << count;
        std::cout << "\nProtocol (sections 1-3): 1 warmup + " << measured_rounds
                  << " measured rounds per implementation; alternating order.\n"
                  << "           Same inputs, one producer; pool creation/shutdown excluded.\n"
                  << "Ratios   : reference time / row time; >1x is faster, <1x is slower.\n"
                  << "           Vs baseline uses batch time; latency ratios use matching percentiles.\n"
                  << "Timing   : informational; use an optimized build without sanitizers.\n";

        std::cout << "\n[1/5] Tiny-task throughput | " << tiny_tasks << " tasks/batch\n"
                  << "Batch and submission columns are medians of measured rounds.\n";
        batch_header(false);
        for (const auto count : workers) {
            print_batch(count, tiny_tasks,
                        batch_benchmark(count, tiny_tasks,
                            [](std::size_t i) { return static_cast<std::uint64_t>(i); },
                            tiny_tasks * (tiny_tasks - 1) / 2));
        }

        const auto seed = static_cast<std::uint64_t>(Clock::now().time_since_epoch().count());
        std::uint64_t expected = 0;
        const double serial_ms = serial_benchmark(seed, expected);
        std::cout << "\n[2/5] CPU-task throughput | " << cpu_tasks << " tasks/batch, "
                  << cpu_iterations << " iterations/task\n"
                  << "Serial reference: " << serial_ms << " ms (direct execution, no thread pool).\n";
        batch_header(true);
        for (const auto count : workers) {
            print_batch(count, cpu_tasks,
                        batch_benchmark(count, cpu_tasks,
                            [seed](std::size_t i) { return cpu_work(seed + i); }, expected), serial_ms);
        }

        std::cout << "\n[3/5] Submit-to-get latency | one outstanding task\n"
                  << latency_tasks << " warmup + " << latency_tasks * measured_rounds
                  << " measured requests per implementation; pooled percentiles.\n"
                  << std::setw(8) << "Workers" << std::setw(11) << "Impl"
                  << std::setw(12) << "P50(us)" << std::setw(12) << "P95(us)"
                  << std::setw(12) << "P99(us)" << std::setw(12) << "P50 vs base"
                  << std::setw(12) << "P95 vs base" << std::setw(12) << "P99 vs base"
                  << '\n' << std::string(91, '-') << std::endl;
        for (const auto count : workers) print_latency(count, latency_benchmark(count));

        stress_benchmark(workers, seed);
        concurrent_benchmark(limit, seed);

        std::cout << "Notes: Submit includes future storage and worker contention; latency includes\n"
                  << "submission, scheduling, execution and result wakeup. Small differences may be noise.\n"
                  << "[PERF PASS] All five benchmark groups completed; all result checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << "[PERF FAIL] " << error.what() << '\n';
        status = 1;
    } catch (...) {
        std::cerr << "[PERF FAIL] unknown exception\n";
        status = 1;
    }
    std::cout.flags(flags);
    std::cout.precision(precision);
    return status;
}
