<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Benchmark Result - 2026-07-23

## Environment

- commit: `a83c174`
- preset: `windows-clang-benchmark`
- target: `caors_benchmarks`
- iterations: `100000`
- runs: `3`
- compiler: `clang version 22.1.8`
- OS: `Microsoft Windows 10 专业版 10.0.19045`
- CPU: `Intel(R) Core(TM) i5-8600K CPU @ 3.60GHz`
- cores / logical processors: `6 / 6`
- raw output: not committed

## Meta Type Data Scope

`time acc` 是每个用例执行 `100000` 次的总耗时；`median/iter` 是三次运行中位数除以迭代次数。
本次只整理 `meta-type-data-redesign.md` 阶段 5 直接要求的类型查询、属性查询和序列化调度路径。

| case | run1 | run2 | run3 | median | median/iter |
| --- | ---: | ---: | ---: | ---: | ---: |
| `meta.resolve` | 2.90 ms | 2.88 ms | 2.96 ms | 2.90 ms | 29.00 ns |
| `meta.find_registered` | 2.63 ms | 2.64 ms | 2.56 ms | 2.63 ms | 26.30 ns |
| `meta.property.member_get_set_typed` | 31.71 ms | 31.36 ms | 31.31 ms | 31.36 ms | 313.60 ns |
| `meta.instance.get_set_value` | 93.97 ms | 94.61 ms | 94.68 ms | 94.61 ms | 946.10 ns |
| `serialization.schema_view_find` | 801.20 us | 803.50 us | 843.20 us | 803.50 us | 8.04 ns |
| `serialization.schema_view_property_lookup` | 28.92 ms | 29.03 ms | 28.70 ms | 28.92 ms | 289.20 ns |
| `serialization.to_value` | 251.65 ms | 262.33 ms | 254.29 ms | 254.29 ms | 2.54 us |
| `serialization.read_json_from_text_reader` | 343.02 ms | 340.16 ms | 343.42 ms | 343.02 ms | 3.43 us |

## Comparison

与同机、同编译器、同迭代次数的 `ec11212` 基线比较：

- `meta.resolve`：`28.50 ns` -> `29.00 ns`，基本持平；
- `meta.find_registered`：`25.90 ns` -> `26.30 ns`，基本持平；
- `meta.property.member_get_set_typed`：`331.50 ns` -> `313.60 ns`，略有改善；
- `meta.instance.get_set_value`：`934.70 ns` -> `946.10 ns`，基本持平；
- `serialization.to_value`：`663.10 ns` -> `2.54 us`，存在明显回退；
- `serialization.read_json_from_text_reader`：`1.54 us` -> `3.43 us`，存在明显回退。

`serialization.schema_view_find` 与 `096e064` 基线的 `8.47 ns` 基本持平。
`serialization.schema_view_property_lookup` 在旧摘要中没有同口径记录。

## Conclusion

统一 `type_data` 后，`resolve/find` 和属性查询热路径没有观察到明显回退。完整序列化路径已经包含此后
多个 serialization 实现变化，当前数据不能直接把回退归因于静态类型数据重构；需要通过提交区间对比和
拆分 benchmark 继续定位。阶段 5 的性能工作因此保持“进行中”，不宣称已经满足整体性能完成标准。
