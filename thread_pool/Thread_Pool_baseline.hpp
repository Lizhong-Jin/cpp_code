#ifndef TINYSERVER_THREADPOOL_BASELINE_HPP
#define TINYSERVER_THREADPOOL_BASELINE_HPP

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace baseline {

// 一个简单的 C++17 固定大小线程池。
//
// - 任务由 std::function 管理，无需手动 malloc/free。
// - submit() 返回 std::future，任务返回值和异常都可以被调用者获取。
// - 析构时停止接收新任务，完成队列中已有任务，然后等待所有线程退出。
class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_workers,
                        std::size_t max_pending_tasks = 10)
        : max_pending_tasks_(max_pending_tasks) {
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

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
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

        auto result = task->get_future();

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (stopping_) {
                throw std::runtime_error(
                    "cannot submit a task to a stopped ThreadPool");
            }
            if (tasks_.size() >= max_pending_tasks_) {
                throw std::runtime_error("ThreadPool task queue is full");
            }

            // std::function 在 C++17 中要求目标可复制；shared_ptr 用来包装
            // 只能移动的 packaged_task。
            tasks_.emplace([task] { (*task)(); });
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
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                task_available_.wait(lock, [this] {
                    return stopping_ || !tasks_.empty();
                });

                if (stopping_ && tasks_.empty()) {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }

            // packaged_task 会把任务异常保存到 future 中，因此不会让
            // worker 线程因用户任务抛异常而退出。
            task();
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable task_available_;
    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    const std::size_t max_pending_tasks_;
    bool stopping_ = false;
};

} // namespace baseline

#endif // TINYSERVER_THREADPOOL_BASELINE_HPP
