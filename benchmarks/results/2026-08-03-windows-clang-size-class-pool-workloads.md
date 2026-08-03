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

## 固定尺寸

连续分配并立即释放 64 字节，池在计时前预热一次。

| 资源 | 第一次 | 第二次 | 第三次 | 中位数 |
| --- | ---: | ---: | ---: | ---: |
| 手工 `fixed_block_pool` | 0.805 ms | 0.780 ms | 0.800 ms | 0.800 ms |
| `std::pmr::unsynchronized_pool_resource` | 0.933 ms | 0.994 ms | 0.933 ms | 0.933 ms |
| `size_class_pool_resource` | 1.21 ms | 1.21 ms | 1.22 ms | 1.21 ms |
| `system_memory_resource` | 5.20 ms | 5.20 ms | 5.30 ms | 5.20 ms |

size class pool 约为系统资源耗时的 23.3%，但比 PMR pool 慢约 29.7%。手工固定块池不需要通用
尺寸路由，仍是该场景的最快对照。

## 突发峰值

每批同时持有 128 个混合尺寸对象，再逆序释放；共执行 781 批。尺寸中包含 5000 字节请求，按设计
回退到上游资源。

| 资源 | 第一次 | 第二次 | 第三次 | 中位数 |
| --- | ---: | ---: | ---: | ---: |
| `std::pmr::unsynchronized_pool_resource` | 2.29 ms | 2.30 ms | 2.38 ms | 2.30 ms |
| `size_class_pool_resource` | 2.43 ms | 2.42 ms | 2.42 ms | 2.42 ms |
| `system_memory_resource` | 6.99 ms | 7.44 ms | 7.37 ms | 7.37 ms |

size class pool 约为系统资源耗时的 32.8%，比 PMR pool 慢约 5.2%。池化请求的峰值容量为
26,368 字节，最终保留 31,744 字节，共 9 个 chunk；上游回退 12,496 次。

## 低复用

每轮新建资源，每种混合尺寸只分配和释放一次，然后立即销毁资源；共执行 10,000 轮。该场景刻意
计入资源初始化、首次 chunk 分配和销毁成本。

| 资源 | 第一次 | 第二次 | 第三次 | 中位数 |
| --- | ---: | ---: | ---: | ---: |
| `system_memory_resource` | 4.33 ms | 4.23 ms | 4.22 ms | 4.23 ms |
| `std::pmr::unsynchronized_pool_resource` | 10.49 ms | 11.10 ms | 10.68 ms | 10.68 ms |
| `size_class_pool_resource` | 13.58 ms | 13.39 ms | 13.10 ms | 13.39 ms |

size class pool 约为系统资源耗时的 3.17 倍，PMR pool 也约为系统资源的 2.52 倍。这验证了池化资源
只适合有足够复用或突发批量分配的生命周期；一次性、短生命周期任务应直接使用上游资源。

## 结论边界

- 固定尺寸和突发峰值 workload 均证明 size class pool 相比系统分配有明显收益；
- 低复用 workload 明确给出了不应使用池的反例；
- 当前只记录批量总耗时，尚未记录 P50/P95 单次延迟；
- 尚未覆盖长时间容量增长、真实业务 workload 和其他编译器、平台。
