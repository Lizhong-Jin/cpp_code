# 线程池功能与性能测试 / Thread pool functional tests and benchmarks

独立的 C++20 测试程序，无第三方测试框架依赖。功能测试在 `Thread_Pool_test.cpp`，性能测试在 `Thread_Pool_benchmark.cpp`，共同编译为一个可执行文件。默认先运行全部功能测试，通过后再运行性能测试。以下命令均在本目录执行。

Standalone C++20 tests with no third-party test framework. Functional tests and benchmarks live in separate source files but share one executable. By default, all functional tests run first; benchmarks follow only if they pass. Run the commands below from this directory.

## 编译与运行 / Build and run

```sh
mkdir -p build
c++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -pthread Thread_Pool_test.cpp Thread_Pool_benchmark.cpp -o build/thread_pool_tests
./build/thread_pool_tests
```

仅运行功能测试、列出用例或按名称片段筛选（筛选时不运行性能测试） / Run only functional tests, list them, or filter by name (filters skip benchmarks):

```sh
./build/thread_pool_tests --functional-only
./build/thread_pool_tests --list
./build/thread_pool_tests shutdown
./build/thread_pool_tests concurrent
```

也可以使用 CMake / Alternatively, use CMake:

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release -V
```

## 覆盖范围 / Coverage

- `MoveOnlyTask` 空状态、只移动语义、内联对象生命周期、大对象/超对齐对象回退、可能抛异常的移动与构造失败。 / MoveOnlyTask empty states, move-only ownership, inline lifetimes, large/over-aligned fallback, potentially throwing moves and construction failures.
- 参数校验、线程数量、空队列、禁止复制和移动。 / Configuration, worker count, empty queues, noncopyable and nonmovable semantics.
- 普通函数、Lambda、成员函数、成员数据、引用包装器。 / Functions, lambdas, member functions, member data, reference wrappers.
- 普通返回值、void、引用、仅可移动的参数、返回值和函数对象。 / Value, void, reference and move-only results, arguments and callables.
- 参数复制与显式引用、提交时复制失败、任务异常传递及线程复用。 / Argument ownership, submission copy failure, exception propagation and worker reuse.
- 默认容量、自定义容量、最小容量、队列满时拒绝、容量恢复、单线程 FIFO。 / Queue limits, overflow rejection, capacity recovery and single-worker FIFO.
- 工作线程同时执行、多生产者提交、任务恰好执行一次、并发队列溢出。 / Parallel workers, concurrent producers, exactly-once execution and concurrent overflow.
- 关闭等待、清空已接收任务、关闭期间拒绝提交、提交与关闭竞争、顺序重复关闭。 / Shutdown waiting and draining, rejection during shutdown, submission/shutdown races and sequential repeated shutdown.
- 析构完成任务、future 跨越线程池生命周期、丢弃 future、资源释放、任务内部提交及反复创建销毁。 / Destruction, future lifetime, discarded futures, resource cleanup, nested submission and repeated lifecycle.

每个功能测试有 15 秒看门狗，防止错误实现导致无限等待。失败返回非零退出码；检查不会因 `NDEBUG` 被关闭。并发顺序通过同步原语建立，不依赖固定时长的睡眠。

Each test has a 15-second watchdog. Failures return a nonzero exit code, and checks remain enabled under `NDEBUG`. Synchronization establishes ordering without fixed sleeps.

当前 `Thread_Pool.hpp` 内的 `MoveOnlyTask` 是 C++20 的 `void()` 任务包装：提供 32 字节、`max_align_t` 对齐的内联存储，仅对大小/对齐适合且移动构造不抛异常的对象启用；其他对象使用堆存储。禁止复制，移动构造与赋值为 `noexcept`，移动后源对象为空；空任务调用抛出 `std::bad_function_call`。线程池在锁外直接包装 `packaged_task`，去掉外层 `shared_ptr`，但仍保留 future 共享状态。baseline 保留原有实现。

`MoveOnlyTask` lives in `Thread_Pool.hpp`. Its 32-byte buffer, aligned to `max_align_t`, stores fitting, nothrow-movable callables inline; other callables use heap storage. Copies are disabled, moves are noexcept and empty the source, and invoking an empty task throws `std::bad_function_call`. The pool wraps `packaged_task` outside the queue lock without an outer `shared_ptr`; future shared state remains. Baseline retains the original implementation.

## 性能测试 / Performance benchmarks

功能测试通过后，性能结果按五个编号分组输出为对齐表格。每个线程数下，`baseline` 和 `current` 两行相邻，便于直接比较；第四、五组额外加入无线程池的 `trivial` 对照。

| 输出名称 | 实现 |
| --- | --- |
| `current` | `Thread_Pool.hpp` 中的 `ThreadPool` |
| `baseline` | `Thread_Pool_baseline.hpp` 中的 `baseline::ThreadPool` |
| `trivial` | 每个任务创建一个 `std::thread`，任务完成后 `join()` 回收；不复用线程 |
| `serial` | 直接串行执行相同 CPU 计算，仅用作 CPU 加速比参考，不是 baseline 线程池 |

两个线程池使用独立的头文件保护宏与类型，避免同名实现被跳过或在链接时混用。baseline 仅增加命名空间，算法保持原样；后续可分别修改两个实现文件。功能测试仍针对 current，性能测试对两个实现均校验任务结果。

1. **小任务吞吐量**：每批 20,000 个任务。输出整批耗时 `Batch(ms)`、每秒完成任务数 `Tasks/s`、平均提交耗时 `Submit(ns/task)` 和相对 baseline 的倍率。
2. **CPU 任务吞吐量**：每批 1,000 个任务，每个执行 20,000 次整数混合运算。输出上述指标，并额外显示相对直接串行执行的 `Vs serial`。
3. **低负载往返延迟**：每次提交一个任务并等待 `future.get()`。每个实现预热 1,000 次，采集 6,000 个样本，输出 P50、P95、P99（微秒）及各分位数相对 baseline 的倍率。
4. **重 CPU 任务压力测试**：每轮 512 个任务，每个执行 500,000 次运算，是第二组单任务计算量的 25 倍。对比 baseline、current、trivial，并以直接串行执行作参考；输出中位耗时、最小/最大耗时、吞吐量、每轮创建线程数和三种相对倍率。
5. **轻任务高并发测试**：每轮 100,000 个任务，每个仅执行 32 次整数混合运算。固定工作线程数，比较 1、4、16 个生产者同时向同一执行器提交任务时 baseline、current、trivial 的耗时和吞吐量。

倍率的定义统一为 **参考耗时 / 该行耗时**。`Vs baseline` 使用相同线程数的 baseline 整批耗时；`P50/P95/P99 vs base` 使用相应分位数；`Vs serial` 使用直接串行执行耗时。大于 `1x` 表示该行更快，小于 `1x` 表示更慢；baseline 相对自身为 `1.000x`。例如 current 的 `Vs baseline = 1.250x` 表示完成同批任务的吞吐量为 baseline 的 1.25 倍、耗时为其 80%。

前三组测量约定：

- 使用 1、2、4、8 个工作线程，最高不超过系统报告的硬件并发数；也测试非 2 的幂的上限，无法获取时使用 1 个线程。
- 两个实现共用同一套模板测试逻辑、任务输入、任务数和队列容量。每项各预热一轮、测量六轮，每轮交替先后顺序，使双方各先运行三轮。两个线程池同时存在，但逐个测量，另一方空闲。
- 批量耗时和提交耗时分别取六轮中位数；`Tasks/s` 按任务数除以耗时中位数计算。延迟将六轮样本合并后计算分位数。
- 使用单生产者和单调时钟 `steady_clock`，不计线程池创建和销毁。整批耗时包括提交及收取全部结果；提交耗时包括保存 future 的开销及与工作线程的竞争。
- 批量测试队列容量等于整批任务数，避免队列满导致拒绝；延迟测试容量为 1。往返延迟包括提交、调度、任务执行和结果唤醒，不单独代表排队延迟。
- 性能测试整体保留 180 秒看门狗（CTest 总超时为 240 秒）。耗时无固定通过阈值；结果校验失败、异常或超时会使程序失败。使用 Release / `-O2` 构建，在机器、编译器、编译选项一致且系统较空闲时比较。相同实现也会因调度、系统负载和温度出现差异，单次比值不能证明优化有效。

第四组重任务压力测试的设计：

- **控制并发数**：所有并发实现使用相同大小的滚动窗口，`Limit` 等于线程池工作线程数。提交窗口满后，先取回最早任务的结果，再提交下一个；trivial 在复用窗口槽位前还会回收旧线程。不会一次创建 512 个并发线程，也不会等整批结束才补充任务。FIFO 收取结果仍可能等待较慢的槽位，这一限制对所有并发实现相同。
- **明确无线程池对照**：trivial 每轮累计创建 512 个线程，同时存活不超过 `Limit`；线程池每轮只创建 `Limit` 个工作线程；serial 在调用线程中执行，创建线程数为零。串行参考值单独测量一次并用于所有并发数，不参与三个并发实现的轮换。
- **统一输入和结果处理**：各实现使用相同输入、计算和结果校验。所有并发实现均通过 `future` 获取结果，trivial 使用 `packaged_task` 传递返回值和异常。每个任务结果在计时结束后与串行计算逐个比较。
- **计入完整生命周期**：与前三组不同，第四组从创建执行器开始计时，包含线程池/线程创建、提交、收取结果、销毁和所有 `join()`；公共结果数组的分配及结果校验不计入耗时。每次运行都会重新创建执行器，避免只对 trivial 计入创建成本。
- **重复与轮换**：每种实现预热一轮、测量三轮，输出中位数和最小/最大值。三种并发实现依次轮换执行次序，每种恰好先运行一次。每次只存在当前受测执行器。
- **读取比值**：`Vs baseline`、`Vs trivial`、`Vs serial` 都是相应参考耗时除以该行耗时，大于 1x 表示该行更快。`Threads/run` 表示累计创建的线程数，并非峰值并发数。

这是有限任务量的重负载测试，用来观察持续计算和线程创建/复用的差异；不是长时间稳定性测试或无限制创建线程的资源极限测试。重计算可能掩盖线程管理开销，因此不应预设线程池一定大幅领先 trivial。可在 `Thread_Pool_benchmark.cpp` 中调整 `stress_tasks`、`stress_iterations`、`stress_rounds`；大幅增加负载时需同步调整主程序的性能看门狗和 CTest 超时。

第五组轻任务高并发测试的设计：

- **真实的多生产者提交**：工作线程数固定为 `min(8, hardware_concurrency)`，无法获取硬件并发数时取 1；生产者数依次为 1、4、16。生产者共用同一执行器，由同一个启动门放行。每种配置的总任务数均为 100,000，增加生产者数不会增加工作总量。
- **限制在途任务**：每个生产者使用 64 个 future 槽位的滚动窗口，取回旧结果后才能补充新任务。表格 `Max flight` 是配置的总在途上限，分别为 64、256、1024，并非实测峰值。两个线程池的队列容量也设为该上限，避免以拒绝任务的方式虚增吞吐量。
- **无线程池对照**：第五组使用支持多生产者的 `ConcurrentThreadPerTask`，每个任务仍新建一个线程，任务线程数达到工作线程上限时，提交者等待空闲槽位。旧线程先 join，再替换为新线程；不会把十万个线程一次性创建出来。该实现包含并发控制和线程创建成本，线程池则允许任务先进入队列，因此比较的是完整处理耗时，不是相同排队策略下的纯互斥锁开销。
- **一致的测量与校验**：各实现使用相同任务输入、总数和每生产者窗口，预热一轮后测量三轮，轮换执行顺序。计时覆盖执行器和生产者线程创建、提交、获取结果及全部线程回收；任务结果在生产者与执行器退出后逐项校验。所有生产者异常回传到主线程，失败时先回收线程，再报告错误。
- **读取输出**：显示中位耗时、最小/最大耗时、每秒完成任务数以及相对 baseline/trivial 的倍率。`Task threads` 是每轮累计创建的任务线程数：线程池为工作线程数，trivial 为 100,000；双方共同的生产者线程不计入这一列。

该场景模拟有在途数量限制的高并发提交，重点观察任务很轻时的入队竞争、调度和线程管理成本，不表示无限到达率或长期稳定性测试。可调整 `concurrent_tasks`、`concurrent_iterations`、`producer_window`、`concurrent_rounds` 改变负载。

当前不覆盖饱和队列延迟、队列满时的性能或内存占用。

After functional tests pass, five aligned tables compare tiny-task throughput, CPU-task throughput, low-load submit-to-get latency, heavy CPU stress, and high-volume concurrent light tasks. Each worker count has adjacent baseline/current rows. `current` is `ThreadPool` from `Thread_Pool.hpp`; `baseline` is `baseline::ThreadPool` from `Thread_Pool_baseline.hpp`. Distinct guards and types keep the implementations independent. The serial reference is direct CPU execution, not the baseline pool. Functional tests target current; benchmarks validate all implementations' results.

In sections 1-3, both implementations use the same templated harness, inputs, task counts and queue capacities. Each has one warmup and six measured rounds, alternating which runs first. Both pools exist during a comparison, but only one executes tasks at a time. Worker counts are powers of two up to eight, capped by hardware concurrency (also testing the cap; falling back to one).

Tiny batches contain 20,000 tasks. CPU batches contain 1,000 tasks of 20,000 iterations each. Batch/submission columns are medians; throughput is task count divided by median batch time. Latency uses one outstanding task, 1,000 warmup requests and 6,000 measured requests per implementation, pooling samples for P50/P95/P99. Every ratio is reference time divided by row time: greater than 1x is faster. Batch ratios use baseline batch time; latency ratios use corresponding baseline percentiles; serial ratios use direct serial time.

Timing in sections 1-3 excludes pool creation/destruction but includes submission and result collection. Submission includes future storage and worker contention. Batch queue capacity equals task count; latency capacity is one. The 180-second performance watchdog prevents hangs; CTest has a 240-second overall timeout. Timing has no pass/fail threshold; result errors, exceptions and timeouts fail the executable. Compare optimized builds under matching conditions; small differences may be noise even for identical implementations. Saturated queue latency, queue-full performance and memory usage are not measured.

Section 4 runs 512 heavy tasks of 500,000 iterations each (25 times the earlier per-task work). Current, baseline and trivial use an identical FIFO rolling window capped at the worker count. Trivial creates a new `std::thread` per task and joins before reusing its slot: 512 threads created per run, but at most `Limit` alive at once. Pools create `Limit` workers per run. All parallel executors return futures; trivial uses `packaged_task` for results and exceptions. A separately measured serial reference uses no additional threads and is reused across concurrency levels.

Stress timing includes the entire executor lifecycle, submissions, results and every thread join. Common result-buffer allocation and per-task verification against serial output occur outside timing. Each implementation warms up once and runs three measured trials; parallel order rotates so each runs first once. Only the active executor exists during each run. Tables report median/min/max time, throughput, cumulative threads created, and speedups against baseline, trivial and serial. This bounded heavy-load test is not a long-duration soak or an unbounded-thread resource-limit test. Heavy computation can dominate thread overhead; pool reuse need not yield a large speedup. Constants `stress_tasks`, `stress_iterations` and `stress_rounds` control the load; larger settings may require updating the performance watchdog and CTest timeout.

Section 5 fixes task concurrency at the hardware-capped worker limit and varies concurrent producers across 1, 4 and 16. Each run processes exactly 100,000 tasks with 32 integer-mixing iterations each. Producers share one executor and start behind a common gate. Each has a FIFO window of 64 futures; the configured total in-flight cap (`Max flight`) and pool queue capacity are 64, 256 or 1024. These are bounds, not measured peaks.

The multi-producer trivial control (`ConcurrentThreadPerTask`) launches a fresh thread per task. Submitters wait for a free task-thread slot; an old thread is joined before the slot is reused. Live task threads stay within the same worker limit as the pools. This measures the full processing cost, including trivial's concurrency control and thread creation, rather than isolating lock overhead under identical queueing semantics.

Each configuration warms up once and runs three trials in rotating implementation order. Timing includes producer/executor construction, submissions, result retrieval and all joins. Producer exceptions are collected and reported after cleanup; each task result is checked outside timing. Tables show median/min/max, throughput, ratios, and cumulative task threads created (excluding common producer threads). Constants `concurrent_tasks`, `concurrent_iterations`, `producer_window` and `concurrent_rounds` control the workload. This is bounded concurrent offered load, not unbounded-arrival latency or a long-duration soak test.

## 内存与数据竞争检查 / Sanitizers

支持相应 Sanitizer 的 Clang/GCC 可使用以下命令。内存检查与线程检查需分别构建。

With a supporting Clang/GCC toolchain, build memory and thread checks separately:

```sh
c++ -std=c++20 -g -O1 -pthread -fsanitize=address,undefined -fno-omit-frame-pointer Thread_Pool_test.cpp Thread_Pool_benchmark.cpp -o build/thread_pool_asan
./build/thread_pool_asan --functional-only

c++ -std=c++20 -g -O1 -pthread -fsanitize=thread Thread_Pool_test.cpp Thread_Pool_benchmark.cpp -o build/thread_pool_tsan
./build/thread_pool_tsan --functional-only
```

## 测试边界 / Limitations

当前实现的 `shutdown()` 只适合由外部线程串行调用。多个线程同时调用会竞争 `join()`；工作线程内部关闭线程池可能等待自身并终止进程。这两种场景不作为受支持行为测试。单线程池任务内提交新任务可以，但不能在该任务内同步等待新任务完成。

The current implementation requires shutdown calls to be serialized and made outside its workers. Concurrent shutdown calls can race on `join()`, and shutdown from a worker can attempt to join itself and terminate the process. These are not tested as supported behavior. Nested submission is supported, but waiting for the nested task inside the sole worker would deadlock.

测试未注入操作系统线程创建失败或内存分配失败，也不能证明所有调度交错均无问题。关闭中的状态检查有一处依赖当前“已关闭”异常文本，以区分关闭和队列已满。

OS thread-creation and allocation failures are not injected. Tests cannot prove correctness for every scheduling interleaving. One shutdown-state check depends on the current stopped-pool exception message to distinguish shutdown from a full queue.
