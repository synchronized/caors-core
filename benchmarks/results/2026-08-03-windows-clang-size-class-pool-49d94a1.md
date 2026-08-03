<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Windows Clang size class pool 阶段性基准

## 环境与方法

- 日期：2026-08-03；
- 基线提交：`49d94a1`，另含尚未提交的 pool benchmark；
- preset：`windows-clang-benchmark`；
- 编译器：Clang 22.1.8；
- 每次运行 100,000 个混合批次，共运行三次；
- 每批请求：`8、24、40、72、128、256、1024、5000` 字节，使用默认对齐；
- 前七个请求进入池，5000 字节请求回退上游；每批先全部分配，再逆序全部释放。

这些数据用于阶段性方向判断，不是跨机器或跨提交的绝对性能承诺。

## 结果

| 资源 | 第一次 | 第二次 | 第三次 | 中位数 |
| --- | ---: | ---: | ---: | ---: |
| 多个手工 `fixed_block_pool` | 11.45 ms | 11.17 ms | 11.19 ms | 11.19 ms |
| `std::pmr::unsynchronized_pool_resource` | 14.37 ms | 14.38 ms | 14.56 ms | 14.38 ms |
| `size_class_pool_resource` | 16.10 ms | 15.51 ms | 15.43 ms | 15.51 ms |
| `system_memory_resource` | 45.44 ms | 44.26 ms | 44.12 ms | 44.26 ms |

本样本中，size class pool 的中位耗时约为系统资源的 35%，但比 PMR pool 慢约 7.9%，比手工固定块池
慢约 38.6%。手工池已由调用方提前知道尺寸路由，属于更专用的上界对照，不代表通用 API 应达到完全相同
成本。

## 容量与碎片

- 单批池化请求字节：1552；
- 峰值 class 容量：1648；
- 当前活跃分配内部碎片：96 字节，约 6.19%；
- 已保留 chunk 容量：97,280 字节；
- 活跃 size class：6；
- 每次正式运行的上游回退次数：100,000。

96 字节的 class 向上取整损耗可接受，但固定每 class 64 块会让低并发、尺寸分布稀疏的 workload 保留过多
容量，尤其 1024 字节 class 首次使用即保留 64 KiB。下一阶段应优先评估按 class 缩放初始 chunk 块数或
设置目标 chunk 字节数，再重复相同基准；在此之前不建议把默认策略接入其他 CAORS 组件。

## 结论边界

- 已证明当前路由和空闲链表在重复混合分配中明显减少系统分配成本；
- 尚未覆盖固定尺寸、突发高并发、低复用、多峰尺寸分布以及长时间容量增长；
- 尚未记录 P50/P95 单次延迟；
- 尚未进行 MSVC、Linux Clang/GCC、ASan/UBSan 和真实业务 workload 验证。
