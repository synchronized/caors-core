<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 分级 size class 内存池计划

## 文档状态

- 当前状态：核心实现与 Windows Clang 定向验证进行中；
- 目标：为中小型、频繁分配提供一个按尺寸等级复用内存块的通用 `memory_resource`；
- 首批范围：非线程安全资源、确定性尺寸路由、大块请求回退、统计、边界测试和基准；
- 非目标：首批不提供跨线程无锁池、不替换系统默认资源、不自动回收仍含存活分配的 chunk、
  不引入第三方分配器，也不把该类型加入稳定插件 C ABI。

## 背景

当前 `fixed_block_pool` 只服务单一块尺寸，适合节点类型或固定对象池；普通容器和运行期对象会产生多种
大小的分配，调用方若手工组合多个固定块池，还需要自行实现路由、回退和统计。

新资源负责把请求映射到一组有限的 size class。小请求从对应空闲链表复用块，超过池化范围或无法由
池满足的对齐请求直接交给上游资源。它是 `fixed_block_pool` 之上的通用策略补充，不替代
`monotonic_resource`、`stack_resource` 或调试资源。

## 对外结构

首版暂定新增独立头文件 `caors/memory/size_class_pool.h`，并由 `caors/memory.h` 聚合：

```cpp
namespace caors::memory {

struct size_class_pool_options {
  std::size_t smallest_block_size = 16;
  std::size_t largest_block_size = 4096;
  std::size_t blocks_per_chunk = 64;
  std::size_t target_chunk_size = 4096;
  std::size_t pooled_alignment = default_memory_alignment;
};

struct size_class_pool_statistics {
  std::size_t pooled_allocation_count = 0;
  std::size_t pooled_deallocation_count = 0;
  std::size_t fallback_allocation_count = 0;
  std::size_t fallback_deallocation_count = 0;
  std::size_t chunk_allocation_count = 0;
  std::size_t allocation_failure_count = 0;
  std::size_t chunk_count = 0;
  std::size_t pooled_bytes_in_use = 0;
  std::size_t peak_pooled_bytes_in_use = 0;
  std::size_t reserved_bytes = 0;
};

class size_class_pool_resource final : public memory_resource {
public:
  explicit size_class_pool_resource(memory_resource* upstream = nullptr);
  size_class_pool_resource(size_class_pool_options options,
                           memory_resource* upstream = nullptr);
  ~size_class_pool_resource() override;

  [[nodiscard]] memory_resource* upstream_resource() const noexcept;
  [[nodiscard]] const size_class_pool_options& options() const noexcept;
  [[nodiscard]] size_class_pool_statistics statistics() const noexcept;
  void reset_statistics() noexcept;
  void release() noexcept;
};

} // namespace caors::memory
```

具体字段在实现前仍可依据布局验证微调，但首版必须保持配置为值语义、资源非拥有上游、资源自身不可复制。

## 路由规则

### 尺寸等级

首版使用二次幂等级：`16、32、64……4096`。配置必须满足：

- `smallest_block_size` 和 `largest_block_size` 均为非零二次幂；
- 最小值不大于最大值；
- `blocks_per_chunk` 非零；
- `target_chunk_size` 非零；实际 chunk 块数不超过 `blocks_per_chunk`，并按 class 尺寸缩放；
- `pooled_alignment` 为合法二次幂；
- 每个块至少能保存内部空闲链表节点。

请求大小先沿用 `memory_resource` 的零大小标准化，再向上取整到第一个可容纳它的等级。路由只由分配时
传入的 `size` 和 `alignment` 决定，不为每个分配额外保存 class header。

### 对齐与上游回退

- 当 `size <= largest_block_size` 且 `alignment <= pooled_alignment` 时进入池；
- 大于最大池化尺寸或要求更高对齐时，原样转发给上游；
- 池内 chunk 使用 `pooled_alignment` 分配，各 block stride 同时满足块尺寸和该对齐；
- 上游失败时 `try_allocate()` 返回 `nullptr`，`allocate()` 继续遵循现有统一失败语义；
- 上游回退不是错误，也不计入 rejected request。

