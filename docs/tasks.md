<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Tasks

本文档说明 CAORS 任务调度模块的当前边界。任务系统第一阶段只提供最小公共底座：move-only
`task`、executor 抽象、同步 `inline_executor` 和主线程 `completion_queue`。它用于后续资源异步加载、
资源导入、后台序列化和跨线程完成回调。

## 设计目标

当前已覆盖：

- `tasks::task`：move-only 任务包装器，可保存 move-only lambda，任务 callable 可以返回 `void` 或
  `caors::error`。
- `tasks::executor`：最小提交抽象，只定义 `submit(task)`。
- `tasks::inline_executor`：提交后在调用线程立即执行，用于测试、确定性执行和同步 fallback。
- `tasks::completion_queue`：线程安全完成回调队列，适合后台任务完成后把 commit 阶段交给主线程。
- `task` 和 `completion_queue` 可传入 `memory_resource`，内部动态存储使用对应资源。

当前不会包含：

- 线程池。
- work stealing、fiber、协程 scheduler、任务图或平台异步 IO。
- 优先级、取消传播、进度和 request 状态对象。
- 自动捕获 callable 异常。
- 聚合 `completion_queue::drain()` 中每个任务返回的错误。

这些能力会在资源异步加载和资源热更新流程落地后继续扩展。

## 命名空间和头文件

任务模块定义位于 `caors::tasks` 命名空间：

- `tasks::task`
- `tasks::executor`
- `tasks::inline_executor`
- `tasks::completion_queue`

根命名空间保留常用别名：

- `caors::task`
- `caors::task_executor`
- `caors::inline_task_executor`
- `caors::task_completion_queue`

公共头文件：

- `caors/tasks/task.h`
- `caors/tasks/executor.h`
- `caors/tasks/inline_executor.h`
- `caors/tasks/completion_queue.h`
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

后续 `thread_pool_executor` 会复用同一个提交接口，但会补充停止、析构、drain、取消和线程安全边界。

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
- 请求状态：后续资源层会定义 `load_request` 保存 queued/loading/loaded/failed/cancelled、错误和可选进度。

第一阶段不会让资源 cache、manifest、provider 或对象系统在后台线程被隐式修改。
