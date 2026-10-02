#ifndef TINYSERVER_THREADPOOL_HPP
#define TINYSERVER_THREADPOOL_HPP

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <new>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// C++20 的只移动 void() 包装。小对象直接存储，其他对象在堆上存储。
// 内联对象必须能无异常移动；因此包装本身的移动操作始终 noexcept。
class MoveOnlyTask {
public:
    MoveOnlyTask() noexcept = default;
    MoveOnlyTask(std::nullptr_t) noexcept {}

    template <class Function, class Stored = std::decay_t<Function>,
              std::enable_if_t<!std::is_same_v<Stored, MoveOnlyTask>, int> = 0,
              std::enable_if_t<std::is_constructible_v<Stored, Function&&> &&
                               std::is_invocable_r_v<void, Stored&> &&
                               std::is_nothrow_destructible_v<Stored>, int> = 0>
    explicit MoveOnlyTask(Function&& function) {
        if constexpr (std::is_pointer_v<Stored>) {
            if (function == nullptr) return;
        }
        if constexpr (fits_inline<Stored>) {
            object_ = ::new (static_cast<void*>(storage_)) Stored(std::forward<Function>(function));
        } else {
            object_ = new Stored(std::forward<Function>(function));
        }
        operations_ = operations_for<Stored>();
    }

    MoveOnlyTask(const MoveOnlyTask&) = delete;
    MoveOnlyTask& operator=(const MoveOnlyTask&) = delete;

    MoveOnlyTask(MoveOnlyTask&& other) noexcept { move_from(other); }
    MoveOnlyTask& operator=(MoveOnlyTask&& other) noexcept {
        if (this != &other) {
            reset();
            move_from(other);
        }
        return *this;
    }

    ~MoveOnlyTask() noexcept { reset(); }

    explicit operator bool() const noexcept { return operations_ != nullptr; }

    void operator()() {
        if (!operations_) throw std::bad_function_call();
        operations_->invoke(object_);
    }

    void reset() noexcept {
        if (operations_) operations_->destroy(object_);
        object_ = nullptr;
        operations_ = nullptr;
    }

private:
    static constexpr std::size_t inline_bytes = 32;
    template <class T>
    static constexpr bool fits_inline = sizeof(T) <= inline_bytes &&
        alignof(T) <= alignof(std::max_align_t) && std::is_nothrow_move_constructible_v<T>;

    struct Operations {
        void (*invoke)(void*);
        void (*destroy)(void*) noexcept;
        void* (*relocate)(void*, void*) noexcept; // nullptr 表示转移堆指针即可。
    };

    template <class T>
    static const Operations* operations_for() noexcept {
        if constexpr (fits_inline<T>) {
            static const Operations operations{
                [](void* object) { static_cast<void>(std::invoke(*static_cast<T*>(object))); },
                [](void* object) noexcept { static_cast<T*>(object)->~T(); },
                [](void* source, void* destination) noexcept -> void* {
                    // 必须调用移动构造和析构，不能 memcpy 非平凡对象。
                    auto* moved = ::new (destination) T(std::move(*static_cast<T*>(source)));
                    static_cast<T*>(source)->~T();
                    return moved;
                }
            };
            return &operations;
        } else {
            static const Operations operations{
                [](void* object) { static_cast<void>(std::invoke(*static_cast<T*>(object))); },
                [](void* object) noexcept { delete static_cast<T*>(object); },
                nullptr
            };
            return &operations;
        }
    }

    void move_from(MoveOnlyTask& other) noexcept {
        if (!other.operations_) return;
        object_ = other.operations_->relocate
            ? other.operations_->relocate(other.object_, static_cast<void*>(storage_))
            : other.object_;
        operations_ = std::exchange(other.operations_, nullptr);
        other.object_ = nullptr;
    }

    alignas(std::max_align_t) std::byte storage_[inline_bytes];
    void* object_ = nullptr;
    const Operations* operations_ = nullptr;
};

