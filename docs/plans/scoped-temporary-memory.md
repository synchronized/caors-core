<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 作用域临时内存开发计划

## 文档状态

- 当前状态：实施中；`stack_resource`、`stack_scope` 和线程局部入口已完成；
- 目标：为同步、嵌套调用提供低成本临时分配，并在作用域结束时回滚到进入前的位置；
- 首个使用方：serialization 的 MessagePack writer/parser 和事务式容器读入；
- 非目标：不替代返回值、目标对象、借用存储、异步任务或全局元数据使用的长期内存资源。

## 背景

现有 `memory::monotonic_resource` 支持连续分配和整池 `reset()`，适合生命周期一致的一批对象，但不能安全
支持下面的嵌套调用：

```text
外层序列化 mark A
  分配外层临时值
  内层序列化 mark B
    分配内层临时值
  回滚到 B
  继续使用外层临时值
回滚到 A
```

如果内层直接调用 `monotonic_resource::reset()`，外层仍在使用的分配也会失效。因此需要可嵌套的
checkpoint/rewind 模型，而不是共享资源上的无条件整池重置。

## 核心设计

### `stack_resource`

新增基于 chunk 的单调栈式资源：

```cpp
class stack_resource final : public memory_resource {
public:
  struct marker;

  explicit stack_resource(std::size_t initial_capacity = 64 * 1024,
                          memory_resource* upstream = nullptr);

  [[nodiscard]] marker mark() const noexcept;
  void rewind(marker position) noexcept;
  void reset() noexcept;
};
```

`marker` 至少记录：

- 所属 `stack_resource` 的身份或 Debug cookie；
- 当前 chunk；
- 当前 chunk offset；
- 可选 generation，用于识别 reset 后使用旧 marker。

`rewind(marker)` 必须满足：

- 只允许回滚本资源产生的有效 marker；
- 恢复 marker 所在 chunk 的 offset；
- marker 之后新增的 chunk 可以释放给 upstream，也可以按缓存上限保留复用；
- Debug 构建检测跨资源 marker、重复回滚、非 LIFO scope 和过期 generation；
- Release 构建不在分配热路径增加原子操作。

与 `monotonic_resource` 一样，单次 `deallocate()` 不回收空间；内存只通过 rewind/reset 批量回收。

### `stack_scope`

提供严格 LIFO 的 RAII scope：

```cpp
class stack_scope final {
public:
  explicit stack_scope(stack_resource& resource) noexcept;
  ~stack_scope();

  [[nodiscard]] memory_resource* resource() noexcept;
};
```

构造时保存 marker，析构时 rewind。scope 不可拷贝、不可移动，避免 marker 生命周期漂移。

必须明确：rewind 只回收字节，不调用对象析构函数。因此所有使用临时资源的容器和非平凡对象必须在
`stack_scope` 析构前先正常析构：

```cpp
stack_scope scope{stack};
{
  vector<temporary_value> values{allocator<temporary_value>{scope.resource()}};
  // values 先析构，随后 scope 才回滚内存。
}
```

不能通过裸 placement new 把需要析构的对象放入栈资源后直接依赖 rewind。

### 线程局部资源

提供库内部入口：

```cpp
[[nodiscard]] stack_resource& library_temporary_resource() noexcept;
```

第一版使用每线程独立实例：

```cpp
thread_local stack_resource temporary;
```

这样普通同步序列化不需要锁或原子计数，嵌套调用通过 marker 保留外层分配。

默认同步 API 不在每次调用时重新构造 `stack_resource`。每个线程只创建并复用一个资源，每次调用只创建
轻量的 `stack_scope`：

```cpp
auto& temporary = library_temporary_resource();
stack_scope scope{temporary};
```

因此常规调用只承担保存 marker 和析构时 rewind 的成本。首次或扩容时申请的普通 chunk 可以被同一线程
后续调用复用。

限制：

- 从哪个线程创建的 scope，必须在哪个线程析构；
- 临时指针不能传给异步任务或其他线程；
- DLL/插件边界不承诺各模块的 thread-local 实例地址一致；
- 跨 runtime 边界时应由 host 显式传入 temporary resource，不能依赖模块本地默认入口。

### 资源选择优先级

serialization 按以下顺序选择临时资源：

1. 调用方显式传入的 `temporary_resource`；
2. 当前线程的 `library_temporary_resource()`。

不为每次 convenience API 调用创建独立 `stack_resource`。只有测试、低频隔离任务或调用方提供固定初始
缓冲区的场景，才适合由调用方按调用创建资源。

跨 DLL/plugin 调用时由 host 显式传入资源，确保分配、回滚和最终释放发生在同一 runtime 边界。插件不得
假设自身模块中的线程局部默认资源与 host 是同一实例。

