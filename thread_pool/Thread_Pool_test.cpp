// Standalone C++20 tests; see README.md for build and sanitizer commands.
#include "Thread_Pool.hpp"
#include "Thread_Pool_benchmark.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>

namespace {
using namespace std::chrono_literals;

// Unlike assert(), checks remain active in release builds.
#define CHECK(expr) do { if (!(expr)) throw std::runtime_error( \
    std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #expr); } while (false)

template <class Exception, class Function>
void expect_throw(Function&& function) {
    bool caught = false;
    try { function(); }
    catch (const Exception&) { caught = true; }
    CHECK(caught);
}

template <class T>
decltype(auto) result(std::future<T>& future) {
    CHECK(future.wait_for(5s) == std::future_status::ready);
    return future.get();
}

// Gates establish ordering without depending on scheduler speed or sleeps.
class Gate {
public:
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return open_; });
    }
    void open() {
        { std::lock_guard<std::mutex> lock(mutex_); open_ = true; }
        cv_.notify_all();
    }
private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool open_ = false;
};

struct OpenOnExit {
    Gate& gate;
    ~OpenOnExit() { gate.open(); }
};

struct JoinOnExit {
    std::vector<std::thread>& threads;
    ~JoinOnExit() {
        for (auto& thread : threads) if (thread.joinable()) thread.join();
    }
};

// A broken implementation must fail instead of hanging forever in join().
class Watchdog {
public:
    explicit Watchdog(const char* name, std::chrono::seconds timeout = 15s)
        : thread_([this, name, timeout] {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this] { return done_; })) {
            std::cerr << "[TIMEOUT] " << name << " exceeded " << timeout.count() << " seconds\n";
            std::_Exit(2);
        }
    }) {}
    ~Watchdog() {
        { std::lock_guard<std::mutex> lock(mutex_); done_ = true; }
        cv_.notify_one();
        thread_.join();
    }
private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool done_ = false;
    std::thread thread_;
};

static_assert(!std::is_copy_constructible_v<ThreadPool>);
static_assert(!std::is_copy_assignable_v<ThreadPool>);
static_assert(!std::is_move_constructible_v<ThreadPool>);
static_assert(!std::is_move_assignable_v<ThreadPool>);
static_assert(std::is_nothrow_destructible_v<ThreadPool>);
static_assert(!std::is_copy_constructible_v<MoveOnlyTask>);
static_assert(!std::is_copy_assignable_v<MoveOnlyTask>);
static_assert(std::is_nothrow_move_constructible_v<MoveOnlyTask>);
static_assert(std::is_nothrow_move_assignable_v<MoveOnlyTask>);
static_assert(std::is_nothrow_destructible_v<MoveOnlyTask>);
static_assert(!std::is_constructible_v<MoveOnlyTask, int>);

// Track nontrivial object lifetimes: bytewise relocation would fail these checks.
struct TaskProbe {
    int* live;
    int* moves;
    int* calls;
    TaskProbe(int& live_count, int& move_count, int& call_count)
        : live(&live_count), moves(&move_count), calls(&call_count) { ++*live; }
    TaskProbe(const TaskProbe&) = delete;
    TaskProbe(TaskProbe&& other) noexcept
        : live(other.live), moves(other.moves), calls(other.calls) { ++*live; ++*moves; }
    ~TaskProbe() { --*live; }
    void operator()() { ++*calls; }
};

void move_only_task_empty_and_invocation() {
    MoveOnlyTask empty;
    CHECK(!empty);
    expect_throw<std::bad_function_call>([&] { empty(); });
    MoveOnlyTask null(nullptr);
    CHECK(!null);
    void (*pointer)() = nullptr;
    MoveOnlyTask null_function(pointer);
    CHECK(!null_function);
    MoveOnlyTask moved_empty(std::move(empty));
    CHECK(!moved_empty && !empty);

    int calls = 0;
    auto function = [&] { ++calls; };
    MoveOnlyTask referenced(std::ref(function));
    referenced();
    CHECK(calls == 1);
    MoveOnlyTask owned([value = std::make_unique<int>(42), &calls]() mutable {
        CHECK(*value == 42);
        value.reset();
        ++calls;
    });
    owned();
    CHECK(calls == 2);
    MoveOnlyTask throwing([] { throw std::logic_error("task exception"); });
    expect_throw<std::logic_error>([&] { throwing(); });
    CHECK(throwing);
    referenced = std::move(empty);
    CHECK(!referenced && !empty);
}

