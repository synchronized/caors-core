<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Memory

本文档说明 CAORS 内存层的当前设计边界、已实现能力和后续扩展方向。当前实现仍处于基础阶段，聚合入口为 `caors/memory.h`。

## 设计目标

CAORS 内存层希望同时支持两类用法：

- 简单场景可以直接使用默认入口，例如 `caors::allocate()` / `caors::deallocate()`。
- 性能敏感或生命周期明确的模块可以显式传入 `memory_resource`，让不同系统使用不同分配策略。

这套设计接近标准库 allocator/resource 分层：资源负责分配策略，`allocator<T>` 和 CAORS 基础容器负责把资源接入对象与容器。
同时保留一组全局默认入口，方便后续在引擎或应用初始化阶段统一替换默认分配器。

依赖方向固定为：

```text
memory/resource, memory/tracking, memory/debug_tracking
  -> 不依赖 core 容器

core containers
  -> 依赖 memory_resource / allocator<T>

meta / reflection / serialization / object
  -> 默认使用 core containers
```

因此底层 resource API 不返回 `caors::vector`、`caors::string` 这类上层容器；需要容器化调试结果时，由
`caors/memory/debug_tools.h` 提供可选便利函数完成收集。

## 核心概念

### 文件组织

内存模块按职责拆分公共头文件：

- `caors/memory/resource.h`：`memory_resource`、系统后端、回调后端、默认资源入口、`allocator<T>`、`make<T>()` / `destroy<T>()`。
- `caors/memory/tracking.h`：`tracking_resource` 和基础统计快照。
- `caors/memory/debug_tracking.h`：`debug_tracking_resource`、存活分配记录和调试统计。
- `caors/memory/debug_tools.h`：调试记录收集 helper，例如 `collect_live_allocations()`。
- `caors/memory/monotonic.h`：线性分配策略资源。
- `caors/memory/fixed_block_pool.h`：固定块池策略资源。
- `caors/memory.h`：兼容用聚合头，包含以上所有入口。

实现文件也按相同职责拆分在 `src/caors/memory/`；共享的内部对齐、溢出检查、默认资源解析等工具保留在私有头 `src/caors/memory/memory_internal.h`。

### memory_resource

`memory_resource` 是所有内存资源的统一抽象，负责定义分配、释放和资源等价性判断接口。

调用者可以直接使用某个资源：

```cpp
caors::memory_resource& resource = caors::get_system_memory_resource();
void* ptr = resource.allocate(128, 64);
resource.deallocate(ptr, 128, 64);
```

也可以通过全局入口并显式指定资源：

```cpp
void* ptr = caors::allocate(128, 64, &resource);
caors::deallocate(ptr, 128, 64, &resource);
```

分配失败有两类入口：

- `try_allocate()`：不抛异常，失败返回 `nullptr`。非法对齐、上游耗尽、后端返回空指针都会走这个路径。
- `allocate()`：易用包装。启用异常时失败抛出异常；关闭异常时触发 fail-fast。

释放入口 `deallocate()` 保持 `noexcept`，调用方需要传回与分配时一致的大小和对齐。

### system_memory_resource

`system_memory_resource` 是当前内置系统后端。

它使用 C++ `operator new` / `operator delete` 分配内存；当请求对齐大于 `caors::config::default_new_alignment` 时，使用对齐版本的 `operator new` / `operator delete`。

这个资源适合作为默认后端，优点是没有额外依赖，跨平台行为稳定。

### callback_memory_resource

`callback_memory_resource` 是回调适配器。

它不直接决定底层如何分配，而是把分配和释放请求转发给调用方提供的函数指针：

```cpp
caors::callback_memory_resource resource(&allocate_fn, &deallocate_fn, user_data);
```

它适合临时接入外部分配器或已有系统，例如：

- jemalloc
- mimalloc
- rpmalloc
- 引擎已有 allocator
- 自定义 arena / pool

