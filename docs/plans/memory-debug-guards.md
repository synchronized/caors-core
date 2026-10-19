<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 内存 Guard 与填充调试能力开发计划

## 文档状态

- 当前状态：核心实施与 Windows、Linux Clang/GCC、ASan 验证均已完成；
- 目标：扩展 `debug_tracking_resource`，检测用户分配前后越界、释放参数不匹配、重复或非法释放，
  并通过可配置字节模式辅助发现未初始化读取和释放后使用；
- 首批范围：guard block、分配/释放填充、违规统计、可选诊断回调、对齐和溢出安全；
- 非目标：不实现通用 AddressSanitizer、不拦截全局 `new/delete`、不隔离已释放虚拟页、不提供跨进程
  崩溃转储，也不让普通 `memory_resource` 默认承担调试开销。

## 背景

当前 `debug_tracking_resource` 已经维护存活分配表，并能检测：

- 泄漏；
- 非法或重复释放；
- 释放时传入的大小或对齐不匹配。

但当前上游只分配用户请求的有效载荷，无法判断写入是否越过前后边界，也不会在分配和释放时覆盖
内存。因此许多错误只能表现为后续随机损坏，诊断位置离真正写坏内存的位置较远。

本计划继续把能力放在显式使用的 `debug_tracking_resource` 中。普通 `system_memory_resource`、
`tracking_resource` 和 Release 热路径不增加字段、分支或额外分配。

## 设计原则

- **显式启用**：只有构造并使用 `debug_tracking_resource` 才承担调试成本；
- **保守诊断**：发现 guard 损坏时记录并报告，但仍按内部保存的原始信息安全释放上游块；
- **物理信息与公开信息分离**：公共记录描述用户指针，内部记录保存上游原始块；
- **不信任释放参数**：释放始终使用分配时记录的原始指针、大小和对齐；
- **不掩盖主错误**：诊断回调不得改变释放流程，不允许异常穿过 `deallocate() noexcept`；
- **线程安全不扩大**：只保证调试记录和统计自身无数据竞争，不让底层非线程安全资源变安全。

## 建议公共 API

### 调试选项

```cpp
struct memory_debug_options {
  std::size_t guard_size = 16;
  std::byte front_guard_pattern{0xF1};
  std::byte back_guard_pattern{0xF2};
  std::byte allocation_fill_pattern{0xCD};
  std::byte deallocation_fill_pattern{0xDD};
  bool fill_on_allocate = true;
  bool fill_on_deallocate = true;
};
```

规则：

- `guard_size == 0` 时关闭前后 guard，但填充能力仍可独立使用；
- 首批不设置全局默认选项，每个 `debug_tracking_resource` 独立保存一份；
- pattern 使用 `std::byte`，避免字符有符号性；
- options 在构造后不可修改，避免已有分配使用旧布局、新释放使用新布局；
- 不增加 `#ifdef DEBUG` 隐式行为，Debug/Release 语义一致且可测试。

保留现有构造符号，并增加选项重载，避免把已有单参数构造改成新的 ABI：

```cpp
explicit debug_tracking_resource(memory_resource* upstream = nullptr);
debug_tracking_resource(memory_resource* upstream,
                        memory_debug_options options);
```

单参数构造委托给默认 options，保持源码和二进制接口边界清晰。

### 违规类型与诊断回调

```cpp
enum class memory_debug_violation_kind {
  invalid_deallocation,
  mismatched_deallocation,
  front_guard_corruption,
  back_guard_corruption,
};

struct memory_debug_violation {
  memory_debug_violation_kind kind;
  memory_debug_record allocation;
  std::size_t supplied_size = 0;
  std::size_t supplied_alignment = 0;
};
```

`debug_tracking_resource` 增加可选回调：

```cpp
using violation_fn = void (*)(const memory_debug_violation&, void*) noexcept;

void set_violation_callback(violation_fn callback,
                            void* user_data = nullptr) noexcept;
```

回调规则：

- 默认回调为空，只更新统计，不向 `stderr` 输出，也不主动断言；
- 回调在内部记录锁之外调用，允许日志系统消费事件，避免锁内重入死锁；
- 回调必须 `noexcept`，不得保存其中借用的临时对象；
- invalid deallocation 没有可信分配记录，事件只保证提供调用方传入的指针、大小和对齐；
- 首批每次释放最多分别报告一次前 guard 和一次后 guard 损坏。

