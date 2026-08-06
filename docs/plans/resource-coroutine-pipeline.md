<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Resources 与 Assets 协程流水线开发计划

## 文档状态

- 当前状态：方案基线，尚未开始实现；
- 前置条件：`asio-task-redesign` 阶段 0 到阶段 5 已完成；
- 目标：让现有资源请求可安全 `co_await`，减少手工轮询，同时保持 cache 提交线程、共享请求和
  取消语义明确；
- 首批范围：`load_request<T>`、`preload_request` 的多等待者支持、协程适配、内存资源、测试和文档；
- 非目标：不把 cache 改成并发容器，不改变 loader 同步接口，不引入隐式线程池，不在首批替换
  assets 公共 API，也不实现超时、优先级、远程 I/O 或结构化取消树。

## 当前基线

资源异步加载已经分成两个执行阶段：

```text
request_load<T>(guid, background_executor, completion_executor)
  -> background_executor: loader<T>::load()
  -> completion_executor: cache<T>::insert()/replace()
  -> load_request<T>: loaded / failed / cancelled
```

当前行为：

- resources 入口接收两个轻量 executor，不依赖具体 `completion_queue`；
- assets 仍保留 `executor + completion_queue&` 兼容 API；
- `load_request<T>` 和 `preload_request` 是可复制共享状态句柄，查询由互斥量保护；
- 同类型、同 GUID 的进行中请求可以复用；
- cache 和 loader 以引用进入异步任务，必须比请求流水线存活更久；
- cache 只在 completion executor 所属线程修改；
- 取消是共享且协作式的，不会中断已经运行的 loader；
- 当前等待方式是轮询 request 或 pump `completion_queue`，尚不支持非阻塞 `co_await`。

现有 Windows Clang Debug 基线为全量 86 项测试通过。安装包已分别验证基础 API、核心 tasks 和
coroutine consumer；本计划不能破坏普通 consumer 不包含 coroutine 头的能力。

## 核心决策

### 1. 首批让请求可等待，不重写加载流水线

保留现有 `request_load/request_reload/request_preload` 作为启动操作的正式 API。新增显式适配头：

```cpp
#include <caors/resources/coroutine.h>

auto request = database.request_load<texture>(guid, worker, main_executor);
caors::resources::handle_result<texture> result =
    co_await caors::resources::async_wait(request);
```

预加载对应返回 `preload_result`：

```cpp
caors::resources::preload_result result =
    co_await caors::resources::async_wait(preload_request);
```

不让 request 自身直接实现 `operator co_await`，原因是：

- 普通 resources 头不必包含 `<coroutine>`；
- coroutine 能力保持显式可选；
- `async_wait()` 与 tasks 的命名和取消边界一致；
- 未来可以增加带 executor 或取消策略的等待适配，而不扩大 request 基础布局。

### 2. 请求完成线程就是默认恢复线程

等待者在请求进入终态后，由完成该请求的线程恢复。正常加载通常在 completion executor 线程恢复；
后台加载失败可能在 worker 线程进入终态，因此调用方不能假设固定恢复线程。

需要固定线程时显式切换：

```cpp
auto result = co_await caors::resources::async_wait(request);
if (caors::error status = co_await caors::tasks::schedule(main_executor)) {
  co_return status;
}
```

首批不隐式增加一次 executor hop。这样避免隐藏调度开销，并保留现有失败路径的时序。

### 3. 多等待者与锁外恢复

`load_request<T>` 和 `preload_request` 的共享状态增加类型擦除 waiter 列表：

- 注册、终态转换和 waiter 摘取在状态锁内完成；
- coroutine 恢复必须在释放状态锁后执行；
- 已处于终态的请求不挂起；
- 多个 coroutine 可以同时等待同一个复用请求；
- 等待方 frame 提前销毁后，完成通知不得访问该 frame。

实现应复用或抽取 tasks request 已验证的 waiter 模型，避免 resources 再实现一套不同协议。

### 4. 取消保持现有共享语义

- 销毁等待方只取消等待，不取消资源请求；
- `load_request<T>::cancel()` 仍取消所有共享句柄观察到的同一请求；
- `preload_request::cancel()` 仍只取消该聚合请求，不强制取消被其它请求共享的底层 load；
- 首批不把 coroutine cancellation 自动传播到资源请求；
- 若未来增加传播策略，必须由独立 API 显式选择。

### 5. 生命周期不因 coroutine 自动变安全

