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

## 建议命令

```sh
cmake --preset windows-clang-benchmark
cmake --build --preset windows-clang-benchmark --target caors_benchmarks
build/windows-clang-benchmark/benchmarks/caors_benchmarks 100000
```

同一提交建议运行 3 次，取中位数。
