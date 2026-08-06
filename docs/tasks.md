<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Tasks

本文档说明 CAORS 任务调度模块的当前边界。任务系统以仓库固定的 standalone Asio 作为私有后端，
公共接口提供 move-only `task`、轻量 `executor`、同步 `inline_executor`、单线程
`execution_context`、固定线程数 `thread_pool`、主线程 `completion_queue` 和轻量 `request` 状态句柄。

## 设计目标

当前已覆盖：

- `tasks::task`：move-only 任务包装器，可保存 move-only lambda，任务 callable 可以返回 `void` 或
  `caors::error`。
- `tasks::executor`：一个指针大小的可复制调度句柄，通过 `post(task)` 提交任务。
- `tasks::inline_executor`：提交后在调用线程立即执行，用于测试、确定性执行和同步 fallback。
- `tasks::execution_context`：由调用方驱动 `run()` / `poll()` 的单线程执行上下文。
- `tasks::thread_pool`：固定线程数后台执行上下文，析构默认 drain 已入队任务。
- `tasks::completion_queue`：线程安全完成回调队列，适合后台任务完成后把 commit 阶段交给主线程。
- `tasks::request`：提交任务后的共享状态句柄，覆盖 queued/running/completed/failed/cancelled 和错误查询。
- `task` 和执行环境可传入 `memory_resource`，长期 control block 使用对应资源；Asio 内部 operation
  allocator 的接入仍属于后续优化。

当前不会包含：

- work stealing、fiber、任务图或平台异步 IO。
- 优先级、阻塞等待、取消传播、进度和任务返回值保存。
- 任务异常诊断保存。
- 聚合 `completion_queue::drain()` 中每个任务返回的错误。

这些能力会在资源异步加载和资源热更新流程落地后继续扩展。

## 第一阶段边界

任务模块当前只提供“能提交、能执行、能回主线程、能查询请求状态”的基础能力。它不是通用游戏引擎
scheduler，也不是完整作业系统：

- 不做 work stealing。当前没有细粒度任务图和大量短任务场景，固定线程池更容易验证生命周期和停止边界。
- 不做 fiber。C++20 coroutine 适配将在统一 executor 语义稳定后单独提供。
- 不做任务图。资源依赖闭包和预加载顺序目前由资源数据库管理，任务层只负责执行单个 callable。
- 不做平台异步 IO。provider/source 仍以同步读取为主；后续如果加入文件包、远程资源或平台 IO，再在资源层评估接入方式。
- 不做强制取消。`request::cancel()` 只改变请求状态，不中断正在运行的线程或 callable。

后续只有在资源导入、热重载、编辑器工具或大量后台作业暴露出明确需求后，才会继续评估优先级、等待 API、
进度聚合、任务图、协程适配或更复杂的线程池策略。

## 命名空间和头文件

任务模块定义位于 `caors::tasks` 命名空间：

- `tasks::task`
- `tasks::executor`
- `tasks::inline_executor`
- `tasks::execution_context`
- `tasks::thread_pool`
- `tasks::shutdown_mode`
- `tasks::completion_queue`
- `tasks::request`
- `tasks::request_status`
- `tasks::submit_request()`

任务模块不再向根命名空间导出低层别名。任务、执行器、完成队列和请求状态统一通过
`caors::tasks::...` 使用，避免根命名空间承担运行期调度组合件。

公共头文件：

- `caors/tasks/task.h`
- `caors/tasks/executor.h`
- `caors/tasks/inline_executor.h`
- `caors/tasks/execution_context.h`
- `caors/tasks/thread_pool.h`
- `caors/tasks/completion_queue.h`
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

`inline_executor` 是第一版确定性实现：

```cpp
caors::tasks::inline_executor executor;

auto status = executor.get_executor().post(caors::tasks::task{[] {
  // 在 post 调用线程立即执行。
}});
```

## request

`request` 是任务提交后的共享状态句柄。它不保存任务返回值，只保存状态和错误；多个 `request` 副本共享同一状态，
可跨线程查询：

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

## completion_queue

`completion_queue` 是主线程 `execution_context` 的兼容包装，用于把完成回调排队到主线程：

```cpp
caors::tasks::completion_queue completions;

completions.push(caors::tasks::task{[] {
  // 在主线程 drain 时执行。
}});

std::size_t count = completions.drain();
```

需要注意：

- 后台线程可以调用 `push()`；`drain()` 和 `clear()` 由拥有该执行环境的线程调用，且不得彼此并发。
- `drain()` 开始后新增的任务会留到下一次 `drain()`。
- `clear()` 丢弃尚未开始的任务，并保留后续继续投递和执行的能力。
- `drain()` 返回执行数量，不聚合任务返回的错误。需要错误传播时，应把错误写入调用方自己的 request 状态。

## 与资源系统的关系

资源系统当前加载、重载和保存仍是同步 API。`tasks` 模块只是为后续异步资源流程提供底层拼装件：

- 后台阶段：通过 executor 读取 provider、调用 loader、构造资源对象。
- 主线程阶段：通过 completion queue 把成功结果插入 cache、替换资源、更新 generation，并发送 reload 通知。
- 请求状态：资源层使用 `resources::load_request<T>` 保存 queued/loading/loaded/failed/cancelled、错误和结果 handle。

第一阶段不会让资源 cache、manifest、provider 或对象系统在后台线程被隐式修改。