void move_only_task_inline_lifetime() {
    int live = 0, moves = 0, calls = 0;
    {
        MoveOnlyTask source{TaskProbe(live, moves, calls)};
        CHECK(live == 1);
        const int before = moves;
        MoveOnlyTask destination(std::move(source));
        CHECK(!source && destination);
        CHECK(live == 1 && moves == before + 1);
        expect_throw<std::bad_function_call>([&] { source(); });
        destination();
        MoveOnlyTask replacement{TaskProbe(live, moves, calls)};
        CHECK(live == 2);
        replacement = std::move(destination);
        CHECK(!destination && live == 1 && moves == before + 3);
        auto* alias = &replacement;
        replacement = std::move(*alias);
        CHECK(replacement && live == 1);
        replacement();
        CHECK(calls == 2);
        replacement.reset();
        CHECK(!replacement && live == 0);
        replacement.reset();
    }
    CHECK(live == 0);
}

template <class Probe>
void check_heap_task_lifetime() {
    int live = 0, moves = 0, calls = 0;
    {
        MoveOnlyTask source{Probe(live, moves, calls)};
        CHECK(live == 1);
        const int before = moves;
        MoveOnlyTask moved(std::move(source));
        CHECK(!source && moved && moves == before && live == 1);
        moved();
        // Also exercise overwriting an inline target with a heap target.
        MoveOnlyTask destination{TaskProbe(live, moves, calls)};
        CHECK(live == 2);
        const int before_assignment = moves;
        destination = std::move(moved);
        CHECK(!moved && live == 1 && moves == before_assignment);
        destination();
        CHECK(calls == 2);
        // The opposite transition must delete the heap target exactly once.
        MoveOnlyTask small{TaskProbe(live, moves, calls)};
        destination = std::move(small);
        CHECK(!small && live == 1);
    }
    CHECK(live == 0);
}

void move_only_task_large_and_aligned() {
    struct Large : TaskProbe {
        using TaskProbe::TaskProbe;
        std::byte padding[128]{};
    };
    struct alignas(64) Aligned : TaskProbe {
        using TaskProbe::TaskProbe;
        void operator()() {
            CHECK(reinterpret_cast<std::uintptr_t>(this) % alignof(Aligned) == 0);
            TaskProbe::operator()();
        }
    };
    check_heap_task_lifetime<Large>();
    check_heap_task_lifetime<Aligned>();
}

void move_only_task_throwing_construction() {
    struct ThrowingMove {
        int* live;
        bool* fail;
        ThrowingMove(int& count, bool& flag) : live(&count), fail(&flag) { ++*live; }
        ThrowingMove(const ThrowingMove& other) : live(other.live), fail(other.fail) { ++*live; }
        ThrowingMove(ThrowingMove&& other) : live(other.live), fail(other.fail) {
            if (*fail) throw std::runtime_error("move failed");
            ++*live;
        }
        ~ThrowingMove() { --*live; }
        void operator()() {}
    };
    int live = 0;
    bool fail = false;
    {
        ThrowingMove callable(live, fail);
        MoveOnlyTask source(callable);
        CHECK(live == 2);
        fail = true;
        // A potentially throwing move forces heap storage, even for small objects.
        MoveOnlyTask destination(std::move(source));
        CHECK(!source && destination && live == 2);
        destination();
        expect_throw<std::runtime_error>([&] { MoveOnlyTask rejected(std::move(callable)); });
        CHECK(live == 2);
    }
    CHECK(live == 0);
    struct ThrowOnCopy {
        ThrowOnCopy() = default;
        ThrowOnCopy(const ThrowOnCopy&) { throw std::logic_error("copy failed"); }
        void operator()() {}
    } callable;
    expect_throw<std::logic_error>([&] { MoveOnlyTask rejected(callable); });
}

void invalid_configuration() {
    expect_throw<std::invalid_argument>([] { ThreadPool pool(0); });
    expect_throw<std::invalid_argument>([] { ThreadPool pool(1, 0); });
    expect_throw<std::invalid_argument>([] { ThreadPool pool(0, 0); });
}

void empty_pool_and_repeated_shutdown() {
    ThreadPool pool(3);
    CHECK(pool.workerCount() == 3);
    CHECK(pool.pendingTaskCount() == 0);
    pool.shutdown();
    pool.shutdown();
    CHECK(pool.pendingTaskCount() == 0);
    expect_throw<std::runtime_error>([&] { pool.submit([] {}); });
}