### chunk 与超大分配策略

建议默认 chunk 为 64 KiB，并采用有上限的增长策略：

- 普通请求从当前 chunk 顺序分配；
- 空间不足时新增 chunk，容量按倍数增长；
- 大于当前增长容量的请求使用独立 large chunk；
- rewind 时 large chunk 默认立即释放；
- 普通 chunk 允许保留少量高水位缓存，避免每次调用重新向 upstream 分配；
- 提供 Debug 统计：当前占用、峰值、chunk 数、large allocation 次数和回滚次数。

资源提供：

```cpp
void trim() noexcept;
```

`trim()` 只允许在不存在活动 scope 时调用，用于释放基础 chunk 之外的缓存。worker 线程长期空闲、完成一批
超大任务或测试结束时可以主动调用。普通 rewind 不应永久保留 large chunk。

普通 chunk 的默认缓存上限在 benchmark 后确定，第一版不公开复杂调参 API，但必须设置内部上限，避免一次
异常大输入让线程局部资源永久保持峰值容量。

## Serialization 双资源边界

serialization 应显式区分：

```text
temporary resource
├─ writer 容器 frame
├─ 字段名和成员暂存
├─ parser 中间节点
├─ 事务式容器元素
└─ schema 过滤临时列表

result / target resource
├─ to_messagepack() 返回 byte_buffer
├─ parse_messagepack() 返回 variant
├─ 目标对象最终拥有的数据
├─ string_view 的外部 string_storage
├─ preserved unknown fields
└─ 逃出调用的 resolver 或异步状态
```

第一版不向 `serialization::context` 增加格式专用字段。临时资源有两种来源：

1. typed convenience API 内部创建 `stack_scope{library_temporary_resource()}`；
2. 高级调用方通过跨格式的通用 temporary resource 入口显式覆盖。

如果需要公开覆盖，建议扩展已有 memory resolver：

```cpp
struct memory_options {
  memory_resource* resource = nullptr;           // 目标/返回值资源
  memory_resource* temporary_resource = nullptr; // 本次调用临时资源
};
```

它属于跨格式运行期资源，不放入 `json_options` 或 `messagepack_options`。

### 流式对象

`messagepack_writer`、流式 parser 等跨多次成员函数调用保存状态的对象，应在构造时绑定临时资源，而不是
每次 `begin_*`、`write_*` 或 `read_*` 时重新查询线程局部入口：

```cpp
messagepack_writer writer{output, temporary_resource};
```

临时资源必须比 writer/parser 活得更久。每个完整文档开始时记录 marker，在文档成功、失败或显式 reset 后
回滚；不能在仍有未完成 frame 时 trim 或销毁资源。未显式传入时，构造函数绑定当前线程的默认临时资源，
对象随后不得跨线程使用。

### MessagePack writer

当前 `messagepack_writer` 使用 `variant` 容器帧暂存字段。迁移后：

- frame 栈和 frame 内临时容器使用 temporary resource；
- 最终编码字节直接提交到调用方 byte buffer 或 output stream；
- `to_messagepack()` 返回 buffer 始终使用 result resource；
- writer 析构所有 frame 后，外层 scope 才允许 rewind。

### MessagePack parser

当前 parser 直接构造最终 `variant`。不能简单把整个 DOM 改到 stack resource，否则
`parse_messagepack()` 返回时会悬空。

第一版选择：

- parser 控制栈、临时 key/value 和校验状态使用 temporary resource；
- 最终 `variant` 继续使用 result resource；
- typed `read_messagepack_from()` 后续可以增加内部临时 DOM 路径，在完成目标写入后统一回滚；
- 公开 `parse_messagepack()` 返回的 DOM 不使用作用域临时资源。

## 失败与安全语义

- 分配失败沿用 CAORS memory resource 的异常/失败策略，不把已提交目标修改成半完成状态；
- rewind 必须 `noexcept`；
- scope 析构期间不执行用户回调；
- Debug 构建在 rewind 后用固定字节填充已释放区域，辅助发现 use-after-rewind；
- 不允许目标对象、返回值或借用 view 保存指向 temporary resource 的地址；
- serializer callback 可以递归进入 serialization，内层 scope 只回滚自己的分配；
- callback 如果把临时地址保存到外部，属于明确的生命周期违规，Debug 模式应尽量诊断。

## 分阶段实施

### 阶段 0：基线与契约

- [x] 记录 `monotonic_resource`、MessagePack writer/parser 和事务式容器读入的当前分配基线；
- [x] 冻结 marker 有效性、LIFO、线程归属和析构顺序契约；
- [x] 确认公开返回值与临时内存的不可逃逸边界。

当前基线：

