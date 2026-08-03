<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Windows Clang size class pool workload 基准

## 环境与方法

- 日期：2026-08-03；
- 基线：提交 `614aabd` 加本次 workload 改动；
- preset：`windows-clang-benchmark`；
- 编译器：Clang 22.1.8；
- 每种场景执行约 100,000 次分配，完整程序重复运行三次；
- 结果是批量操作总耗时，用于同轮横向比较，不代表单次调用延迟。

吞吐按一次分配或一次释放各计一个操作，由中位总耗时换算。

## 固定尺寸

连续分配并立即释放 64 字节，池在计时前预热一次。

| 资源 | 第一次 | 第二次 | 第三次 | 中位数 | 中位吞吐 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 手工 `fixed_block_pool` | 0.805 ms | 0.780 ms | 0.800 ms | 0.800 ms | 250.0 M ops/s |
| `std::pmr::unsynchronized_pool_resource` | 0.933 ms | 0.994 ms | 0.933 ms | 0.933 ms | 214.4 M ops/s |
| `size_class_pool_resource` | 1.21 ms | 1.21 ms | 1.22 ms | 1.21 ms | 165.3 M ops/s |
| `system_memory_resource` | 5.20 ms | 5.20 ms | 5.30 ms | 5.20 ms | 38.5 M ops/s |

size class pool 约为系统资源耗时的 23.3%，但比 PMR pool 慢约 29.7%。手工固定块池不需要通用
尺寸路由，仍是该场景的最快对照。

## 突发峰值

每批同时持有 128 个混合尺寸对象，再逆序释放；共执行 781 批。尺寸中包含 5000 字节请求，按设计
回退到上游资源。

| 资源 | 第一次 | 第二次 | 第三次 | 中位数 | 中位吞吐 |
| --- | ---: | ---: | ---: | ---: | ---: |
| `std::pmr::unsynchronized_pool_resource` | 2.29 ms | 2.30 ms | 2.38 ms | 2.30 ms | 86.9 M ops/s |
| `size_class_pool_resource` | 2.43 ms | 2.42 ms | 2.42 ms | 2.42 ms | 82.6 M ops/s |
| `system_memory_resource` | 6.99 ms | 7.44 ms | 7.37 ms | 7.37 ms | 27.1 M ops/s |

size class pool 约为系统资源耗时的 32.8%，比 PMR pool 慢约 5.2%。池化请求的峰值容量为
26,368 字节，最终保留 31,744 字节，共 9 个 chunk；上游回退 12,496 次。

## 低复用

每轮新建资源，每种混合尺寸只分配和释放一次，然后立即销毁资源；共执行 10,000 轮。该场景刻意
计入资源初始化、首次 chunk 分配和销毁成本。

| 资源 | 第一次 | 第二次 | 第三次 | 中位数 | 中位吞吐 |
| --- | ---: | ---: | ---: | ---: | ---: |
| `system_memory_resource` | 4.33 ms | 4.23 ms | 4.22 ms | 4.23 ms | 37.8 M ops/s |
| `std::pmr::unsynchronized_pool_resource` | 10.49 ms | 11.10 ms | 10.68 ms | 10.68 ms | 15.0 M ops/s |
| `size_class_pool_resource` | 13.58 ms | 13.39 ms | 13.10 ms | 13.39 ms | 11.9 M ops/s |

size class pool 约为系统资源耗时的 3.17 倍，PMR pool 也约为系统资源的 2.52 倍。这验证了池化资源
只适合有足够复用或突发批量分配的生命周期；一次性、短生命周期任务应直接使用上游资源。

## 分配延迟分布

Windows 上单次快速分配短于 `steady_clock` 的有效量化粒度，直接逐调用计时会产生 0 ns 样本。因此
每个样本连续执行 16 次分配，再将批次耗时除以 16，得到单次分配平均值。每轮采集 512 个批次，
共 8192 次混合尺寸分配；释放不计入采样区间。下表对三轮各项结果再次取中位数：

| 资源 | P50 | P95 | 最大批次折算值 |
| --- | ---: | ---: | ---: |
| `system_memory_resource` | 37 ns | 2125 ns | 5368 ns |
| `size_class_pool_resource` | 18 ns | 2168 ns | 5412 ns |
| `std::pmr::unsynchronized_pool_resource` | 87 ns | 981 ns | 6131 ns |

size class pool 的常规空闲链表路径具有最低 P50；其 P95 接近系统资源，原因是该冷启动突发样本同时
包含大尺寸等级的多次 chunk 扩容和 5000 字节请求的上游回退。PMR 的 P50 较高，但当前增长策略的
P95 更低。后续可评估渐进式 chunk 增长，在容量和尾延迟之间取得更好的平衡。

### 渐进式增长实验

曾实验让同一 class 的后续 chunk 按 `4、8、16、32、64` 块渐进增长。三轮结果中，size class pool
的 P95 中位数从 2168 ns 降至 2012 ns，改善约 7.2%；但突发保留容量从 31,744 增至 44,032 字节，
增加约 38.7%，突发中位耗时也从 2.42 ms 增至 2.54 ms，慢约 5%。该方案的容量和吞吐代价高于有限
的尾延迟收益，因此未纳入实现，继续使用按 class 固定目标 chunk 容量的策略。

## 结论边界

- 固定尺寸和突发峰值 workload 均证明 size class pool 相比系统分配有明显收益；
- 低复用 workload 明确给出了不应使用池的反例；
- P50/P95 是 16 次分配批次的单次折算值，受时钟、调度和批内扩容影响，不是硬件级逐调用测量；
- 尚未覆盖长时间容量增长、真实业务 workload 和其他编译器、平台。