释放仍要求调用方传回与分配时一致的大小和对齐。资源据此重新计算同一条路由；错误的释放参数属于整个
`memory_resource` 契约之外的未定义用法，应通过外层 `debug_tracking_resource` 检测，而不让发布版池为
每个分配承担额外元数据成本。

## 内部结构

实现使用 `.cpp` 内 opaque state，避免在公共 ABI 中暴露 class 数量、chunk 和空闲链表布局：

```text
size_class_pool_resource
  -> state
       -> class[16]   -> chunks + free list
       -> class[32]   -> chunks + free list
       -> ...
       -> class[4096] -> chunks + free list
       -> upstream fallback
```

内部算法可以复用 `fixed_block_pool` 的溢出检查、chunk 构造和空闲链表思路，但不直接把一组公共
`fixed_block_pool` 对象作为 ABI 布局。共享逻辑达到足够规模时再提取到 `src/caors/memory/detail/`，避免
为了复用提前扩大 detail API。

## 生命周期与线程安全

- 首版与当前 `fixed_block_pool` 一样，不保证并发调用安全；
- 推荐每线程、每任务域或由外层同步保护后使用；
- 不在热路径中加入隐藏 mutex 或原子计数；
- 后续确有真实跨线程 workload 时，再评估独立 `synchronized_size_class_pool_resource`，不通过运行期
  布尔选项让同一类型承担两套性能模型；
- 销毁或 `release()` 前必须释放全部池内分配；否则属于调用方生命周期错误；
- `release()` 归还所有 chunk，但不释放由上游回退产生且仍存活的分配；这些分配仍须由调用方正常释放；
- 插件卸载前必须销毁所有引用插件内资源或仍由插件代码访问的池实例和分配结果。

## 与现有资源的组合

- `tracking_resource -> size_class_pool_resource -> system`：统计调用方请求，并获得池化效果；
- `debug_tracking_resource -> size_class_pool_resource -> system`：检测调用方越界和释放参数错误；
- `size_class_pool_resource -> tracking_resource -> system`：观察池向上游申请 chunk 和大块回退；
- `pmr_resource_view -> size_class_pool_resource`：供标准 PMR 容器使用；
- 不建议把 `monotonic_resource` 或 `stack_resource` 作为长期池上游，因为它们的逐块释放语义不同。

统计必须明确区分“调用请求”和“上游 chunk/回退请求”。`size_class_pool_statistics` 只描述池自身看到的
请求与保留容量；需要观察真实上游调用时继续组合 `tracking_resource`。

## 与常见方案的取舍

### 相对 `std::pmr::unsynchronized_pool_resource`

优点：

- 保留 CAORS `try_allocate()` 无异常入口；
- 配置、统计和外层调试资源与现有内存模块一致；
- 不要求业务容器迁移为 PMR 类型。

缺点：

- 首版策略更简单，暂不提供标准 PMR 实现可能具有的调优和成熟度；
- 需要 CAORS 自己维护跨平台测试和碎片率基准。

### 相对 jemalloc、mimalloc、rpmalloc

该资源规模更小、可组合、适合局部子系统，但不试图成为进程级通用分配器。大型项目仍可通过
`callback_memory_resource` 接入成熟第三方分配器；只有基准证明局部 size class 池有稳定收益时，才推荐
在热点模块启用。

## 风险与控制

1. **内部碎片**：向上取整会浪费块尾空间。统计请求大小与实际 class 容量，并以基准记录碎片率。
2. **保留内存增长**：峰值后 chunk 默认保留到 `release()`。首版提供显式释放，不在热路径自动 trim。
3. **错误释放路由**：错误的 size/alignment 可能污染错误 class。文档强化契约，并用调试资源组合验证。
4. **过度池化**：低复用 workload 可能比系统分配更差。必须与 system、fixed block 和 PMR pool 对照。
5. **线程误用**：无锁资源被跨线程访问会产生数据竞争。公共注释和文档明确非线程安全。
6. **统计溢出或口径混淆**：计数采用饱和或明确的自然溢出策略，并固定调用层与上游层统计定义。