- `monotonic_resource` 只支持整池 `reset()`，不提供嵌套 marker；
- MessagePack writer 的 frame 栈和容器暂存已使用 temporary resource；
- MessagePack parser 的流输入缓冲和 map 判型暂存使用 temporary resource，返回 DOM 递归使用
  result resource；
- typed MessagePack 当前仍先构造完整 DOM，再由 `variant_reader` 写入目标；
- 通用事务式容器元素和 schema 临时列表尚未迁移，保留为阶段 4 工作。

契约以 `stack_resource` 公共注释和 `docs/memory.md` 为准：marker 属于创建它的资源，只能在同一线程按
LIFO 回滚；资源内非平凡对象必须先析构；公开返回值、目标对象和借用 view 不得引用已回滚的临时地址。

### 阶段 1：`stack_resource`

- [x] 实现 mark/rewind/reset；
- [x] 覆盖对齐、跨 chunk、large chunk 和空回滚；
- [x] 增加错误 marker、旧 generation 和非 LIFO Debug 检查；
- [x] 验证 rewind 后外层 marker 之前的内容仍然有效。

完成标准：嵌套 scope 可以独立回滚，普通分配路径无锁。

### 阶段 2：线程局部入口与统计

- [x] 实现 `stack_scope`；
- [x] 实现 `library_temporary_resource()`；
- [x] 实现无活动 scope 时可调用的 `trim()`；
- [x] 验证多线程实例隔离和同线程递归；
- [x] 验证普通 chunk 缓存上限和 large chunk 及时释放；
- [x] 增加峰值、chunk 和 large allocation 统计。

策略资源统一采用“独立快照结构 + `statistics()` + `reset_statistics()`”风格，不引入共同统计基类：

- `stack_resource_statistics`：当前/峰值占用字节、当前/峰值 chunk 数、large allocation、
  mark、rewind、trim 和 reset 次数；
- `monotonic_resource_statistics`：当前/峰值占用字节、当前/峰值 chunk 数、独立大块分配和 reset 次数；
- `fixed_block_pool_statistics`：当前/峰值使用块数、总块数、空闲块数、池命中、扩容和拒绝请求次数。

三种资源统计分别实施。统计结构在 Debug/Release 均可用，计数使用普通整数，不增加原子操作；这些策略资源
本身不是线程安全资源。`reset_statistics()` 只重置累计计数，并把峰值重置为当前水位，不改变资源内容。
三种资源按独立批次接入和验证。

实施状态：

- [x] `stack_resource_statistics`；
- [x] `monotonic_resource_statistics`；
- [ ] `fixed_block_pool_statistics`。

完成标准：同步调用可以用一行 RAII scope 获得并安全回滚临时资源。

### 阶段 3：MessagePack writer 接入

- [x] 让 writer frame 栈和 frame 容器暂存使用 temporary resource；
- [x] 让流式 writer 在构造时绑定资源，并验证资源生命周期和线程约束；
- [x] 让 MessagePack parser 接受并绑定临时资源；
- [x] 保证最终 byte buffer 使用 result resource；
- [ ] 增加嵌套序列化 callback 测试；
- [ ] 比较接入前后的分配次数、峰值内存和吞吐。

完成标准：连续调用能够复用临时 chunk，返回 buffer 在 scope 回滚后仍然有效。

### 阶段 4：parser 与通用 serialization 接入

- [x] 让 MessagePack 返回 DOM 的字符串和嵌套容器递归使用 result resource；
- [x] MessagePack map 判型所需的中间 entry 容器使用 temporary resource；
- [ ] 评估 typed MessagePack 内部临时 DOM；
- [ ] 将事务式容器元素和 schema 临时列表迁移到 temporary resource；
- [ ] 审计 unknown fields、string storage、resolver 和构造结果，防止临时地址逃逸。

完成标准：失败路径和成功路径都不遗留临时分配，目标/返回值生命周期不受 rewind 影响。

### 阶段 5：验证与文档

- [ ] Windows Clang/MSVC Debug/Release；
- [ ] Linux Clang/GCC 和 ASan；
- [ ] 多线程、递归、深层容器和大输入压力测试；
- [x] 更新 memory 与 serialization 正式文档。

## 暂缓内容

- 跨线程移动 `stack_scope`；
- 强制回收其他线程的临时资源；
- 将全局 meta registry 或 `string_name` intern 表迁入临时资源；
- 为异步 serialization 自动延长临时 arena 生命周期；
- 替代业务层 frame allocator 或长期对象池；
- 在第一版公开复杂 chunk 缓存调参 API。

这些场景的生命周期不符合同步 LIFO 临时内存模型，应使用显式拥有型 resource 或任务级 arena。