int add(int a, int b) { return a + b; }

void values_and_callables() {
    ThreadPool pool(2);
    auto a = pool.submit(add, 19, 23);
    auto b = pool.submit([] { return std::string("hello"); });
    auto c = pool.submit([n = 0]() mutable { return ++n; });
    auto d = pool.submit([] { return std::make_unique<int>(7); });
    CHECK(result(a) == 42);
    CHECK(result(b) == "hello");
    CHECK(result(c) == 1);
    CHECK(*result(d) == 7);
}

void void_and_reference_results() {
    int value = 0;
    ThreadPool pool(1);
    auto a = pool.submit([&] { value = 42; });
    result(a);
    CHECK(value == 42);
    auto b = pool.submit([&]() -> int& { return value; });
    CHECK(&result(b) == &value);
}

void argument_copy_and_reference() {
    Gate gate;
    int value = 10;
    ThreadPool pool(1);
    OpenOnExit release{gate};
    auto blocker = pool.submit([&] { gate.wait(); });
    auto copied = pool.submit([](int n) { return n; }, value);
    auto referenced = pool.submit([](int& n) { n += 5; }, std::ref(value));
    value = 20;
    gate.open();
    result(blocker);
    CHECK(result(copied) == 10);
    result(referenced);
    CHECK(value == 25);
    auto constant = pool.submit([](const int& n) { return n; }, std::cref(value));
    CHECK(result(constant) == 25);
}

void move_only_arguments_and_functors() {
    struct RvalueOnly {
        std::unique_ptr<int> value;
        int operator()() && { return *value; }
    };
    ThreadPool pool(2);
    auto value = std::make_unique<int>(42);
    auto a = pool.submit([](std::unique_ptr<int> p) { return *p; }, std::move(value));
    CHECK(!value);
    auto b = pool.submit([p = std::make_unique<int>(7)] { return *p; });
    auto c = pool.submit(RvalueOnly{std::make_unique<int>(9)});
    CHECK(result(a) == 42);
    CHECK(result(b) == 7);
    CHECK(result(c) == 9);
}

void member_functions_and_reference_wrapper() {
    struct Counter {
        int value = 10;
        int increment(int n) { return value += n; }
        int read() const { return value; }
        int operator()() & { return ++value; }
    } counter;
    ThreadPool pool(1);
    auto a = pool.submit(&Counter::increment, &counter, 5);
    CHECK(result(a) == 15);
    auto b = pool.submit(&Counter::read, std::cref(counter));
    CHECK(result(b) == 15);
    auto c = pool.submit(std::ref(counter));
    CHECK(result(c) == 16);
    auto d = pool.submit(&Counter::value, &counter);
    CHECK(&result(d) == &counter.value);
}

void task_exceptions_do_not_kill_workers() {
    ThreadPool pool(1, 64);
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 40; ++i) {
        futures.push_back(pool.submit([i] {
            if (i % 2 == 0) throw std::logic_error("task failure");
            return i;
        }));
    }
    for (int i = 0; i < 40; ++i) {
        if (i % 2 == 0) {
            bool caught = false;
            try { result(futures[i]); }
            catch (const std::logic_error& error) {
                caught = std::string(error.what()) == "task failure";
            }
            CHECK(caught);
        } else CHECK(result(futures[i]) == i);
    }
    auto nonstandard = pool.submit([] { throw 123; });
    bool caught = false;
    try { result(nonstandard); } catch (int n) { caught = n == 123; }
    CHECK(caught);
    auto after = pool.submit([] { return 42; });
    CHECK(result(after) == 42);
}

void submission_copy_failure_leaves_pool_usable() {
    struct ThrowOnCopy {
        ThrowOnCopy() = default;
        ThrowOnCopy(const ThrowOnCopy&) { throw std::logic_error("copy failure"); }
        void operator()() const {}
    } callable;
    ThreadPool pool(1);
    expect_throw<std::logic_error>([&] { pool.submit(callable); });
    CHECK(pool.pendingTaskCount() == 0);
    auto next = pool.submit([] { return 42; });
    CHECK(result(next) == 42);
}