// 本身不负责同步，调用者必须持有线程池的 mutex。
class TaskRingBuffer {
public:
    explicit TaskRingBuffer(std::size_t capacity)
        : slots_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument(
                "TaskRingBuffer capacity must be positive");
        }
    }

    TaskRingBuffer(const TaskRingBuffer&) = delete;
    TaskRingBuffer& operator=(const TaskRingBuffer&) = delete;
    TaskRingBuffer(TaskRingBuffer&&) = delete;
    TaskRingBuffer& operator=(TaskRingBuffer&&) = delete;

    [[nodiscard]] bool empty() const noexcept {
        return size_ == 0;
    }

    [[nodiscard]] bool full() const noexcept {
        return size_ == slots_.size();
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    // 满时返回 false，不消耗传入的任务。
    bool try_push(MoveOnlyTask&& task) noexcept {
        if (full()) {
            return false;
        }

        slots_[tail_] = std::move(task);
        advance(tail_);
        ++size_;
        return true;
    }

    // 空时返回 false，不改变输出参数。
    bool try_pop(MoveOnlyTask& task) noexcept {
        if (empty()) {
            return false;
        }

        task = std::move(slots_[head_]);
        // MoveOnlyTask 保证移动后的源槽位为空。
        advance(head_);
        --size_;
        return true;
    }

private:
    void advance(std::size_t& index) noexcept {
        if (++index == slots_.size()) {
            index = 0;
        }
    }

    std::vector<MoveOnlyTask> slots_;
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t size_ = 0;
};


// 一个简单的 C++20 固定大小线程池。
//
// - 任务由 MoveOnlyTask 管理，直接持有 packaged_task，无需 shared_ptr 包装。
// - submit() 返回 std::future，任务返回值和异常都可以被调用者获取。
// - 析构时停止接收新任务，完成队列中已有任务，然后等待所有线程退出。
class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_workers,
                        std::size_t max_pending_tasks = 10)
        : tasks_(max_pending_tasks), max_pending_tasks_(max_pending_tasks) {
        if (num_workers == 0) {
            throw std::invalid_argument("ThreadPool requires at least one worker");
        }
        if (max_pending_tasks_ == 0) {
            throw std::invalid_argument(
                "ThreadPool requires a non-zero task queue capacity");
        }

        workers_.reserve(num_workers);

        try {
            for (std::size_t i = 0; i < num_workers; ++i) {
                workers_.emplace_back([this] { workerLoop(); });
            }
        } catch (...) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stopping_ = true;
            }
            task_available_.notify_all();

            for (auto& worker : workers_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            throw;
        }
    }

    ~ThreadPool() noexcept {
        shutdown();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    // 提交任意可调用对象及其参数。
    // 队列已满时立即抛出 std::runtime_error，不会阻塞或静默丢弃任务。
    template <class Function, class... Args>
    auto submit(Function&& function, Args&&... args)
        -> std::future<std::invoke_result_t<std::decay_t<Function>,
                                            std::decay_t<Args>...>> {
        using ReturnType =
            std::invoke_result_t<std::decay_t<Function>,
                                 std::decay_t<Args>...>;

        std::packaged_task<ReturnType()> task(
            [function = std::decay_t<Function>(
                 std::forward<Function>(function)),
             arguments = std::make_tuple(std::forward<Args>(args)...)]()
                mutable -> ReturnType {
                return std::apply(
                    [&function](auto&&... unpacked) -> ReturnType {
                        return std::invoke(
                            std::move(function),
                            std::forward<decltype(unpacked)>(unpacked)...);
                    },
                    std::move(arguments));
            });

        auto result = task.get_future();
        // 在锁外完成任务包装；future 的共享状态仍由 packaged_task 管理。
        MoveOnlyTask submit_task{[task = std::move(task)]() mutable { task(); }};

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (stopping_) {
                throw std::runtime_error(
                    "cannot submit a task to a stopped ThreadPool");
            }
            if (!tasks_.try_push(std::move(submit_task))) {
                throw std::runtime_error("ThreadPool task queue is full");
            }
        }

        task_available_.notify_one();
        return result;
    }

    // graceful shutdown：拒绝新任务，完成已入队任务并等待 worker 退出。
    // 可重复调用。
    void shutdown() noexcept {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }

        task_available_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    [[nodiscard]] std::size_t workerCount() const noexcept {
        return workers_.size();
    }

    [[nodiscard]] std::size_t pendingTaskCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }

private:
    void workerLoop() noexcept {
        while (true) {
            MoveOnlyTask task;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                task_available_.wait(lock, [this] {
                    return stopping_ || !tasks_.empty();
                });

                if (stopping_ && tasks_.empty()) {
                    return;
                }

                tasks_.try_pop(task);
            }

            // packaged_task 会把任务异常保存到 future 中，因此不会让
            // worker 线程因用户任务抛异常而退出。
            task();
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable task_available_;
    TaskRingBuffer tasks_;
    std::vector<std::thread> workers_;
    const std::size_t max_pending_tasks_;
    bool stopping_ = false;
};

#endif // TINYSERVER_THREADPOOL_HPP
