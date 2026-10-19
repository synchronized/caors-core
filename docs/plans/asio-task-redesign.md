<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Asio 与 Tasks 重构开发计划

> 后端方案已取代：本文保留原迁移设计与历史证据。2026-10-19 起按[标准库调度后端](standard-thread-backends.md)移除 Asio；当前 API 以[任务参考](../reference/tasks.md)为准。

## 文档状态

- 当前状态：已取代。原阶段 0 到阶段 6 的迁移验收已完成，证据仅适用于旧后端；固定 Linux 环境正式性能复验仍为历史延期项；
- 目标：引入固定版本的 standalone Asio，统一当前 `executor`、`completion_queue`、
  `thread_pool_executor` 的调度模型，并为 C++20 coroutine 提供可选适配；
- 首批范围：依赖接入、execution context、轻量 executor、主线程 poll、线程池、现有 task/request
  兼容迁移、内存与错误语义、测试和安装包验证；
- 后续范围：assets/resources 的 coroutine 加载流水线另立计划，不在本计划中直接修改；
- 非目标：不引入 Vulkan，不重构稳定插件 ABI，不实现网络协议、远程 file server、定时器上层业务 API，
  不复制 Asio 的完整 executor property/completion token 体系。

## 背景

当前 tasks 由几个独立对象拼装：

```text
task
  -> executor::submit()
     -> inline_executor
     -> thread_pool_executor

后台任务
  -> completion_queue::push()
     -> 主线程 drain()

submit_request()
  -> request 共享状态
```

主要问题：

- `executor::submit()` 与 `completion_queue::push()` 都表示向执行目标投递任务，但接口不同；
- `thread_pool_executor` 同时承担 execution context、队列、线程所有权和提交接口；
- `completion_queue` 本质是手动 pump 的单线程 context，却没有实现 executor 抽象；
- 调用 assets/resources 异步入口时需要同时传入后台 executor 和 completion queue；
- 当前 request 只支持轮询，无法自然接入 `co_await`、超时和结构化取消；
- 自行继续扩展协程调度、timer、I/O 和 cancellation 会逐步重复 Asio 已验证的基础设施。

Asio 已提供 `io_context`、`thread_pool`、轻量 executor、`post/dispatch/defer`、`awaitable<T>`、
`co_spawn()` 和 cancellation。CAORS 不复制完整模型，只在其上建立符合自身错误、内存和生命周期约束的
薄层。

参考资料：