## 实施阶段

### 阶段 0：冻结接口与契约

- [x] 确认首版为非线程安全资源；
- [x] 确认二次幂 size class 和确定性释放路由；
- [x] 确认大块及过度对齐请求回退上游；
- [x] 确认不保存逐分配 header；
- [x] 实施前复核默认最大尺寸和默认每 chunk 块数。

### 阶段 1：核心实现

- [x] 新增公共 options、statistics 和 `size_class_pool_resource`；
- [x] 新增 opaque state、class table、chunk 与空闲链表；
- [x] 实现配置校验、尺寸路由、溢出安全和失败回滚；
- [x] 实现上游回退、统计重置和 `release()`；
- [x] 接入 `caors/memory.h` 与 CMake 源文件。

### 阶段 2：正确性测试

- [x] 覆盖各等级边界、零大小和默认对齐；
- [x] 覆盖大块、过度对齐回退及释放配对；
- [x] 覆盖块复用、多个 chunk、上游失败和算术溢出；
- [x] 覆盖统计、reset、release 和资源析构；
- [x] 通过外层 `debug_tracking_resource` 验证错误释放诊断组合；
- [x] 验证 Static/Shared 定向构建和公共头自包含。

### 阶段 3：性能与碎片率验证

- [ ] 增加固定尺寸、混合尺寸、突发峰值和低复用 workload；
- [x] 对比 system resource、多个手工 fixed block pool、`std::pmr::unsynchronized_pool_resource`；
- [ ] 记录吞吐、P50/P95 延迟、上游调用次数、峰值保留容量和内部碎片率；
- [ ] 只有在真实 workload 获得稳定收益后，才在其他 CAORS 组件中采用。

### 阶段 4：跨平台验证与文档收尾

- [ ] Windows Clang/MSVC Debug、Release、Static/Shared；
- [ ] Linux Clang/GCC Debug、Release；
- [ ] Linux ASan/UBSan 压力验证；
- [ ] 更新 `docs/memory.md`、`docs/roadmap.md` 和计划状态索引。

## 完成标准

- 公共 API 的所有权、线程安全、失败和释放契约完整；
- 所有尺寸与对齐边界均有自动化测试；
- 池化与上游回退统计可解释、可复现；
- Static/Shared 和跨平台验证通过；
- 基准证明至少一个真实或代表性 workload 中优于直接系统分配，且碎片率处于可接受范围。

## 当前验证记录

- 2026-08-03：`windows-clang-debug` 定向构建与 `caors.memory.size_class_pool` 通过；
- 2026-08-03：`windows-clang-shared-debug` 定向构建与 `caors.memory.size_class_pool` 通过；
- 2026-08-03：`windows-clang-debug` 全量构建与 83/83 测试通过，包含公共头自包含；
- 2026-08-03：`windows-clang-benchmark` Release 构建通过；10 万混合批次重复三次，size class pool
  中位耗时 15.51 ms，system resource 44.26 ms，PMR pool 14.38 ms，手工 fixed pools
  11.19 ms；详细记录见
  `benchmarks/results/2026-08-03-windows-clang-size-class-pool-49d94a1.md`；
- 2026-08-03：默认 chunk 改为以 4096 字节为目标、64 块为上限的自适应容量；相同 workload
  的保留容量由 97,280 降至 19,456 字节，减少 80%，三轮中位耗时为 16.25 ms；详细记录见
  `benchmarks/results/2026-08-03-windows-clang-size-class-pool-adaptive.md`；
- 当前构建目录的 Ninja 日志曾被前台桥接超时截断，后续构建会自动恢复并产生额外重编译；这不属于
  源码或测试失败；
- Windows Shared 全量、MSVC、更多 workload、正式延迟分布和 Linux 验证仍未执行。