### 统计扩展

`memory_debug_statistics` 增加：

```cpp
std::size_t front_guard_corruption_count = 0;
std::size_t back_guard_corruption_count = 0;
```

原有 `invalid_deallocation_count` 和 `mismatched_deallocation_count` 保持语义不变。guard 损坏不会计入
参数不匹配，便于分别观察调用协议错误和内存写越界。

## 内存布局

用户请求 `size`、`alignment`，guard 长度为 `G` 时，上游分配布局为：

```text
raw_ptr
  ├─ alignment padding（0 到 alignment - 1 字节）
  ├─ front guard（G 字节）
  ├─ user payload（标准化后的 size 字节，user_ptr 满足 alignment）
  └─ back guard（G 字节）
```

上游请求大小：

```text
raw_size = normalized_size + 2 * G + alignment - 1
```

所有加法和乘法必须先做溢出检查。溢出时 `try_allocate()` 返回 `nullptr`，`allocate()` 按现有失败
策略抛 `std::bad_alloc` 或 fail-fast。

`G == 0` 时必须走直接转发特例：`raw_size == normalized_size`、`user_ptr == raw_ptr`，不增加
alignment padding。启用 guard 时，原始块使用用户请求的 alignment 向上游分配，并在块内对
`front guard + user payload` 的边界做对齐计算。

不能把 `user_ptr` 交给上游释放。内部记录必须额外保存：

```cpp
struct debug_allocation_record {
  memory_debug_record public_record;
  void* raw_ptr;
  std::size_t raw_size;
  std::size_t raw_alignment;
};
```

公共 `for_each_live_allocation()` 和 `collect_live_allocations()` 仍只暴露用户指针、用户大小和用户
对齐，不泄漏 guard/padding 的实现布局。

## 分配流程

1. 校验 alignment 为 2 的幂，并标准化零尺寸；
2. 检查 `raw_size` 计算是否溢出；
3. 向上游申请原始块；
4. 在原始块内计算满足 alignment 的 `user_ptr`；
5. 写入前后 guard pattern；
6. 如启用，使用 allocation pattern 填充用户 payload；
7. 将公开记录和原始块信息一起插入存活表；
8. 插表失败时用原始指针和原始参数释放上游块；
9. 统计中的 bytes 继续按用户请求大小计算，不把 guard/padding 计入用户在用字节。

## 释放流程

1. 在锁内查找并移除内部记录；
2. 找不到时累计 invalid deallocation，并在锁外报告；
3. 比较调用方 size/alignment 与公开记录，累计 mismatched deallocation；
4. 检查前后 guard，分别累计损坏次数并生成事件；
5. 如启用，用 deallocation pattern 覆盖用户 payload；
6. 使用 `raw_ptr/raw_size/raw_alignment` 释放上游块；
7. 更新释放和在用字节统计；
8. 在锁外依次调用诊断回调。

发现 guard 损坏后仍然释放，因为原始上游参数保存在独立记录中，不依赖可能被用户破坏的前缀
header。首批不把内部 metadata 放在用户块里，避免越界同时破坏释放所需信息。

## 线程安全与生命周期

- 存活表继续由 mutex 保护，统计继续使用 atomic；
- guard 检查发生在记录已从表中移除、上游释放之前；
- 调用方必须保证没有其他线程在释放期间继续访问同一用户块；
- options 在构造时固定，不需要运行期同步；
- 回调指针和 user data 需要一起原子快照或在 mutex 下读取，调用发生在锁外；
- 销毁 `debug_tracking_resource` 前仍必须结束所有并发分配和释放；
- 资源析构不自动释放存活用户块，继续保持“报告泄漏但不猜测对象生命周期”的现有边界。

## 与 ASan 等工具的关系

该能力不能替代 ASan：

- guard 只在释放或显式检查时发现损坏，ASan 通常能在越界写发生处立即报告；
- guard 无法可靠检测任意 use-after-free，填充值只能让错误更容易暴露；
- guard 可以在不支持 ASan 的构建、插件宿主和长期运行诊断中使用；
- CAORS 记录包含 label 和 source location，能提供业务级分配来源。

