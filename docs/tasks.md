<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Tasks

本文档说明 CAORS 任务调度模块的当前边界。任务系统第一阶段只提供最小公共底座：move-only
`task`、executor 抽象、同步 `inline_executor`、固定线程数 `thread_pool_executor`、主线程
`completion_queue` 和轻量 `request` 状态句柄。它用于资源异步加载、资源导入、后台序列化和跨线程完成回调。

## 设计目标

当前已覆盖：

- `tasks::task`：move-only 任务包装器，可保存 move-only lambda，任务 callable 可以返回 `void` 或
  `caors::error`。
- `tasks::executor`：最小提交抽象，只定义 `submit(task)`。
- `tasks::inline_executor`：提交后在调用线程立即执行，用于测试、确定性执行和同步 fallback。
- `tasks::thread_pool_executor`：固定线程数后台执行器，析构默认 drain 已入队任务。
- `tasks::completion_queue`：线程安全完成回调队列，适合后台任务完成后把 commit 阶段交给主线程。
- `tasks::request`：提交任务后的共享状态句柄，覆盖 queued/running/completed/failed/cancelled 和错误查询。
- `task` 和 `completion_queue` 可传入 `memory_resource`，内部动态存储使用对应资源。

当前不会包含：

- work stealing、fiber、协程 scheduler、任务图或平台异步 IO。
- 优先级、阻塞等待、取消传播、进度和任务返回值保存。
- 任务异常诊断保存。
- 聚合 `completion_queue::drain()` 中每个任务返回的错误。

这些能力会在资源异步加载和资源热更新流程落地后继续扩展。

## 命名空间和头文件

任务模块定义位于 `caors::tasks` 命名空间：

- `tasks::task`
- `tasks::executor`
- `tasks::inline_executor`
- `tasks::thread_pool_executor`
- `tasks::shutdown_mode`
- `tasks::completion_queue`
- `tasks::request`
- `tasks::request_status`
- `tasks::submit_request()`

根命名空间保留常用别名：

- `caors::task`
- `caors::task_executor`
- `caors::inline_task_executor`
- `caors::thread_pool_task_executor`
- `caors::task_completion_queue`
- `caors::task_request`
- `caors::task_request_status`
- `caors::submit_request()`

公共头文件：

- `caors/tasks/task.h`
- `caors/tasks/executor.h`
- `caors/tasks/inline_executor.h`
- `caors/tasks/thread_pool_executor.h`
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

`executor` 只定义任务提交入口：

```cpp
class executor {
public:
  virtual caors::error submit(caors::tasks::task next) = 0;
};
```

`inline_executor` 是第一版确定性实现：

```cpp
caors::tasks::inline_executor executor;

auto status = executor.submit(caors::tasks::task{[] {
  // 在 submit 调用线程立即执行。
}});
```

`thread_pool_executor` 复用同一个提交接口，并在执行器自身定义停止、析构、drain、取消和线程安全边界。

## request

`request` 是任务提交后的共享状态句柄。它不保存任务返回值，只保存状态和错误；多个 `request` 副本共享同一状态，
可跨线程查询：

```cpp
caors::tasks::thread_pool_executor executor{2};

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

## thread_pool_executor

`thread_pool_executor` 是固定线程数后台执行器：

```cpp
caors::tasks::thread_pool_executor executor{2};

executor.submit(caors::tasks::task{[] {
  // 在后台 worker 中执行。
}});

executor.shutdown(caors::tasks::shutdown_mode::drain);
```

常用入口：

- `thread_pool_executor(thread_count, resource)`：创建固定数量 worker。`thread_count == 0` 时使用默认线程数。
- `submit(task)`：提交后台任务。空任务返回 `errc::invalid_argument`，关闭后返回 `errc::frozen`。
- `shutdown(shutdown_mode::drain)`：停止接收新任务，执行完已经入队的任务后退出 worker。
- `shutdown(shutdown_mode::cancel)`：停止接收新任务，丢弃尚未开始执行的任务，当前正在执行的任务继续完成。
- `pending()`：查询尚未开始执行的任务数量。
- `thread_count()`：查询 worker 数量。

需要注意：

- 析构会自动执行 `shutdown(shutdown_mode::drain)`。
- `thread_pool_executor` 自身不保存每个任务的返回错误；需要状态时使用 `submit_request()` 包装任务。
- 当启用异常时，worker 会捕获逃逸异常以避免后台线程终止进程，但当前不会保存异常信息。
- `shutdown(cancel)` 不是强制中断；已经开始执行的任务仍会跑完。
- 资源 cache、manifest、provider 和对象状态仍不应在后台 worker 中被隐式修改。

## completion_queue

`completion_queue` 用于把完成回调排队到主线程：

```cpp
caors::tasks::completion_queue completions;

completions.push(caors::tasks::task{[] {
  // 在主线程 drain 时执行。
}});

std::size_t count = completions.drain();
```

需要注意：

- `push()`、`size()`、`empty()`、`clear()` 和 `drain()` 内部受互斥锁保护。
- `drain()` 执行任务期间不持锁，任务内部可以继续 `push()`。
- `drain()` 开始后新增的任务会留到下一次 `drain()`。
- `drain()` 返回执行数量，不聚合任务返回的错误。需要错误传播时，应把错误写入调用方自己的 request 状态。

## 与资源系统的关系

资源系统当前加载、重载和保存仍是同步 API。`tasks` 模块只是为后续异步资源流程提供底层拼装件：

- 后台阶段：通过 executor 读取 provider、调用 loader、构造资源对象。
- 主线程阶段：通过 completion queue 把成功结果插入 cache、替换资源、更新 generation，并发送 reload 通知。
- 请求状态：资源层使用 `resources::load_request<T>` 保存 queued/loading/loaded/failed/cancelled、错误和结果 handle。

第一阶段不会让资源 cache、manifest、provider 或对象系统在后台线程被隐式修改。