void queue_capacity_case(std::size_t capacity, bool use_default) {
    Gate gate;
    std::promise<void> started;
    auto ready = started.get_future();
    std::atomic<int> rejected_runs{0};
    auto pool = use_default ? std::make_unique<ThreadPool>(1)
                            : std::make_unique<ThreadPool>(1, capacity);
    OpenOnExit release{gate};
    auto running = pool->submit([&] { started.set_value(); gate.wait(); });
    result(ready);
    CHECK(pool->pendingTaskCount() == 0); // Running tasks do not occupy queue slots.
    std::vector<std::future<int>> queued;
    for (std::size_t i = 0; i < capacity; ++i) {
        queued.push_back(pool->submit([i] { return static_cast<int>(i); }));
        CHECK(pool->pendingTaskCount() == i + 1);
    }
    expect_throw<std::runtime_error>([&] {
        pool->submit([&] { ++rejected_runs; });
    });
    CHECK(pool->pendingTaskCount() == capacity);
    CHECK(running.wait_for(0s) == std::future_status::timeout);
    gate.open();
    result(running);
    for (std::size_t i = 0; i < capacity; ++i) CHECK(result(queued[i]) == static_cast<int>(i));
    auto next = pool->submit([] { return 42; });
    CHECK(result(next) == 42);
    pool->shutdown();
    CHECK(rejected_runs == 0);
    CHECK(pool->pendingTaskCount() == 0);
}

void minimum_queue_capacity() { queue_capacity_case(1, false); }
void explicit_queue_capacity() { queue_capacity_case(4, false); }
void default_queue_capacity() { queue_capacity_case(10, true); }

void single_worker_fifo() {
    Gate gate;
    std::vector<int> order;
    ThreadPool pool(1, 128);
    OpenOnExit release{gate};
    auto running = pool.submit([&] { gate.wait(); });
    std::vector<std::future<void>> tasks;
    for (int i = 0; i < 100; ++i) tasks.push_back(pool.submit([&, i] { order.push_back(i); }));
    gate.open();
    result(running);
    for (auto& task : tasks) result(task);
    CHECK(order.size() == 100);
    for (int i = 0; i < 100; ++i) CHECK(order[i] == i);
}

void all_workers_run_concurrently() {
    constexpr int count = 4;
    Gate gate;
    std::atomic<int> active{0};
    std::promise<void> all_started;
    auto ready = all_started.get_future();
    ThreadPool pool(count, count);
    OpenOnExit release{gate};
    std::vector<std::future<std::thread::id>> tasks;
    for (int i = 0; i < count; ++i) tasks.push_back(pool.submit([&] {
        if (++active == count) all_started.set_value();
        gate.wait();
        --active;
        return std::this_thread::get_id();
    }));
    result(ready);
    CHECK(active == count);
    CHECK(pool.pendingTaskCount() == 0);
    gate.open();
    std::vector<std::thread::id> ids;
    for (auto& task : tasks) ids.push_back(result(task));
    for (int i = 0; i < count; ++i) {
        CHECK(ids[i] != std::this_thread::get_id());
        for (int j = 0; j < i; ++j) CHECK(ids[i] != ids[j]);
    }
    CHECK(active == 0);
}

void concurrent_producers_exactly_once() {
    constexpr int producers = 8, per_producer = 250, total = producers * per_producer;
    std::vector<std::atomic<int>> hits(total);
    for (auto& hit : hits) hit.store(0);
    std::atomic<int> errors{0};
    Gate start;
    ThreadPool pool(4, total);
    std::vector<std::thread> threads;
    JoinOnExit join{threads};
    OpenOnExit release{start};
    for (int p = 0; p < producers; ++p) threads.emplace_back([&, p] {
        start.wait();
        try {
            std::vector<std::future<int>> tasks;
            for (int j = 0; j < per_producer; ++j) {
                const int id = p * per_producer + j;
                tasks.push_back(pool.submit([&, id] { ++hits[id]; return id; }));
            }
            for (int j = 0; j < per_producer; ++j)
                if (result(tasks[j]) != p * per_producer + j) ++errors;
        } catch (...) { ++errors; }
    });
    start.open();
    for (auto& thread : threads) thread.join();
    pool.shutdown();
    CHECK(errors == 0);
    for (auto& hit : hits) CHECK(hit == 1);
}

