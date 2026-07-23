<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Benchmark Result - 2026-07-23

## Environment

- commit: `f4f2bba`
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
本次验证序列化字段运行时分派缓存修复，同时复测阶段 5 的类型查询和属性查询路径。

| case | run1 | run2 | run3 | median | median/iter |
| --- | ---: | ---: | ---: | ---: | ---: |
| `meta.resolve` | 2.93 ms | 2.88 ms | 2.88 ms | 2.88 ms | 28.80 ns |
| `meta.find_registered` | 2.56 ms | 2.57 ms | 2.57 ms | 2.57 ms | 25.70 ns |
| `meta.property.member_get_set_typed` | 31.01 ms | 31.07 ms | 31.34 ms | 31.07 ms | 310.70 ns |
| `meta.instance.get_set_value` | 94.21 ms | 95.01 ms | 94.58 ms | 94.58 ms | 945.80 ns |
| `serialization.schema_view_find` | 831.20 us | 792.50 us | 799.80 us | 799.80 us | 8.00 ns |
| `serialization.schema_view_property_lookup` | 28.63 ms | 28.23 ms | 28.08 ms | 28.23 ms | 282.30 ns |
| `serialization.to_value` | 69.50 ms | 67.43 ms | 67.01 ms | 67.43 ms | 674.30 ns |
| `serialization.write_json_reused_writer` | 95.46 ms | 95.41 ms | 93.33 ms | 95.41 ms | 954.10 ns |
| `serialization.deserialize_from_variant` | 106.56 ms | 106.14 ms | 106.22 ms | 106.22 ms | 1.06 us |
| `serialization.read_json_from_text_reader` | 150.49 ms | 159.96 ms | 152.40 ms | 152.40 ms | 1.52 us |

## Comparison

与同机、同编译器、同迭代次数的 `ec11212` 基线比较：

- `meta.resolve`：`28.50 ns` -> `28.80 ns`，基本持平；
- `meta.find_registered`：`25.90 ns` -> `25.70 ns`，基本持平；
- `meta.property.member_get_set_typed`：`331.50 ns` -> `310.70 ns`，略有改善；
- `meta.instance.get_set_value`：`934.70 ns` -> `945.80 ns`，基本持平；
- `serialization.to_value`：`663.10 ns` -> `674.30 ns`，基本持平；
- `serialization.write_json_reused_writer`：`967.40 ns` -> `954.10 ns`，基本持平；
- `serialization.deserialize_from_variant`：`1.07 us` -> `1.06 us`，基本持平；
- `serialization.read_json_from_text_reader`：`1.54 us` -> `1.52 us`，基本持平。

## Conclusion

序列化字段 schema 与属性分派现在在类型注册时编译一次，并保存在 `type_schema` 的私有运行时缓存中。
公开 schema 仍只表达配置，不暴露内部 `property_data`。完整序列化路径已经恢复到旧基线附近，
`a83c174` 记录的性能回退已解决。