如果未来 CAORS 提供专用 `jemalloc_memory_resource` 或 `mimalloc_memory_resource`，`callback_memory_resource` 仍然可以作为低成本扩展点保留。

### default_resource

`default_resource` 是默认路由指针，不是分配策略本身。

当调用方没有显式传入资源时：

```cpp
void* ptr = caors::allocate(128);
```

内部会按以下顺序选择资源：

```text
显式传入的 resource
  -> default_resource
  -> system_memory_resource
```

也就是说，`default_resource` 的作用是决定“不指定资源时走谁”。

应用可以在初始化阶段替换默认资源：

```cpp
caors::set_default_memory_resource(&resource);
```

之后没有显式传入资源的 `caors::allocate()` 调用都会走新的默认资源。

### 基础容器适配

`caors::allocator<T>` 可以把 CAORS `memory_resource` 接入标准容器。`caors/core/containers.h`
进一步提供常用容器别名：

```cpp
caors::tracking_resource tracking(&caors::get_system_memory_resource());

caors::vector<int> values{caors::allocator<int>{&tracking}};
values.push_back(42);

auto names = caors::make_hash_map<int, caors::string>(&tracking);
names.emplace(1, caors::string{"player", caors::allocator<char>{&tracking}});
```

`caors::string`、`caors::vector<T>`、`caors::hash_map<K, V>` 和 `caors::hash_set<T>` 都使用
`caors::allocator`。默认构造时，allocator 会捕获当前 CAORS 默认 `memory_resource`；显式资源可以通过
allocator 构造参数或 `make_string()`、`make_vector()`、`make_hash_map()`、`make_hash_set()` 工厂传入。

CAORS 不会调用 `std::pmr::set_default_resource()`，避免库代码修改进程级全局状态。

### 容器依赖边界

`caors/core/containers.h` 的底层只依赖 `caors/memory/resource.h`，不会反向包含完整 `memory.h`。需要遍历
memory 内部调试记录时，底层 `debug_tracking_resource` API 使用 callback：

```cpp
debug.for_each_live_allocation(
    [](const caors::memory_debug_record& record, void* user_data) {
      // 直接消费 record，不要求 memory 层构造容器。
    },
    nullptr);
```

如果调用方需要快照容器，可以使用 `caors/memory/debug_tools.h` 提供的便利 helper：

```cpp
caors::vector<caors::memory_debug_record> records =
    caors::collect_live_allocations(debug);
```

这样可以避免 `memory -> core containers -> memory` 的循环依赖，也让底层 allocator/resource
保持更小的头文件依赖面。

## 分层模型

后续内存系统可以按职责分成三层。

### backend resource

后端资源负责真正向系统或第三方库申请内存。

当前已实现：

- `system_memory_resource`
- `callback_memory_resource`

未来可能增加：

- `jemalloc_memory_resource`
- `mimalloc_memory_resource`
- `rpmalloc_memory_resource`

### strategy resource

策略资源负责改变内存组织方式，通常会从某个上游资源拿较大的内存块，再自行管理。

当前已实现：

- `monotonic_resource`：线性分配器，适合一批对象一起释放的临时内存。
- `fixed_block_pool`：固定块池，适合同尺寸对象频繁创建和销毁。

### wrapper resource

包装资源不一定改变底层分配策略，而是在另一个资源外面增加能力。

当前已实现：

- `tracking_resource`：记录分配次数、释放次数、累计字节数、当前使用字节数和峰值使用字节数。
- `debug_tracking_resource`：记录存活分配、标签、调用位置、非法释放和释放参数不匹配。

暂不默认提供同步包装器。`system_memory_resource`、jemalloc、mimalloc 这类后端应由后端自身负责线程安全和线程局部优化；对 strategy resource 强行套锁容易变成负优化。

示例组合：

```text
caors::allocate()
  -> default_resource
  -> tracking_resource
  -> fixed_block_pool
  -> system_memory_resource
```