void concurrent_queue_overflow() {
    constexpr int capacity = 16, producers = 8, attempts = 32;
    Gate gate, start;
    std::promise<void> started;
    auto ready = started.get_future();
    std::atomic<int> accepted{0}, rejected{0}, executed{0}, unexpected{0};
    ThreadPool pool(1, capacity);
    OpenOnExit unblock_worker{gate};
    auto running = pool.submit([&] { started.set_value(); gate.wait(); });
    result(ready);
    std::vector<std::thread> threads;
    JoinOnExit join{threads};
    OpenOnExit unblock_producers{start};
    for (int i = 0; i < producers; ++i) threads.emplace_back([&] {
        start.wait();
        for (int j = 0; j < attempts; ++j) {
            try { pool.submit([&] { ++executed; }); ++accepted; }
            catch (const std::runtime_error&) { ++rejected; }
            catch (...) { ++unexpected; }
        }
    });
    start.open();
    for (auto& thread : threads) thread.join();
    CHECK(accepted == capacity);
    CHECK(rejected == producers * attempts - capacity);
    CHECK(unexpected == 0);
    CHECK(pool.pendingTaskCount() == capacity);
    gate.open();
    result(running);
    pool.shutdown();
    CHECK(executed == accepted);
}

void shutdown_waits_and_drains_queue() {
    Gate gate;
    std::promise<void> started, closed;
    auto ready = started.get_future();
    auto shutdown_done = closed.get_future();
    std::atomic<int> completed{0};
    ThreadPool pool(1, 4096);
    std::vector<std::thread> threads;
    JoinOnExit join{threads};
    OpenOnExit release{gate}; // Unblock the worker before joining on failure.
    auto running = pool.submit([&] { started.set_value(); gate.wait(); ++completed; });
    result(ready);
    std::vector<std::future<void>> queued;
    for (int i = 0; i < 20; ++i) queued.push_back(pool.submit([&] { ++completed; }));
    threads.emplace_back([&] { pool.shutdown(); closed.set_value(); });

    // Observe the closed state while the worker remains blocked. A full queue
    // alone is not proof that shutdown has started, so distinguish the errors.
    bool stopped = false;
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!stopped && std::chrono::steady_clock::now() < deadline) {
        try { pool.submit([] {}); }
        catch (const std::runtime_error& error) {
            stopped = std::string(error.what()) == "cannot submit a task to a stopped ThreadPool";
        }
        std::this_thread::yield();
    }
    CHECK(stopped);
    CHECK(shutdown_done.wait_for(0s) == std::future_status::timeout);
    CHECK(completed == 0);
    gate.open();
    result(shutdown_done);
    result(running);
    for (auto& task : queued) result(task);
    CHECK(completed == 21);
    CHECK(pool.pendingTaskCount() == 0);
    threads.front().join();
    pool.shutdown();
}

void submission_races_with_shutdown() {
    constexpr int count = 4, attempts = 500;
    std::atomic<int> accepted{0}, rejected{0}, executed{0}, unexpected{0};
    Gate start;
    ThreadPool pool(4, count * attempts);
    std::vector<std::thread> threads;
    JoinOnExit join{threads};
    OpenOnExit release{start};
    for (int i = 0; i < count; ++i) threads.emplace_back([&] {
        start.wait();
        for (int j = 0; j < attempts; ++j) {
            try { pool.submit([&] { ++executed; }); ++accepted; }
            catch (const std::runtime_error&) { ++rejected; }
            catch (...) { ++unexpected; }
        }
    });
    start.open();
    pool.shutdown(); // Only one shutdown caller; producers may submit concurrently.
    for (auto& thread : threads) thread.join();
    CHECK(unexpected == 0);
    CHECK(accepted + rejected == count * attempts);
    CHECK(executed == accepted);
    CHECK(pool.pendingTaskCount() == 0);
    expect_throw<std::runtime_error>([&] { pool.submit([] {}); });
}

void destructor_drains_and_futures_survive() {
    std::atomic<int> completed{0};
    std::vector<std::future<int>> tasks;
    std::future<void> failure;
    {
        ThreadPool pool(3, 501);
        for (int i = 0; i < 500; ++i)
            tasks.push_back(pool.submit([&, i] { ++completed; return i * i; }));
        failure = pool.submit([] { throw std::logic_error("saved exception"); });
    }
    CHECK(completed == 500);
    for (int i = 0; i < 500; ++i) CHECK(result(tasks[i]) == i * i);
    expect_throw<std::logic_error>([&] { result(failure); });
}

void discarded_futures_still_execute() {
    std::atomic<int> completed{0};
    {
        ThreadPool pool(2, 201);
        for (int i = 0; i < 200; ++i) pool.submit([&] { ++completed; });
        pool.submit([] { throw std::runtime_error("unobserved exception"); });
    }
    CHECK(completed == 200);
}