- [Asio C++20 Coroutines Support](https://www.boost.org/doc/libs/latest/doc/html/boost_asio/overview/composition/cpp20_coroutines.html)；
- [Asio io_context::run](https://www.boost.org/doc/libs/latest/doc/html/boost_asio/reference/io_context/run.html)；
- [Asio thread_pool](https://www.boost.org/doc/libs/latest/doc/html/boost_asio/reference/thread_pool.html)；
- [Asio Using, Building and Configuring](https://www.boost.org/doc/libs/latest/doc/html/boost_asio/using.html)。

## 核心决策

### 1. 使用 standalone Asio

首选 standalone Asio，而不是为了 tasks 单独引入完整 Boost：

- 固定并记录 Asio 版本和许可证；
- 建立唯一 CMake target，例如 `caors_asio_config`；
- 统一 `ASIO_STANDALONE`、`ASIO_NO_DEPRECATED`、C++20 和线程配置；
- 不手工定义 Asio 的自动特性检测宏；
- 禁止各子模块自行引入另一版本或重新定义配置宏。

依赖落地前先比较两种方式：

1. 仓库固定版本依赖，符合当前 Catch2/ctrack 的离线可构建方式；
2. `find_package()` 加可选获取逻辑，减小仓库体积但增加首次配置和安装包消费复杂度。

阶段 0 必须通过离线构建、安装包消费和许可证审查后再确定。核心公共头不直接包含 Asio 时，依赖优先
作为 `caors` 的私有实现；只有显式包含 coroutine 适配头的使用方才需要 Asio 头。

### 2. 核心公共 API 不直接暴露 Asio 类型

默认 `caors/tasks.h` 提供 CAORS 包装类型，不出现：

- `asio::io_context`；
- `asio::thread_pool`；
- `asio::any_io_executor`；
- `asio::awaitable<T>`；
- Asio completion token。

可选聚合头 `caors/tasks/asio.h` 才提供 Asio native/coroutine 适配。这样普通同步用户和只使用
fire-and-forget 调度的用户不必把 Asio 类型扩散到自己的接口中。

### 3. 统一 execution context 与 executor

目标核心 API：

```cpp
namespace caors::tasks {

class executor final {
public:
  executor() noexcept = default;

  [[nodiscard]] error post(task next) const;
  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept;
};

class execution_context final {
public:
  explicit execution_context(memory_resource* resource = nullptr);

  [[nodiscard]] executor get_executor() const noexcept;
  [[nodiscard]] std::size_t run();
  [[nodiscard]] std::size_t run_one();
  [[nodiscard]] std::size_t poll();
  [[nodiscard]] std::size_t poll_one();
  void stop() noexcept;
  void restart();
  [[nodiscard]] bool stopped() const noexcept;
};

class thread_pool final {
public:
  explicit thread_pool(std::size_t thread_count = default_thread_count(),
                       memory_resource* resource = nullptr);

  [[nodiscard]] executor get_executor() const noexcept;
  void shutdown(shutdown_mode mode = shutdown_mode::drain) noexcept;
  [[nodiscard]] std::size_t thread_count() const noexcept;
};

[[nodiscard]] executor inline_executor() noexcept;

} // namespace caors::tasks
```

语义：

- `execution_context` 拥有手动运行的任务队列，主线程每帧调用 `poll()`；
- `thread_pool` 拥有后台 worker 和底层 Asio context；
- `executor` 是可复制的轻量提交句柄，不拥有线程；
- `inline_executor()` 立即在调用线程执行；
- 普通提交统一命名为 `post()`，首批不公开 `dispatch/defer`，避免调用点误判内联和重入语义；
- `post()` 保证不在调用栈内执行，`inline_executor()` 是唯一明确允许立即执行的策略。

### 4. executor 生命周期

executor 不能保存裸 `execution_context*` 后任其悬空。目标状态包含共享生命周期控制：

- context/pool 活跃时，`post()` 转发给底层 executor；
- `stop()`/`shutdown()` 后拒绝新任务并返回 `errc::frozen`；
- context wrapper 析构后，仍存活的 executor 不访问已销毁内存；
- executor 不因为自身存活而隐式保持 worker 线程运行；
- pool/context 销毁前明确关闭状态，之后 executor 只能观察关闭并拒绝提交。

阶段 1 原型需要比较 `shared_ptr<state>` 与小型 type-erased control block 的布局、分配和提交成本。不得使用
仅靠调用方自律的悬空裸指针方案。

### 5. completion queue 迁移

`completion_queue` 收敛为手动 `execution_context`：

```cpp
tasks::execution_context main_context;
tasks::executor main_executor = main_context.get_executor();

worker_executor.post(task{[main_executor] {
  // 后台工作。
  (void)main_executor.post(task{[] {
    // 主线程提交。
  }});
}});

(void)main_context.poll();
```

迁移必须保留当前 `drain()` 的关键边界：本轮 pump 开始后新提交的任务默认留到下一轮，避免同一帧无界
递归处理。Asio 原生 `poll()` 是否满足该快照语义需要专项测试；若不满足，CAORS context 在外层维护
active/pending 两批队列，不能静默改变现有行为。

### 6. task 与 request 保留

`task` 仍是 move-only、可返回 `void` 或 `caors::error` 的 callable 包装。首批只把它作为 Asio handler
提交，不立即改成 coroutine result 类型。

后续在基准确认必要时为小 lambda 增加 SBO，减少当前每个 task 的独立分配。SBO 与 Asio 接入分开提交，
避免调度语义迁移和表示优化互相干扰。

`request` 仍作为可选共享状态，服务：

- 非 coroutine 调用方轮询；
- load/reload 去重；
- 状态、错误和协作式取消；
- 多个等待者共享同一底层操作。

普通 fire-and-forget 提交不创建 request。首批保持 `submit_request(executor, task)`，只是 executor 改为
轻量值类型。

### 7. coroutine 作为可选适配层

首批 coroutine API 放在 `caors/tasks/asio.h`：

```cpp
namespace caors::tasks {

template <typename T>
using awaitable = asio::awaitable<T>;

[[nodiscard]] auto schedule(executor target);

[[nodiscard]] awaitable<error> async_wait(request operation);

} // namespace caors::tasks
```

最终签名以原型验证为准。约束：

- `co_await schedule(executor)` 只负责切换执行目标；
- coroutine 完成值继续使用 `caors::result<T>` / `caors::error`；
- 默认不依赖异常表达业务失败；
- `co_spawn()` 的顶层 completion 必须显式处理异常或转换为 `caors::error`；
- 一个 `asio::awaitable<T>` 不能替代可被多个调用方等待的共享 request state；
- 不在核心 `tasks.h` 中定义一套与 Asio 竞争的完整 `task<T>` coroutine runtime。

### 8. 错误与异常语义

CAORS 继续以 `caors::error` 为公共失败模型：

- executor 为空：`core / invalid_argument`；
- context 已关闭：`core / frozen`；
- 请求取消：`core / cancelled`；
- handler 逃逸异常：在允许异常的构建中由顶层边界转换为 `core / backend_failure`；
- 禁用异常构建不能依赖 Asio 默认抛异常的 `use_awaitable` 路径。

阶段 0 必须验证 CAORS 当前异常开关与 standalone Asio 的组合。若无异常 coroutine 需要额外的 Asio
throw hook 或不稳定配置，不得用宏掩盖问题；可以先让 coroutine 适配在无异常构建中不可用，同时保持
callback/context 核心可构建，并在文档中明确限制。

### 9. 内存资源

需要区分三类分配：

- context/pool 长期状态：使用构造时捕获的 `memory_resource`；
- CAORS `task` 和 `request` 状态：继续使用调用方资源；
- Asio 内部 operation/coroutine frame：通过 associated allocator 适配 CAORS resource，或在暂时无法
  全面控制时记录为 Asio/system 域。

不得宣称所有 Asio 内部分配已经进入 CAORS resource，除非 tracking 测试能够证明。coroutine frame 和
挂起 operation 不属于同步 TLS scratch，必须使用覆盖整个异步操作的长期资源。

### 10. 线程与停止语义

统一规则：

- `execution_context::poll()` 不阻塞；
- `run()` 可以阻塞，直到没有工作或 context 被停止；
- `thread_pool::~thread_pool()` 默认 drain，保持当前行为；
- `shutdown(cancel)` 拒绝新任务并丢弃尚未开始的 CAORS 队列任务，但不强制中断正在运行的 handler；
- `stop()` 与 drain 不是同义词，API 注释和测试必须分别覆盖；
- 同一 context 可由多个线程运行，但主线程 commit context 推荐只由主线程 poll；
- handler 内再次 post、嵌套 poll/run 和从 worker 自身 shutdown 的行为必须定义，不能依赖偶然实现。

## 与现有 API 的映射

| 当前 API | 目标 API | 迁移方式 |
|---|---|---|
| `executor::submit(task)` | `executor::post(task)` | 先提供弃用转发，再移除旧虚基类 |
| `inline_executor` 类 | `inline_executor()` 轻量 executor | 保留短期兼容包装 |
| `thread_pool_executor` | `thread_pool` + `get_executor()` | 保留短期兼容包装或 beta 阶段直接迁移 |
| `completion_queue::push()` | 主 `execution_context` 的 executor `post()` | `completion_queue` 暂时包装 context |
| `completion_queue::drain()` | `execution_context::poll()` | 保留快照批次语义 |
| `submit_request(executor&, task)` | `submit_request(executor, task)` | request 状态模型保留 |

项目仍处于 beta。实施前在阶段 0 决定是保留一个迁移周期，还是一次性破坏旧 API；不得同时长期维护两套
调度实现。

## 文件组织建议

```text
include/caors/tasks/
  execution_context.h
  executor.h
  thread_pool.h
  task.h
  request.h
  asio.h                 # 可选 native/coroutine 适配

src/caors/tasks/
  execution_context.cpp
  executor.cpp
  thread_pool.cpp
  asio.cpp

3rd/
  asio-<固定版本>/       # 若阶段 0 选择仓库固定依赖
```

Asio 的 concrete 类型放在 `.cpp` 私有实现或私有头中。核心公共包装类型避免因为 Asio 小版本更新而改变
布局。安装包若安装 `caors/tasks/asio.h`，必须同时正确导出或查找 Asio 依赖。

## 分阶段实施

### 阶段 0：依赖与语义原型

- [x] 固定 standalone Asio `asio-1.38.2`，使用 Boost Software License 1.0；
- [x] 选择仓库固定依赖，避免 `FetchContent` 首次配置联网；Asio 在核心库中保持私有；
- [x] 建立唯一 `caors_asio_config` target，集中声明 `ASIO_STANDALONE`、
  `ASIO_NO_DEPRECATED` 和 C++20；
- [x] 验证 MSVC/Clang/GCC、Static/Shared 和 C++20 coroutine 检测；无异常模式通过
  Windows Clang/MSVC 与 Linux Clang/GCC 的公共头编译探针验证，不等同于完整无异常运行矩阵；
- [x] 验证安装包 consumer 不使用 coroutine 时不必包含 Asio 公共头；
- [x] executor 使用一个指针大小的侵入式引用计数 control block；句柄可复制，context
  销毁后句柄仍可安全判定失效，不采用公共虚基类或 `std::shared_ptr` 布局；
- [x] `execution_context::poll()` 保留调用开始时的快照批次语义；Asio 原生 `poll()`
  可能继续执行 handler 内重入投递的工作，因此实现不能直接无界转发；
- [x] 项目仍处 beta，本分支直接迁移到轻量 executor；不长期维护两套调度实现。

当前验证记录：

- Windows Clang Static Debug：基础调度探针通过；
- Windows MSVC Static Debug：基础调度探针通过；
- Windows Clang Shared Debug：基础调度探针通过；
- Windows Clang Shared Debug 安装包：原始前缀与迁移前缀 consumer 均通过，安装导出不依赖 Asio；
- Windows Clang `-fno-exceptions` 已验证核心库、`asio.h` 独立包含以及当前 CAORS 原生 coroutine
  适配可以编译；阶段 6 已补 Windows MSVC 和 Linux Clang/GCC 的 tasks/resources 无异常公共头
  探针，完整无异常运行矩阵仍属于后续扩展验证。

control block 由 context 指定的长期 `memory_resource` 分配，内部保存引用计数、提交状态和私有 Asio
backend。`execution_context` 销毁时先关闭提交并销毁 backend；外部仍存活的 executor 只保留 control
block，后续 `post()` 返回 `errc::frozen`。投递到 backend 的 handler 在完成或被销毁前持有内部引用，
避免任务与 control block 的生命周期竞争。context 析构不与其自身的 `run/poll` 成员调用并发，这是
对象生命周期层面的前置条件，而不是 executor 句柄的限制。

### 阶段 1：execution context 与 executor

- [x] 实现 `execution_context` 私有 Asio backend；
- [x] 实现一个指针大小、可复制且可安全失效的轻量 `executor`；
- [x] 实现 `post()`、`run/run_one`、快照 `poll/poll_one`、`stop/restart`；
- [x] 实现明确返回轻量句柄的 `inline_executor()`；
- [x] 覆盖空 executor、关闭后提交、嵌套提交和 context 析构后 executor 行为；
- [x] 增加 control block 长期内存资源与无泄漏测试；Asio operation allocator 仍属于后续工作。

生产实现与 `request/resources/assets` 已统一使用轻量 executor 的 `post()`，不再依赖 executor
虚基类或线程池上的 `submit()` 转发。迁移期 `thread_pool_executor` 别名和 `inline_executor` 包装类
已经删除。

### 阶段 2：线程池与 completion queue 迁移

- [x] 实现 `thread_pool` 并保持默认 drain 析构；
- [x] 定义并测试 `shutdown(drain/cancel)`；
- [x] 让 `completion_queue` 临时包装 `execution_context`；
- [x] 迁移 tests/examples 到统一 `post()` 接口；
- [x] 验证主线程每帧 `poll()` 和本轮新任务延后语义；
- [x] 移除旧独立队列/worker 实现，避免两套调度状态长期并存。

### 阶段 3：task/request 适配

- [x] 通过内部持有 control block 引用的 move-only handler 安全提交 `task`；
- [x] 迁移 `submit_request()`、resources 与 assets 异步入口到按值轻量 executor 和 `post()`；
- [x] 保持 queued/running/completed/failed/cancelled 行为；
- [x] 覆盖多个共享观察者、提交失败、handler 失败和协作取消；
- [x] 完成 callable 分配基线评估；当前保留每 callable 一次显式资源分配，不引入 task SBO。
  SBO 容量与收益仍需后续独立对照实验，不作为本次迁移的验收条件。

### 阶段 4：C++20 coroutine 适配

- [x] 增加可选 `caors/tasks/asio.h`；
- [x] 实现并测试 `schedule(executor)` awaiter；
- [x] 提供顶层 `spawn()` 和 error-only `async_operation`，把逃逸异常转换为 CAORS 错误；
- [x] 为 request 增加支持多个共享等待者的非阻塞 `async_wait()`；
- [x] 覆盖 executor 切换、取消、context stop、异常与 frame 销毁；
- [x] 明确无异常构建的支持级别：当前适配不依赖 Asio `awaitable`，业务失败通过 `caors::error`
  传播；关闭异常后不提供逃逸异常转换，但 `schedule/spawn/async_wait` 仍可编译；
- [x] 验证 coroutine frame 不使用同步 TLS scratch；frame 仍由编译器默认长期分配器管理，
  `schedule` 和 `async_wait` 的辅助状态分别使用 executor 与 request 的长期资源。

### 阶段 5：调用方迁移和正式文档

- [x] 将 resources 和 assets 异步入口统一为 background/completion 两个 executor；
- [x] 更新 `docs/reference/tasks.md`；coroutine 适配保持显式包含，不加入聚合头；
- [x] 更新独立 consumer，覆盖基础 API、核心 tasks 和显式 coroutine 适配；安装导出继续保持 Asio 私有；
- [x] 删除过期 `thread_pool_executor`、`inline_executor` 包装类和 `completion_queue` 兼容层；
- [x] 为后续 assets/resources coroutine 重构建立独立计划和基线，见
  `resource-coroutine-pipeline.md`。

### 阶段 6：性能与全矩阵验证

- [x] 基准比较旧/新 inline、主线程 post/drain 和 thread pool 往返；线程池场景包含屏障，
  不将其解释为纯提交延迟或饱和吞吐；
- [x] 记录 task/request/context 的显式资源分配、布局、采样吞吐和 p50/p95，结果见
  `benchmarks/results/2026-10-19-windows-clang-task-scheduler.md`；协程等待数据见
  `benchmarks/results/2026-08-07-windows-clang-task-coroutines.md`；
- [x] Windows Clang/MSVC Static/Shared Debug/Release；2026-10-19 八组全量复验通过，
  Static 各 86/86、Shared 各 88/88；MSVC Static Debug 使用干净构建；
- [x] Linux Clang/GCC Static、Clang Shared、ASan；
- [x] 在 Ubuntu 24.04 CI 补充 Clang Shared UBSan，并设置失败即停止；固定业务环境复验延期；
- [x] 安装包 consumer 覆盖基础 API、核心 tasks、tasks coroutine 和 resources coroutine，
  Windows Static Release / Shared Debug 原始与迁移前缀四类 consumer 全部通过。

### 2026-10-19 收尾验收

- [Linux 完整 CI](https://github.com/synchronized/caors-core/actions/runs/39194854028) 在
  `73c0290` 全部通过：Clang/GCC Static Debug/Release、Clang Shared Debug/Release、Shared ASan
  和 Shared UBSan；Static/Shared Release 安装包及迁移前缀验证也通过。
- Windows 八组全量测试、Static/Shared 四类 installed consumer 和 Clang/MSVC 无异常公共头探针通过。
  无异常探针只验证公共适配头的编译能力，不声明完整无异常运行矩阵已完成。
- 新旧调度器的 Windows 同机对比已记录。当前空任务 post/drain 和线程池往返较旧实现慢，
  本次迁移不承诺性能提升；应复用长期 context/pool，后续根据真实负载评估批量投递和 allocator。
- 固定 Linux 环境的正式性能复验、真实业务负载和完整无异常运行矩阵继续作为延期扩展验证，
  不以 CI runner 的耗时替代正式性能数据，不阻塞本次迁移验收。

## 验收标准

- 普通使用者只需要一个 `execution_context` 或 `thread_pool`，不再手工拼 `executor + completion_queue`；
- 所有提交统一通过轻量 `executor::post()`；
- 主线程 poll 与后台 pool 使用同一种调度抽象；
- context 销毁后 executor 不悬空，关闭后提交有确定错误；
- task/request 的当前状态与错误语义保持可验证；
- coroutine 是可选层，不强迫全部 CAORS 使用者暴露 Asio 类型；
- CAORS 公共错误仍使用 `caors::error/result`；
- 异步长期分配不进入同步 TLS scratch；
- 不长期保留旧队列与新 Asio backend 两套实现；
- 构建、安装包和验证矩阵与现有 CAORS 支持范围一致。

## 风险

- **依赖扩散**：可选适配头若进入常用聚合头，会让全部 consumer 承担 Asio 编译成本；
- **语义变化**：Asio `poll()`、`stop()` 和 work tracking 与当前 snapshot drain 不完全等价；
- **生命周期**：executor、request、coroutine frame 可能比 context/pool 活得更久；
- **异常模式**：Asio coroutine 默认路径与 CAORS 无异常构建不完全一致；
- **内存统计**：Asio 内部 operation 分配可能暂时无法全部归入 CAORS resource；
- **双调度器**：迁移期若旧 worker/queue 长期保留，会增加线程、状态和死锁风险；
- **错误吞没**：fire-and-forget handler 的异常和 `caors::error` 必须有明确观察入口；
- **安装导出**：可选 Asio 公共头需要正确处理安装包依赖和版本；
- **过度设计**：复制 Asio 的 executor property/completion token 会让 CAORS 包装层失去价值。

## 后续方向

本计划完成后再单独讨论：

- `asset_manager::async_load/reload/preload/save()`；
- source/provider 的真正异步 I/O 与同步线程池 fallback；
- 同一 GUID in-flight request 合并和多等待者；
- 有限并发依赖预加载；
- timer、重试、超时和文件监听；
- 远程 file server 与网络 source。

这些方向不能作为本计划阶段 0 到阶段 4 的隐式范围扩张。
