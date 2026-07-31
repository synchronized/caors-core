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

meta / serialization / object
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
- `caors/memory/pmr.h`：CAORS `memory_resource` 与标准 `std::pmr::memory_resource` 的双向
  非拥有适配。
- `caors/memory/stack.h`：支持嵌套标记和作用域回滚的线程局部临时资源。
- `caors/memory/fixed_block_pool.h`：固定块池策略资源。
- `caors/memory/resource.h`：同时提供裸内存 RAII 句柄 `memory_block` 和
  `allocate_block()` / `try_allocate_block()`。
- `caors/memory/unique_ptr.h`：可保存运行期销毁回调的 `erased_deleter<T>` 和
  `memory::unique_ptr<T>`。
- `caors/memory.h`：内存模块聚合入口，包含以上所有常用入口。

实现文件也按相同职责拆分在 `src/caors/memory/`；共享的内部对齐、溢出检查、默认资源解析等工具保留在私有头 `src/caors/memory/detail/internal.h`。

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
caors::memory::callback_memory_resource resource(&allocate_fn, &deallocate_fn, user_data);
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

安全规则：

- `set_default_memory_resource()` 适合启动期配置默认资源，不应作为运行期频繁切换开关。
- 通过 `caors::make<T>()` 创建的裸指针，如果没有显式传入资源，销毁时 `caors::destroy(ptr)`
  会读取当前默认资源。若中间切换过默认资源，可能出现分配和释放资源不配套。需要跨越切换点时，应显式使用
  `caors::make<T>(&resource)` 和 `caors::destroy(ptr, &resource)`，或使用 `memory::make_unique()` 保存匹配销毁回调。
- 通过 `caors::allocate()` 分配的裸内存也应在释放时传回同一个资源、大小和对齐。不要依赖切换后的默认资源释放旧分配。
- 不要把栈上的临时 `memory_resource` 设置成长期默认资源。`allocator<T>`、`object_manager`、`memory::unique_ptr`
  和 `meta::any` 等类型可能保存 `memory_resource*`，它们不拥有资源对象，调用方必须保证资源生命周期更长。
- 多线程运行中不应切换默认资源。默认资源指针使用原子变量只能避免数据竞争，不能保证同一块内存的分配和释放走同一个资源。
  需要切换时，应在明确的停机点完成，并保证没有跨越切换点存活的默认资源分配。

### memory_block

`memory_block` 是一块裸内存的 RAII 句柄。它保存 `ptr`、`size`、`alignment` 和分配时解析到的
`memory_resource*`，析构或 `reset()` 时会用同一个资源释放内存，不再读取当前默认资源：

```cpp
caors::tracking_resource tracking(&caors::get_system_memory_resource());

caors::memory_block block = caors::allocate_block(128, 64, &tracking);
// block 析构时自动调用 tracking.deallocate(ptr, 128, 64)
```

如果需要不抛异常的路径，可以使用 `try_allocate_block()`。分配失败或非法对齐时返回空句柄：

```cpp
caors::memory_block block = caors::try_allocate_block(128, 64, &tracking);
if (!block) {
  // 分配失败
}
```

需要把裸指针交给外部系统时，可以调用 `release()` 释放所有权并返回裸指针。调用方必须保存并使用原来的资源、大小和对齐释放：

```cpp
void* ptr = block.release();
tracking.deallocate(ptr, 128, 64);
```

推荐规则：

- 普通代码需要持有裸内存时优先使用 `memory_block`。
- 只有在实现底层 resource、对接外部 C API 或确实需要手动生命周期时，才直接使用
  `allocate()` / `deallocate()`。
- `memory_block` 不构造对象；对象生命周期仍应使用 `make<T>()` / `destroy<T>()` 或
  `memory::make_unique<T>()`。

### 内部全局分配策略

类型注册表、`string_name` 表、反射数据和序列化 schema capability 数据等内部全局数据，默认不跟随
`default_resource`，而是固定使用 `system_memory_resource`。原因是这些数据可能在静态初始化阶段被创建，
此时应用通常还没有机会调用 `set_default_memory_resource()`；如果让它们隐式捕获默认资源，容易产生初始化顺序
依赖。

需要调试内部全局元数据分配时，可以在编译 CAORS 前定义：

```cpp
#define CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE 1
```

开启后，内部全局数据会使用访问时的 CAORS 默认资源。该选项适合测试和诊断；正式使用时仍建议保持默认值，
让这些 append-only 全局表使用稳定的系统资源。

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

`caors::make<T>(resource, ...)` 会使用标准 uses-allocator construction 构造对象。对
`caors::vector<T>`、`caors::string`、`caors::hash_map<K, V>` 等 allocator-aware 类型，传入的
`memory_resource` 会转换成对应 `caors::allocator` 并传给对象构造函数；`caors::allocator<T>`
在构造容器元素时也会继续使用同样规则，因此 `caors::vector<caors::string>` 这类嵌套
allocator-aware 类型可以继续向内传播资源。