void captured_resources_are_released() {
    std::weak_ptr<int> weak;
    {
        ThreadPool pool(1);
        auto resource = std::make_shared<int>(42);
        weak = resource;
        auto task = pool.submit([resource] { return *resource; });
        resource.reset();
        CHECK(result(task) == 42);
        pool.shutdown();
    }
    CHECK(weak.expired());
}

void worker_can_submit_without_waiting() {
    ThreadPool pool(1, 4);
    auto outer = pool.submit([&] { return pool.submit([] { return 42; }); });
    auto inner = result(outer);
    CHECK(result(inner) == 42);
}

void repeated_lifecycle() {
    for (int i = 0; i < 50; ++i) {
        ThreadPool pool(1 + i % 4);
        auto task = pool.submit([i] { return i; });
        CHECK(result(task) == i);
        if (i % 2 == 0) pool.shutdown();
    }
}

struct Test { const char* name; void (*run)(); };
const Test tests[] = {
    {"move_only_task_empty_and_invocation", move_only_task_empty_and_invocation},
    {"move_only_task_inline_lifetime", move_only_task_inline_lifetime},
    {"move_only_task_large_and_aligned", move_only_task_large_and_aligned},
    {"move_only_task_throwing_construction", move_only_task_throwing_construction},
    {"invalid_configuration", invalid_configuration},
    {"empty_pool_and_repeated_shutdown", empty_pool_and_repeated_shutdown},
    {"values_and_callables", values_and_callables},
    {"void_and_reference_results", void_and_reference_results},
    {"argument_copy_and_reference", argument_copy_and_reference},
    {"move_only_arguments_and_functors", move_only_arguments_and_functors},
    {"member_functions_and_reference_wrapper", member_functions_and_reference_wrapper},
    {"task_exceptions_do_not_kill_workers", task_exceptions_do_not_kill_workers},
    {"submission_copy_failure_leaves_pool_usable", submission_copy_failure_leaves_pool_usable},
    {"minimum_queue_capacity", minimum_queue_capacity},
    {"explicit_queue_capacity", explicit_queue_capacity},
    {"default_queue_capacity", default_queue_capacity},
    {"single_worker_fifo", single_worker_fifo},
    {"all_workers_run_concurrently", all_workers_run_concurrently},
    {"concurrent_producers_exactly_once", concurrent_producers_exactly_once},
    {"concurrent_queue_overflow", concurrent_queue_overflow},
    {"shutdown_waits_and_drains_queue", shutdown_waits_and_drains_queue},
    {"submission_races_with_shutdown", submission_races_with_shutdown},
    {"destructor_drains_and_futures_survive", destructor_drains_and_futures_survive},
    {"discarded_futures_still_execute", discarded_futures_still_execute},
    {"captured_resources_are_released", captured_resources_are_released},
    {"worker_can_submit_without_waiting", worker_can_submit_without_waiting},
    {"repeated_lifecycle", repeated_lifecycle},
};
} // namespace

int main(int argc, char** argv) {
    const std::string argument = argc > 1 ? argv[1] : "";
    const bool functional_only = argument == "--functional-only";
    const std::string filter = functional_only ? "" : argument;
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0]
                  << " [test-name-substring | --list | --functional-only]\n";
        return 2;
    }
    if (filter == "--list") {
        for (const auto& test : tests) std::cout << test.name << '\n';
        return 0;
    }
    int passed = 0, failed = 0;
    std::cout << "=== Functional tests ===\n";
    for (const auto& test : tests) {
        if (std::string(test.name).find(filter) == std::string::npos) continue;
        std::cout << "[RUN ] " << test.name << std::endl;
        Watchdog watchdog(test.name);
        try {
            test.run();
            ++passed;
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        } catch (...) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
        }
    }
    std::cout << "\nPassed: " << passed << ", failed: " << failed << '\n';
    if (passed + failed == 0) { std::cerr << "No matching tests\n"; return 2; }
    if (failed != 0) {
        std::cerr << "Performance benchmarks skipped because functional tests failed.\n";
        return 1;
    }
    // Name filters retain their original behavior: run only matching functional tests.
    if (functional_only || !filter.empty()) return 0;
    Watchdog benchmark_watchdog("performance benchmarks", 180s);
    return run_thread_pool_benchmarks();
}
