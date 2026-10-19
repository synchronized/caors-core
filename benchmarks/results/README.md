<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Benchmark Results

本目录存放人工整理后的 release benchmark 摘要，用于观察同一环境下不同提交之间的性能趋势。

## 记录规则

- 只提交摘要，不提交完整 `ctrack` 控制台输出。
- 每份结果必须记录提交号、preset、编译器、系统、CPU、迭代次数和运行次数。
- 默认连续运行 3 次，表格记录三次结果和中位数。
- 表格中的 `median/iter` 使用中位数除以迭代次数，仅用于同一用例内部比较。
- 不同机器、不同编译器、不同 preset、不同迭代次数的结果不能直接比较。
- benchmark 结果不是测试门禁，只用于趋势分析和定位明显回归。

## 构建与测量入口

目标、构建命令、用例与统计范围统一见[Benchmark 指南](../../docs/benchmarking.md)。
本页维护结果格式与索引，不维护第二份运行指南。

## 测量记录

以下是历史测量，不构成当前提交的性能保证；比较前核对每份记录中的提交、环境和方法。

- [2026-10-19-windows-clang-task-scheduler](2026-10-19-windows-clang-task-scheduler.md)
- [2026-08-07-windows-clang-task-coroutines](2026-08-07-windows-clang-task-coroutines.md)
- [2026-08-07-windows-clang-resource-coroutines](2026-08-07-windows-clang-resource-coroutines.md)
- [2026-08-05-windows-clang-registry-resources-a4f5ba1](2026-08-05-windows-clang-registry-resources-a4f5ba1.md)
- [2026-08-05-memory-domain-observability](2026-08-05-memory-domain-observability.md)
- [2026-08-04-windows-clang-small-vector](2026-08-04-windows-clang-small-vector.md)
- [2026-08-03-windows-clang-size-class-pool-workloads](2026-08-03-windows-clang-size-class-pool-workloads.md)
- [2026-08-03-windows-clang-size-class-pool-adaptive](2026-08-03-windows-clang-size-class-pool-adaptive.md)
- [2026-08-03-windows-clang-size-class-pool-49d94a1](2026-08-03-windows-clang-size-class-pool-49d94a1.md)
- [2026-07-31-windows-clang-meta-wrapper-22497cb](2026-07-31-windows-clang-meta-wrapper-22497cb.md)
- [2026-07-29-windows-clang-messagepack-baseline](2026-07-29-windows-clang-messagepack-baseline.md)
- [2026-07-27-windows-clang-benchmark-7015a59](2026-07-27-windows-clang-benchmark-7015a59.md)
- [2026-07-23-windows-clang-benchmark-f4f2bba](2026-07-23-windows-clang-benchmark-f4f2bba.md)
- [2026-07-23-windows-clang-benchmark-a83c174](2026-07-23-windows-clang-benchmark-a83c174.md)
- [2026-07-21-windows-clang-benchmark-ec11212](2026-07-21-windows-clang-benchmark-ec11212.md)
- [2026-07-21-windows-clang-benchmark-4f5a18c](2026-07-21-windows-clang-benchmark-4f5a18c.md)
- [2026-07-21-windows-clang-benchmark-096e064](2026-07-21-windows-clang-benchmark-096e064.md)