cache、loader、database、asset manager 和 provider 的生命周期约束保持不变。`co_await` 只延长 request
共享状态，不延长这些外部对象。首批应在公共注释和测试中明确：销毁资源服务前必须停止提交并 drain
或取消其在途工作。

不在本计划中用 `shared_ptr` 隐式拥有整个资源系统。若需要可卸载服务或插件资源加载，应另行设计
owner/lease 边界。

### 6. 内存资源

- waiter 列表使用 request 状态的长期 `memory_resource`；
- 每次 `async_wait()` 的辅助状态使用同一长期资源；
- coroutine frame 继续由编译器默认长期分配器管理；
- 不使用同步 TLS scratch 保存跨挂起状态；
- 增加 tracking resource 测试，验证完成、取消和 frame 提前销毁后恢复到分配基线。

### 7. Assets API 后置

首批完成 resources 请求等待后，再评估 assets 的两步迁移：

1. 增加接收 background/completion executor 的重载，保留现有 completion queue 兼容入口；
2. 根据真实调用点决定是否增加 `async_load/async_reload/async_preload` 便利 awaiter。

在命名、返回错误和取消传播没有稳定前，不直接删除 assets 现有请求 API。

## 风险

- **丢失唤醒**：注册 waiter 与请求完成竞争，必须用单一状态锁和终态复检处理；
- **锁内恢复**：可能导致死锁或重入破坏状态，必须先摘取 waiter 再恢复；
- **悬空 frame**：排队通知与 frame 销毁竞争，需要独立共享等待状态协调；
- **线程误判**：loader 失败与 cache commit 可能在不同线程完成，文档不得承诺固定恢复线程；
- **对象生命周期**：request 存活不代表 cache/loader 存活；
- **共享取消**：复用请求的一个句柄取消会影响所有观察者，协程适配不得隐藏这一点；
- **额外分配**：每个等待操作可能产生一次长期分配，需要 tracking 与基准确认成本；
- **API 重叠**：tasks 与 resources 都有 `async_wait()`，应依靠参数类型和命名空间保持清晰。

## 实施阶段

### 阶段 0：行为与测试基线

- [x] 记录双 executor、请求复用、取消、线程和生命周期现状；
- [x] 明确首批不修改 loader/cache 并发模型；
- [ ] 增加完成线程与多个观察者的显式基线测试；
- [ ] 记录 request 创建及等待辅助状态的分配基线。

### 阶段 1：共享等待协议

- [ ] 为 `load_request<T>` 增加多个 waiter 注册和锁外通知；
- [ ] 为 `preload_request` 增加多个 waiter 注册和锁外通知；
- [ ] 覆盖完成、失败、取消、终态注册和注册/完成竞争；
- [ ] 验证 request 表复用时只完成一次但通知全部等待者。

### 阶段 2：Coroutine 适配

- [ ] 新增 `caors/resources/coroutine.h`；
- [ ] 实现 `async_wait(load_request<T>)`；
- [ ] 实现 `async_wait(preload_request)`；
- [ ] 覆盖等待方提前销毁、无异常构建与内存资源释放；
- [ ] 保持 `caors/resources.h` 不隐式包含 coroutine 适配。

### 阶段 3：Assets 边界

- [ ] 增加接收两个轻量 executor 的 assets 重载；
- [ ] 保留并测试 completion queue 兼容入口；
- [ ] 根据调用点决定是否增加直接返回 awaiter 的便利 API；
- [ ] 不在没有迁移文档和测试时删除现有 API。

### 阶段 4：文档、安装与性能验证

- [ ] 更新 resources/assets/tasks 正式文档；
- [ ] 增加独立安装包 resources coroutine consumer；
- [ ] Windows Clang/MSVC Static/Shared Debug/Release；
- [ ] Linux Clang/GCC Static/Shared、ASan；
- [ ] 记录无等待、单等待和多等待的分配次数与 p50/p95 延迟；
- [ ] 固定 Linux 环境完成 UBSan 与正式性能复验。

## 完成标准

- 资源请求可以被多个 coroutine 非阻塞等待；
- 无丢失唤醒、锁内恢复或 frame 销毁后访问；
- cache 修改仍只发生在显式 completion executor；
- 取消和请求复用语义与迁移前一致；
- 普通 resources consumer 不引入 coroutine 头；
- 安装包、Static/Shared 和异常开关验证通过；
- 文档明确线程、所有权、生命周期和失败行为。