### debug_tracking_resource

`debug_tracking_resource` 是调试用包装资源。它比 `tracking_resource` 更重，会维护一张存活分配表，因此更适合开发期、测试期和问题排查，不建议默认放进热路径。

推荐通过 `allocate_debug()` 捕获调用点和标签：

```cpp
caors::debug_tracking_resource debug(&caors::get_system_memory_resource());

void* ptr = debug.allocate_debug(128, 64, "temporary-buffer");

debug.for_each_live_allocation(
    [](const caors::memory_debug_record& record, void*) {
  // record 中包含 ptr、size、alignment、label、file_name、function_name、line 和 column。
    },
    nullptr);

caors::vector<caors::memory_debug_record> records = caors::collect_live_allocations(debug);

debug.deallocate(ptr, 128, 64);
```

如果通过普通 `allocate()` 入口使用 `debug_tracking_resource`，资源仍会记录存活分配、大小和对齐，但不会有调用点标签。

释放时如果找不到对应记录，资源会把它计入 `invalid_deallocation_count`，并且不会继续转发到底层后端，避免重复释放或野指针释放继续破坏堆。释放传入的 size 或 alignment 与原始记录不一致时，会计入 `mismatched_deallocation_count`，并使用原始记录释放上游内存。

## 线程安全

当前 `default_resource` 使用 `std::atomic<memory_resource*>` 保存。

这只保证默认资源指针本身在多线程读写时不会产生 data race，例如：

```text
线程 A：caors::allocate()
线程 B：caors::set_default_memory_resource(&resource)
```

但它不保证指针指向的资源本身线程安全。

例如，未来如果某个 `fixed_block_pool` 默认不是线程安全的，那么多个线程同时通过它分配和释放仍然需要额外同步。这个问题应由资源本身或调用方在更高层解决。

边界可以这样理解：

```text
atomic default_resource
  保护默认 resource 指针读写

backend / strategy resource
  各自声明并保证自己的线程安全边界
```

## 当前已实现能力

- 统一抽象：`memory_resource`
- STL 适配：`allocator<T>`
- 基础容器：`string` / `vector` / `hash_map` / `hash_set`
- 对象入口：`make<T>()` / `destroy<T>()`
- 系统后端：`system_memory_resource`
- 回调后端：`callback_memory_resource`
- 统计包装：`tracking_resource`
- 调试包装：`debug_tracking_resource`
- 线性策略：`monotonic_resource`
- 固定块策略：`fixed_block_pool`
- 默认资源入口：`get_default_memory_resource()` / `set_default_memory_resource()`
- 全局分配入口：`try_allocate()` / `allocate()` / `deallocate()`
- 基础对齐分配
- 基础配置常量：`caors::config::default_new_alignment` / `caors::config::has_exceptions`

## 后续路线图

- [x] `allocator<T>`：适配 STL 容器。
- [x] `string` / `vector` / `hash_map` / `hash_set`：提供默认走 CAORS allocator 的基础容器别名。
- [x] `make<T>` / `destroy<T>`：统一对象构造和析构入口。
- [x] `tracking_resource`：统计分配和释放，辅助泄漏检测。
- [x] `debug_tracking_resource`：记录存活分配、调用点标签和错误释放。
- [x] `monotonic_resource`：面向临时内存和批量释放。
- [x] `fixed_block_pool`：面向固定大小对象池。
- [x] `try_allocate`：提供不抛异常的失败返回路径。
- [ ] 扩展越界检测、填充值和 guard block 等调试能力。
- [ ] 评估是否需要同步包装器；默认优先让 backend 自己处理线程安全。
- [ ] 可选第三方后端：jemalloc、mimalloc、rpmalloc。

## 相关入口

- 可运行示例位于 `examples/memory/`，示例组织规则见 [examples/README.md](../examples/README.md)。
- 行为测试位于 `tests/memory/`，测试组织规则见 [tests/README.md](../tests/README.md)。
