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

// 支持移动语义、类型擦除和小对象优化的通用任务包装器
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
    template <typename T>
    static constexpr bool fits_inline = sizeof(T) <= inline_bytes &&
        alignof(T) <= alignof(std::max_align_t) && std::is_nothrow_move_constructible_v<T>;

    struct Operations {
        void (*invoke)(void*);
        void (*destroy)(void*) noexcept;
        void* (*relocate)(void*, void*) noexcept; // nullptr 表示转移堆指针即可。
    };

    template <typename T>
    static const Operations* operations_for() noexcept {
        if constexpr (fits_inline<T>) {
            static const Operations operations{
                [](void* object) { static_cast<void>(std::invoke(*static_cast<T*>(object))); },
                [](void* object) noexcept { static_cast<T*>(object)->~T(); },
                [](void* source, void* destination) noexcept -> void* {
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

// 环形缓冲区
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

    bool try_push(MoveOnlyTask&& task) noexcept {
        if (full()) {
            return false;
        }
        slots_[tail_] = std::move(task);
        advance(tail_);
        ++size_;
        return true;
    }

    bool try_pop(MoveOnlyTask& task) noexcept {
        if (empty()) {
            return false;
        }
        task = std::move(slots_[head_]);
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

enum class SubmitStatus {
    Accepted,
    QueueFull,
    Closed
};

// 一个简单的 C++20 固定大小线程池。
//
// 任务由 MoveOnlyTask 管理，直接持有 packaged_task。
// submit() 返回 std::future，任务返回值和异常都可以被调用者获取。
// 析构时停止接收新任务，完成队列中已有任务，然后等待所有线程退出。
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

private:
    template <typename T>
    struct PreparedTask {
        MoveOnlyTask task;
        std::future<T> future;
    };

public:
    template <typename Function, typename... Args>
    auto prepare(Function&& function, Args&&... args)
        -> PreparedTask<std::invoke_result_t<std::decay_t<Function>,
                                            std::decay_t<Args>...>>
    {
        using ReturnType =
            std::invoke_result_t<std::decay_t<Function>, std::decay_t<Args>...>;

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
        
        auto future = task.get_future();
        MoveOnlyTask wrapped{[task = std::move(task)]() mutable {task();}};
        return PreparedTask<ReturnType>{std::move(wrapped), std::move(future)};
    }

    // 尝试提交任意可调用对象及其参数。
    template <typename R>
    [[nodiscard]] SubmitStatus Trysubmit(PreparedTask<R>& prepared) {
        if (!prepared.task)
            throw std::invalid_argument("empty prepared task");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                return SubmitStatus::Closed;
            }
            if (!task.try_push(std::move(prepared.task))) {
                return SubmitStatus::QueueFull;
            }
        }
        task_available_.notify_one();
        return SubmitStatus::Accepted;
    }


    // 提交任意可调用对象及其参数, 失败时抛出异常
    template <typename R>
    void submit(PreparedTask<R>& prepared) {
        const auto status = trySubmit(prepared);

        if (status == SubmitStatus::QueueFull)
            throw std::runtime_error("ThreadPool task queue is full");

        if (status == SubmitStatus::Closed)
            throw std::runtime_error("cannot submit a task to a stopped ThreadPool");
    }

    // 提交任意可调用对象及其参数, 失败时抛出异常，并可能丢失原对象
    // 返回 future
    template<typename Function, typename... Args>
    auto submit(Function&& function, Args&&... args)
        -> std::future<std::invoke_result_t<
            std::decay_t<Function>, std::decay_t<Args>...>>
    {
        auto prepared = prepare(
            std::forward<Function>(function),
            std::forward<Args>(args)...);

        submit(prepared);
        return std::move(prepared.future);
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
