<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Tasks

本文档说明 CAORS 任务调度模块的当前边界。内置后端使用 C++20 标准库线程、互斥锁和条件变量，
公共接口提供 move-only `task`、轻量 `executor`、同步 `inline_executor()` 工厂、单线程
`execution_context`、固定线程数 `thread_pool` 和轻量 `request` 状态句柄。

## 定位与适用范围

`executor` 是任务提交的公共抽象，与 `task`、request 和协程适配共同表达任务提交和完成。
`executor` 不绑定内置线程池；资源异步流程也可使用宿主提供的调度上下文。公开接口仍遵循项目的
[alpha 稳定性政策](api-stability.md)，这里的定位不构成 API 或 ABI 冻结承诺。

内置 `thread_pool` 与 `execution_context` 定位为基础调度后端，提供一般后台工作与调用线程驱动的完成队列。
当前尚未完成新后端的真实业务负载与长期运行验证；实际已执行的功能回归、并发与关闭测试见
[实现验收记录](../records/2026-10-19-v0.2.0-host-integration.md#当前实现验收结论)。
测试通过说明这些场景已有覆盖，不代表吞吐、尾延迟、容量或长期稳定性已经满足任意宿主的上线要求。
旧 Asio 基线也不能作为当前标准库后端的性能结论。

| 使用场景 | 选择与接入方式 |
| --- | --- |
| 一般后台计算或同步资源读取，任务数量与耗时可控 | 使用内置 `thread_pool`，按负载选择 worker 数量 |
| 主线程或资源 cache 所属线程执行完成回调 | 使用 `execution_context`，由所属线程在适当时机主动 `drain()` |
| 宿主已有线程池、作业系统或完成队列 | 优先通过[外部 executor](#外部-executor)适配，保持宿主的调度和关闭策略 |
| 需要容量限制、背压、优先级或延迟约束 | 由宿主实现相应调度策略并验证，再通过外部 executor 接入 |

正式业务采用内置后端时，应使用自己的任务规模、阻塞比例和回调节奏验证队列积压、内存占用、完成延迟与关闭耗时。
内置后端作为现有公共能力维护，其正式业务适用性应由宿主实测确定。

### 内置后端的调度限制

- 队列没有可配置的容量上限或背压机制。持续投递快于消费时，等待任务及其捕获对象会持续占用内存，
  最终可能分配失败；调用方需要控制在途任务量。
- `thread_pool` 使用固定 worker 数量。长时间阻塞的任务会占用 worker，增加其后任务的等待时间；
  需要隔离不同负载或控制优先级时，应使用宿主调度策略。
- `execution_context` 不创建执行线程。所属线程未及时运行队列时，完成回调会积压；快照批次也不提供每帧耗时预算。
- 不提供 work stealing、优先级、定时器或平台异步 I/O；也不承诺实时调度或确定的尾延迟。
- 关闭、任务丢弃与生命周期仍须满足下文各接口契约；换用外部后端不会自动取消这些约束。

## 设计目标

当前已覆盖：

- `tasks::task`：move-only 任务包装器，可保存 move-only lambda，任务 callable 可以返回 `void` 或
  `caors::error`。
- `tasks::executor`：一个指针大小的可复制调度句柄，通过 `post(task)` 提交任务。
- `tasks::inline_executor()`：返回提交后在调用线程立即执行的轻量 executor，用于测试、确定性执行和
  同步 fallback。
- `tasks::execution_context`：由调用方驱动 `run()` / `poll()` / `drain()` 的单线程执行上下文。
- `tasks::thread_pool`：固定线程数后台执行上下文，析构默认 drain 已入队任务。
- `tasks::request`：提交任务后的共享状态句柄，覆盖 queued/running/completed/failed/cancelled 和错误查询。
- `task`、control block、排队节点和 worker 数组使用指定的长期 `memory_resource`；标准库线程及同步原语的系统内部资源不计入该资源。

当前不会包含：

- work stealing、fiber、任务图或平台异步 IO。
- 优先级、阻塞等待、取消传播、进度和任务返回值保存。
- 任务异常诊断保存。
- 聚合 `execution_context::drain()` 中每个任务返回的错误。

是否扩展这些能力由具体需求和验证结果决定，不在此承诺实现时间。

## 第一阶段边界

任务模块当前只提供“能提交、能执行、能回主线程、能查询请求状态”的基础能力。它不是通用游戏引擎
scheduler，也不是完整作业系统：

- 不做 work stealing。当前没有细粒度任务图和大量短任务场景，固定线程池更容易验证生命周期和停止边界。
- 不做 fiber。C++20 coroutine 适配由显式头 `caors/tasks/coroutine.h` 提供。
- 不做任务图。资源依赖闭包和预加载顺序目前由资源数据库管理，任务层只负责执行单个 callable。
- 不做平台异步 IO。provider/source 仍以同步读取为主；后续如果加入文件包、远程资源或平台 IO，再在资源层评估接入方式。
- 不做强制取消。`request::cancel()` 只改变请求状态，不中断正在运行的线程或 callable。

后续只有在资源导入、热重载、编辑器工具或大量后台作业暴露出明确需求后，才会继续评估优先级、等待 API、
进度聚合、任务图或更复杂的线程池策略。

## 命名空间和头文件

任务模块定义位于 `caors::tasks` 命名空间：

- `tasks::task`
- `tasks::executor`
- `tasks::external_executor` / `tasks::external_executor_context`
- `tasks::inline_executor()`
- `tasks::execution_context`
- `tasks::thread_pool`
- `tasks::shutdown_mode`
- `tasks::request`
- `tasks::request_status`
- `tasks::submit_request()`

任务模块不再向根命名空间导出低层别名。任务、执行器、执行环境和请求状态统一通过
`caors::tasks::...` 使用，避免根命名空间承担运行期调度组合件。

公共头文件：

- `caors/tasks/task.h`
- `caors/tasks/executor.h`
- `caors/tasks/external_executor.h`
- `caors/tasks/inline_executor.h`
- `caors/tasks/execution_context.h`
- `caors/tasks/thread_pool.h`
- `caors/tasks/request.h`
- `caors/tasks.h`

## task

`task` 是 move-only 包装器，内部通过类型擦除保存 callable。callable 可以返回 `void`：

```cpp
caors::tasks::task current{[] {
  // 执行同步任务。
}};

caors::error status = current.run();
```

也可以返回 `caors::error`：

```cpp
caors::tasks::task current{[] {
  return caors::error{};
}};
```

需要使用指定内存资源时，把 resource 放在第一个参数：

```cpp
caors::tasks::task current{resource, [] {
  // callable 存储使用 resource 分配。
}};
```

需要注意：

- 空任务调用 `run()` 会返回 `errc::invalid_argument`。
- `task` 不可拷贝，只能移动。
- `task` 不捕获 callable 抛出的异常；需要异常隔离时在 callable 内部处理，或由后续具体 executor 策略处理。
- `task` 可以重复执行，但 callable 是否适合重复执行由调用方保证。

## executor

`executor` 是不拥有执行上下文的轻量句柄。复制句柄不会复制线程或队列；执行上下文销毁后，仍存活的
句柄会安全失效，后续 `post()` 返回 `errc::frozen`：

```cpp
caors::tasks::execution_context context;
caors::tasks::executor executor = context.get_executor();

executor.post(caors::tasks::task{[] {
  // 由 context.run() 或 context.poll() 执行。
}});

context.poll();
```

`inline_executor()` 返回确定性同步执行句柄：

```cpp
caors::tasks::executor executor = caors::tasks::inline_executor();

auto status = executor.get_executor().post(caors::tasks::task{[] {
  // 在 post 调用线程立即执行。
}});
```

## request

`request` 是任务提交后的共享状态句柄。它不保存任务返回值，只保存状态和错误；多个 `request` 副本共享
同一状态，可跨线程查询。取消 running 请求不会中断 callable，且 callable 随后的完成或失败不会覆盖
cancelled 状态：

```cpp
caors::tasks::thread_pool pool{2};
caors::tasks::executor executor = pool.get_executor();

caors::tasks::request request =
    caors::tasks::submit_request(executor, caors::tasks::task{[] {
      return caors::error{};
    }});

if (request.done()) {
  caors::error status = request.result();
}
```

状态语义：

- `queued`：请求已创建，任务尚未开始执行。
- `running`：任务正在执行。
- `completed`：任务返回成功。
- `failed`：任务返回错误，或 executor 拒绝提交。
- `cancelled`：请求已取消。

常用入口：

- `submit_request(executor, task, resource)`：提交带状态的任务请求。同步 executor 可能在返回前完成请求。
- `request::make()` / `request::make_completed()` / `request::make_failed()`：创建手动状态句柄，供上层系统组合。
- `status()` / `done()` / `completed()` / `failed()` / `cancelled()`：非阻塞状态查询。
- `result()`：completed 返回成功；failed 返回失败错误；queued/running 返回 `core / pending`；cancelled 返回
  `core / cancelled`。
- `cancel()`：queued/running 状态可取消；已经开始执行的任务不会被强制中断，但后续完成不会覆盖 cancelled。

需要注意：

- `request` 不提供阻塞等待。等待、轮询组、超时和协程适配后续再补。
- `request` 不保存任务返回值。需要值结果时应由上层 request 类型或业务状态保存。
- 当前只提供“协作式取消状态”，不会把取消信号传入 callable，也不会中断正在运行的线程。
- 当启用异常时，`submit_request()` 包装层会把逃逸异常转换为 `core / backend_failure`。

## thread_pool

`thread_pool` 拥有固定数量的后台 worker，并通过轻量 executor 暴露提交入口：

```cpp
caors::tasks::thread_pool pool{2};
caors::tasks::executor executor = pool.get_executor();

executor.post(caors::tasks::task{[] {
  // 在后台 worker 中执行。
}});

pool.shutdown(caors::tasks::shutdown_mode::drain);
```

常用入口：

- `thread_pool(thread_count, resource)`：创建固定数量 worker。`thread_count == 0` 时使用默认线程数。
- `get_executor()`：取得可复制的任务提交句柄。
- `executor::post(task)`：提交任务。空任务返回 `errc::invalid_argument`，关闭后返回 `errc::frozen`。
- `shutdown(shutdown_mode::drain)`：停止接收新任务，执行完已经入队的任务后退出 worker。
- `shutdown(shutdown_mode::cancel)`：停止接收新任务，丢弃尚未开始执行的任务，当前正在执行的任务继续完成。
- `pending()`：查询尚未开始执行的任务数量。
- `thread_count()`：查询 worker 数量。

需要注意：

- 析构会自动执行 `shutdown(shutdown_mode::drain)`。
- 不得从该线程池正在执行的任务内部调用 `shutdown()` 或销毁线程池；关闭操作需要等待 worker 退出。
- `thread_pool` 自身不保存每个任务的返回错误；需要状态时使用 `submit_request()` 包装任务。
- 当启用异常时，worker 会捕获逃逸异常以避免后台线程终止进程，但当前不会保存异常信息。
- `shutdown(cancel)` 不是强制中断；已经开始执行的任务仍会跑完。
- 资源 cache、manifest、provider 和对象状态仍不应在后台 worker 中被隐式修改。

## execution_context 手动 pump

后台线程通过轻量 executor 投递，拥有线程通过 `drain()` 执行完成回调：

```cpp
caors::tasks::execution_context completions;
caors::tasks::executor completion_executor = completions.get_executor();

completion_executor.post(caors::tasks::task{[] {
  // 在主线程 drain 时执行。
}});

std::size_t count = completions.drain();
```

需要注意：

- 后台线程可以调用 executor 的 `post()`；`drain()` 和 `clear()` 由拥有该执行环境的线程调用，且不得彼此并发。
- `drain()` 开始后新增的任务会留到下一次 `drain()`。
- `clear()` 丢弃尚未开始的任务，并保留后续继续投递和执行的能力。
- `drain()` 会在 context 因上一轮耗尽而 stopped 时自动 restart；`poll()` 保留显式 stop 边界。
- `drain()` 返回执行数量，不聚合任务返回的错误。需要错误传播时，应把错误写入调用方自己的 request 状态。

## coroutine 调度适配

显式包含 `caors/tasks/coroutine.h` 后，可以用 `schedule(executor)` 将当前 C++20 coroutine 切换到指定
执行目标：

```cpp
caors::error status = co_await caors::tasks::schedule(executor);
if (status) {
  co_return status;
}
```

- `schedule()` 只切换执行目标，不定义新的 coroutine 返回类型，也不替代共享 `request` 状态。
- executor 拒绝提交或在执行前销毁时，coroutine 会恢复并获得相应错误；销毁仍在等待的 coroutine frame
  后，排队 handler 不会再次恢复该 frame。
- 调度共享状态使用目标 executor 的长期 `memory_resource`，不使用 TLS scratch；任务完成或等待方
  提前销毁后会归还该资源。

需要让运行时管理 coroutine frame 时，使用 error-only `async_operation` 和 `spawn()`：

```cpp
caors::tasks::async_operation load(caors::tasks::executor worker) {
  if (caors::error status = co_await caors::tasks::schedule(worker)) {
    co_return status;
  }
  co_return caors::error{};
}

caors::tasks::request request = caors::tasks::spawn(main_executor, load(worker_executor));
```

- `spawn()` 返回现有共享 `request`，并维护 queued、running、completed、failed 和 cancelled 状态。
- 提交失败或 executor 在执行前销毁时，frame 会被释放并保存对应错误。
- 允许异常的构建会把未处理异常转换为 `core / backend_failure`。
- 取消 queued 请求会在启动点销毁 frame；取消 running 请求仍是协作式取消，不会强制销毁正在运行或
  挂起的 coroutine。
- coroutine frame 在调用 `spawn()` 前已经分配，当前由编译器默认分配器管理；自定义长期资源将在
  allocator 适配批次处理。
- 关闭异常时，当前 CAORS 原生 coroutine 适配仍通过 `caors::error` 表达业务失败；此时不存在逃逸
  异常转换路径。该模式已在 Windows Clang `-fno-exceptions` 下完成编译探针，其他编译器组合仍需
  在完整构建矩阵中验证。

多个 coroutine 可以非阻塞等待同一个共享 request：

```cpp
caors::error status = co_await caors::tasks::async_wait(request);
```

- 请求进入 completed、failed 或 cancelled 时恢复全部等待者，并返回 `request::result()`。
- 已经终止的请求不会挂起 coroutine。
- 等待者在释放 request 状态锁后恢复，因此恢复的 coroutine 可以安全查询或再次组合 request。
- `async_wait()` 本身不改变取消传播语义；取消等待方的 running request 不会自动取消被等待的 request。
- 每个等待操作的辅助状态使用被等待 request 的长期 `memory_resource`，终止或销毁等待方后释放。

## 与资源系统的关系

资源系统同时提供同步 API 和基于 executor 的异步请求。`tasks` 模块为异步资源流程提供底层拼装件：

- 后台阶段：通过 executor 读取 provider、调用 loader、构造资源对象。
- 主线程阶段：通过轻量 completion executor 把成功结果插入 cache、替换资源、更新 generation，并发送
  reload 通知；该 executor 通常来自主线程 `execution_context`。
- 请求状态：资源层使用 `resources::load_request<T>` 保存 queued/loading/loaded/failed/cancelled、错误和结果 handle。

resources 和 assets 异步入口都接收后台 executor 与 completion executor。需要帧循环手动 pump 时，
可通过主线程 `execution_context::get_executor()` 取得轻量句柄并作为 completion executor；等待资源
请求的 coroutine 适配位于显式可选的
`caors/resources/coroutine.h`。

第一阶段不会让资源 cache、manifest、provider 或对象系统在后台线程被隐式修改。

## 外部 executor

宿主已有调度系统时，优先复用其工作队列与所属线程完成队列，避免额外维护一套独立线程池。
`tasks::external_executor` 将共享拥有的 `external_executor_context` 适配为普通 executor。
宿主实现 `post(task)` 和 `close()`，接口不绑定具体后端；可分别为宿主线程池和所属线程完成队列创建适配器。
`post` 接管任务，成功后必须执行或销毁，拒绝后不得执行。`close` 必须封闭队列并排空/销毁已接受任务，
与投递并发时仍保证协议；不在自己的工作任务中阻塞等待自身关闭。允许同步 inline 执行，需支持重入。
适配器不持锁调用宿主，投递异常在允许异常的构建中转换为 `backend_failure`。

所有者关闭后，晚存活的 executor 返回 `frozen`。上下文由共享所有权保活，资源和模块必须覆盖其全部寿命。
`submit_request()` 或资源加载的排队任务被丢弃时会进入失败终态，错误为 `cancelled`；
这与显式取消后的 cancelled 状态有区别。拒绝投递保留原始错误，终态只通知一次。
资源 cache 的提交线程约束保持有效；宿主应将 completion executor 绑定到 cache 所属线程。

完整示例见 [external_executor.cpp](../../examples/tasks/external_executor.cpp)。
