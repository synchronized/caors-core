<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# CAORS 基准目录

本目录保存基准源码；目标通过 `CAORS_BUILD_BENCHMARKS=ON` 启用。
构建命令、目标、用例清单和统计范围统一由[Benchmark 指南](../docs/guides/benchmarking.md)维护。

- `resources/provider_snapshot_bench.cpp`：独立来源快照成本基准入口。
- `main.cpp`：聚合基准入口；`tasks/scheduler_bench.cpp`：独立调度器基准入口。
- `core/`、`meta/`、`math/`、`serialization/`、`resources/`、`tasks/`、`memory/`：各主题源码。
- `support/`：跨主题 fixture、输入和辅助函数。
- [results/README.md](results/README.md)：性能结果记录规则；具体测量摘要保存在同目录。

用法与行为说明不在本目录重复维护；测量结果不作为测试门禁或性能保证。
