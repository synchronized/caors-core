<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Windows Clang 自适应 size class chunk 基准

## 环境与方法

- 日期：2026-08-03；
- 基线：提交 `89be148` 加本次自适应 chunk 改动；
- preset：`windows-clang-benchmark`；
- 编译器：Clang 22.1.8；
- 每次运行 100,000 个混合批次，共运行三次；
- workload 与前次阶段性基准相同。

默认策略以 4096 字节为目标 chunk 大小，并以 64 块为单个 chunk 的块数上限。实际块数至少为 1，
并随 size class 增大而减少。

## 结果

| 资源 | 第一次 | 第二次 | 第三次 | 中位数 |
| --- | ---: | ---: | ---: | ---: |
| 多个手工 `fixed_block_pool` | 11.86 ms | 12.07 ms | 11.47 ms | 11.86 ms |
| `std::pmr::unsynchronized_pool_resource` | 15.52 ms | 14.73 ms | 14.18 ms | 14.73 ms |
| `size_class_pool_resource` | 17.24 ms | 16.25 ms | 16.11 ms | 16.25 ms |
| `system_memory_resource` | 62.20 ms | 44.79 ms | 44.06 ms | 44.79 ms |

size class pool 相对修改前的 15.51 ms 中位数慢约 4.8%，该差异仍可能包含运行噪声；相对本轮系统资源
中位数约快 63.7%。后续应通过更多 workload 和延迟分布判断增长策略的综合收益。

## 容量与碎片

- 单批池化请求字节：1552；
- 峰值 class 容量：1648；
- 当前活跃分配内部碎片：96 字节，约 6.19%；
- 已保留 chunk 容量：19,456 字节；
- 活跃 size class：6；
- 每次正式运行的上游回退次数：100,000。

相同 workload 的保留容量从 97,280 降至 19,456 字节，减少 80%。其中大尺寸等级不再首次使用就
固定分配 64 块，解决了阶段性基准暴露的稀疏尺寸过度预留问题。

## 结论边界

- 自适应 chunk 明显改善默认策略的初次预留容量；
- 当前性能差异较小，尚不能断言由策略变化导致稳定回退；
- 尚未覆盖固定尺寸、突发峰值、低复用和长时间增长 workload；
- 尚未完成 MSVC、Linux Clang/GCC 和 ASan/UBSan 验证。