两者适合同时使用。CAORS guard 位于真实上游分配块内部，不应触碰上游块之外的 ASan redzone。

## 实施阶段

### 阶段 0：冻结契约

- [x] 明确首批只扩展显式 `debug_tracking_resource`；
- [x] 明确公开记录与内部物理记录分离；
- [x] 明确 guard 损坏后仍按记录安全释放；
- [x] 明确统计、回调、线程安全和 ASan 边界。

### 阶段 1：布局与填充

- [x] 增加 `memory_debug_options`；
- [x] 实现溢出安全的 raw block 大小和对齐计算；
- [x] 实现前后 guard 和分配/释放填充；
- [x] 保持公开 live allocation 记录语义不变。

### 阶段 2：违规报告

- [x] 增加 guard 损坏统计；
- [x] 增加非拥有、`noexcept` 诊断回调；
- [x] 确保回调始终在内部锁外执行；
- [x] 文档化 invalid、mismatch 与 guard corruption 的独立含义。

### 阶段 3：测试与文档

- [x] 覆盖前越界和后越界；
- [x] 覆盖 allocation/deallocation fill；
- [x] 覆盖零尺寸、扩展对齐、布局溢出和上游失败；
- [x] 覆盖回调顺序、空回调和锁外重入；
- [x] 覆盖 guard 关闭的直接布局及独立填充；
- [x] 运行 Windows Static 全量和 Shared 专项测试；
- [x] 更新 `docs/reference/memory.md`、roadmap 和计划状态；
- [x] 使用 GitHub Actions 完成 Linux Clang/GCC/ASan 完整验证。

实施结果：

- `debug_tracking_resource` 默认使用 16 字节前后 guard，并支持独立关闭 guard 和两类填充；
- 内部 `debug_allocation_record` 保存用户记录及上游 raw pointer/size/alignment，guard 损坏不影响安全
  释放；
- 前越界、后越界、释放参数不匹配和非法释放分别统计，并可通过锁外 `noexcept` 回调报告；
- `guard_size == 0` 直接向上游转发用户布局，不保留 guard 或 alignment padding；
- `windows-clang-debug` 完整构建成功，82 项 CTest 全部通过；
- `windows-clang-shared-debug` 成功构建 CAORS DLL 和 debug tracking 测试目标，专项测试通过；
- GitHub Actions Linux 工作流 `30882960581` 完成 Clang/GCC Static Debug、Release，Clang Shared
  Debug、Release 及 Clang Shared ASan 验证，全部通过。

## 风险

- **内存开销**：每个分配增加两段 guard、最多 `alignment - 1` padding 和一条更大的内部记录；该资源
  只用于显式调试路径。
- **性能开销**：分配和释放增加填充与逐字节 guard 检查，复杂度为 `O(size + guard_size)`；允许分别
  关闭填充或将 guard 设为零。
- **超大对齐放大**：raw block 需要额外预留 alignment padding，极端 alignment 会显著增加请求；必须
  做溢出检查并在文档中说明。
- **回调重入**：锁内调用日志或再次分配会死锁；实现必须先生成事件快照、释放锁，再调用回调。
- **损坏范围有限**：跨过整个 guard 的大越界可能写入另一合法区域，不能保证检测所有内存错误。
- **释放后使用检测有限**：内存交还上游后可能立即复用，deallocation fill 只是诊断提示，不提供隔离。
- **误用资源生命周期**：guard 不解决 allocator 保存悬空资源指针的问题，调用方仍需保证资源活得足够久。

## 完成标准

- 正常分配路径保持用户 size/alignment 契约；
- 前后各一字节越界能在释放时被独立统计和报告；
- 上游释放始终收到原始指针、原始大小和原始对齐；
- 关闭 guard 后不保留 guard/padding 开销，关闭填充后不扫描完整用户 payload；
- 诊断回调不在锁内执行且不能让异常穿过 `deallocate() noexcept`；
- 现有 debug tracking API 和测试保持兼容；
- Windows Static/Shared 与 Linux Clang/GCC/ASan 验证通过。