CAORS 不会调用 `std::pmr::set_default_resource()`，避免库代码修改进程级全局状态。

### 标准 PMR 适配

`caors/memory/pmr.h` 提供两个可选的非拥有 view：

- `pmr_resource_view`：把 CAORS `memory_resource` 暴露为 `std::pmr::memory_resource`；
- `caors_resource_view`：把外部 `std::pmr::memory_resource` 暴露为 CAORS
  `memory_resource`。

标准 PMR 容器可以复用 CAORS tracking、monotonic、stack 等资源：

```cpp
caors::tracking_resource tracking{&caors::get_system_memory_resource()};
caors::pmr_resource_view pmr_view{tracking};

std::pmr::vector<int> values{&pmr_view};
values.push_back(42);
```

反方向也可以让 CAORS allocator 使用标准 PMR 资源：

```cpp
std::pmr::unsynchronized_pool_resource pool;
caors::caors_resource_view caors_view{pool};

caors::vector<int> values{caors::allocator<int>{&caors_view}};
values.push_back(42);
```

两个 view 都不拥有底层资源。销毁顺序必须是：

```text
使用资源的容器和分配结果
  -> 资源 view
  -> 底层 memory_resource
```

适配器不增加同步，线程安全性继承底层资源。PMR 分配只有抛出式入口；开启异常时，
`caors_resource_view::try_allocate()` 会捕获底层 PMR 分配异常并返回 `nullptr`。

CAORS 默认资源与 `std::pmr` 默认资源保持独立，适配器不会调用
`set_default_memory_resource()` 或 `std::pmr::set_default_resource()`。PMR 类型属于原生 C++ ABI，
不得通过 CAORS 稳定插件 C ABI 传递；插件卸载前必须销毁所有引用插件资源的 view、allocator、容器和
分配结果。

### memory::unique_ptr

`caors::memory::unique_ptr<T>` 是 `std::unique_ptr<T, memory::erased_deleter<T>>` 的别名。
默认情况下它仍然使用普通 `delete`；当序列化层通过 `serialization::context::resolvers.memory.resource`
构造内部对象或运行期派生对象时，wrapper 适配器可以把匹配的销毁回调写入 deleter。

`memory::make_unique<T>(resource, ...)` 会使用指定 `memory_resource` 构造对象，并让返回的
`memory::unique_ptr<T>` 保存匹配销毁回调。

这个类型主要服务于需要资源托管的 owning pointer 字段，包括普通字段反序列化和普通类多态反序列化：

```cpp
struct scene {
  caors::memory::unique_ptr<actor> owner;
};
```

如果字段使用普通 `std::unique_ptr<T>`，序列化层仍可按默认 `new/delete` 路径构造对象，但不能把
`memory_resource` 构造出的对象安全交给默认 deleter。需要资源托管时，应显式使用
`caors::memory::unique_ptr<T>`。

### Guard 与填充调试

`debug_tracking_resource` 默认在每个用户分配前后放置 16 字节 guard，并在释放时检查是否被修改。
它还会使用固定字节模式填充分配和释放的用户区域，帮助定位未初始化读取和释放后使用：

```cpp
caors::memory::memory_debug_options options;
options.guard_size = 32;
options.allocation_fill_pattern = std::byte{0xCD};
options.deallocation_fill_pattern = std::byte{0xDD};

caors::memory::debug_tracking_resource debug(
    &caors::get_system_memory_resource(), options);
```

可以通过统计分别观察前越界和后越界：

```cpp
const auto stats = debug.debug_statistics();
std::size_t front_errors = stats.front_guard_corruption_count;
std::size_t back_errors = stats.back_guard_corruption_count;
```

需要接入日志或断点系统时，可以设置非拥有诊断回调。回调在内部记录锁之外执行，并且必须
`noexcept`：

```cpp
debug.set_violation_callback(
    [](const caors::memory::memory_debug_violation& violation, void*) noexcept {
      // 根据 violation.kind、allocation 和调用方释放参数记录诊断。
    });
```

重要边界：

- guard 损坏、释放参数不匹配和非法释放分别统计；
- 即使 guard 损坏，上游仍使用内部保存的原始指针、大小和对齐安全释放；
- `guard_size == 0` 会关闭 guard 和对齐 padding，但分配/释放填充仍可独立启用；
- 关闭 `fill_on_allocate` 或 `fill_on_deallocate` 可以避免对应的完整 payload 扫描；
- 该资源只适合显式开发和测试路径，普通 resource 与未使用该资源的 Release 路径没有新增开销；
- guard 只能在释放时发现边界损坏，不能替代 ASan，也不能保证检测越过整个 guard 的大范围写入；
- 诊断回调和 options 不拥有其中的外部对象，调用方必须保证相关生命周期。

### 容器依赖边界

`caors/core/containers.h` 的底层只依赖 `caors/memory/resource.h`，不会反向包含完整 `memory.h`。需要遍历
memory 内部调试记录时，底层 `debug_tracking_resource` API 使用 callback：

```cpp
debug.for_each_live_allocation(
    [](const caors::memory::memory_debug_record& record, void* user_data) {
      // 直接消费 record，不要求 memory 层构造容器。
    },
    nullptr);
```

如果调用方需要快照容器，可以使用 `caors/memory/debug_tools.h` 提供的便利 helper：

```cpp
caors::vector<caors::memory::memory_debug_record> records =
    caors::memory::collect_live_allocations(debug);
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
- `stack_resource`：支持严格 LIFO 的 mark/rewind，适合同步嵌套调用的临时工作内存。
- `fixed_block_pool`：固定块池，适合同尺寸对象频繁创建和销毁。

### stack_resource

`stack_resource` 是基于 chunk 的单调栈式资源。单次 `deallocate()` 不回收空间；调用方通过
`stack_scope` 在进入时创建 marker，并在离开作用域时回滚到原位置：

```cpp
caors::stack_resource temporary;

{
  caors::stack_scope outer{temporary};
  void* outer_data = outer.resource()->allocate(128, 16);

  {
    caors::stack_scope inner{temporary};
    void* inner_data = inner.resource()->allocate(256, 16);
    // inner_data 只在 inner 作用域内有效。
  }

  // 内层已经回滚，outer_data 仍然有效。
}
```

marker 必须由同一个资源创建、在同一线程使用，并严格按后进先出顺序回滚。`reset()` 和 `trim()` 只能在
没有活动 marker 时调用。Debug 构建会断言跨资源 marker、过期 marker 和乱序回滚。

回滚只回收字节，不调用对象析构函数。使用该资源的容器或非平凡对象必须先析构，随后才能析构
`stack_scope`。任何返回值、目标对象或借用 view 都不得保存指向已回滚区域的地址。

`library_temporary_resource()` 返回当前线程复用的 `stack_resource`。同步调用不需要每次创建资源，只需
创建轻量 `stack_scope`；普通 chunk 可以复用，large chunk 会在回滚时释放。该资源不是线程安全的，也不能
假设不同 DLL/plugin 内的线程局部实例相同。跨模块调用应由 host 显式传入临时资源，使分配和最终释放位于
同一 runtime 边界。

长期空闲或完成异常大的任务后，可以在没有活动 marker 时调用 `trim()`，释放基础 chunk 之外的缓存；
`reset()` 则释放全部 chunk。

### strategy resource 统计

三种内置策略资源都提供独立的 `statistics()` 快照和 `reset_statistics()`：

- `stack_resource_statistics`：占用字节、chunk、large allocation、mark、rewind、trim 和 reset；
- `monotonic_resource_statistics`：占用字节、chunk、large allocation 和 reset；
- `fixed_block_pool_statistics`：使用/空闲/总块数、chunk、分配/归还、拒绝请求、上游失败和 release。

统计在 Debug/Release 均可用，并使用普通整数；它不会改变这些策略资源本身“非线程安全”的约束。
`reset_statistics()` 只清零累计事件计数，把峰值重置为当前水位，不释放资源内容。策略资源内部水位用于观察
chunk 或 block 的实际使用情况；需要逐地址泄漏、标签和错误释放诊断时，仍应使用
`debug_tracking_resource`。

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
caors::memory::debug_tracking_resource debug(&caors::get_system_memory_resource());

void* ptr = debug.allocate_debug(128, 64, "temporary-buffer");

debug.for_each_live_allocation(
    [](const caors::memory::memory_debug_record& record, void*) {
  // record 中包含 ptr、size、alignment、label、file_name、function_name、line 和 column。
    },
    nullptr);

caors::vector<caors::memory::memory_debug_record> records = caors::memory::collect_live_allocations(debug);

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
- 调试包装：`debug_tracking_resource`，支持存活记录、guard、分配/释放填充和违规回调
- 线性策略：`monotonic_resource`
- 作用域临时策略：`stack_resource` / `stack_scope` / `library_temporary_resource()`
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
- [x] `stack_resource`：面向同步嵌套调用的作用域临时内存。
- [x] `fixed_block_pool`：面向固定大小对象池。
- [x] `try_allocate`：提供不抛异常的失败返回路径。
- [x] 扩展越界检测、填充值和 guard block 等调试能力。
- [ ] 评估是否需要同步包装器；默认优先让 backend 自己处理线程安全。
- [ ] 可选第三方后端：jemalloc、mimalloc、rpmalloc。

## 相关入口

- 可运行示例位于 `examples/memory/`，示例组织规则见 [examples/README.md](../examples/README.md)。
- 行为测试位于 `tests/memory/`，测试组织规则见 [tests/README.md](../tests/README.md)。
