<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Benchmark Result - 2026-07-21

## Environment

- commit: `ec11212`
- preset: `windows-clang-benchmark`
- target: `caors_benchmarks`
- iterations: `100000`
- runs: `3`
- compiler: `clang version 22.1.8`
- OS: `Microsoft Windows 10 专业版 10.0.19045`
- CPU: `Intel(R) Core(TM) i5-8600K CPU @ 3.60GHz`
- cores / logical processors: `6 / 6`
- raw output: not committed

## Summary

`time acc` 是每个用例执行 `100000` 次的总耗时；`median/iter` 是三次运行中位数除以迭代次数。

| case | run1 | run2 | run3 | median | median/iter |
| --- | ---: | ---: | ---: | ---: | ---: |
| `core.string_name.hot_lookup` | 10.81 ms | 10.92 ms | 10.60 ms | 10.81 ms | 108.10 ns |
| `core.string_name.compare` | 128.20 us | 143.90 us | 128.10 us | 128.20 us | 1.28 ns |
| `meta.resolve` | 3.01 ms | 2.85 ms | 2.79 ms | 2.85 ms | 28.50 ns |
| `meta.find_registered` | 2.59 ms | 2.61 ms | 2.58 ms | 2.59 ms | 25.90 ns |
| `meta.property.member_get_set_typed` | 32.06 ms | 33.58 ms | 33.15 ms | 33.15 ms | 331.50 ns |
| `meta.property.member_get_set_value` | 71.20 ms | 71.21 ms | 70.51 ms | 71.20 ms | 712.00 ns |
| `meta.instance.get_set_value` | 94.06 ms | 93.47 ms | 93.01 ms | 93.47 ms | 934.70 ns |
| `meta.method.direct_call` | 122.40 us | 122.40 us | 123.80 us | 122.40 us | 1.22 ns |
| `meta.method.invoke_result_of_any` | 83.77 ms | 83.88 ms | 84.13 ms | 83.88 ms | 838.80 ns |
| `meta.method.invoke_value` | 15.55 ms | 15.59 ms | 15.32 ms | 15.55 ms | 155.50 ns |
| `meta.instance.call_value_span` | 45.61 ms | 44.98 ms | 45.19 ms | 45.19 ms | 451.90 ns |
| `meta.instance.call_value_template` | 23.70 ms | 23.45 ms | 23.47 ms | 23.47 ms | 234.70 ns |
| `meta.instance.call_any_template` | 111.43 ms | 112.49 ms | 111.88 ms | 111.88 ms | 1.12 us |
| `meta.any_ref.from_and_try_get` | 8.69 ms | 8.24 ms | 8.26 ms | 8.26 ms | 82.60 ns |
| `meta.any.from_scalar` | 3.58 ms | 3.62 ms | 3.62 ms | 3.62 ms | 36.20 ns |
| `math.mat4.multiply` | 1.23 ms | 1.21 ms | 1.18 ms | 1.21 ms | 12.10 ns |
| `math.trs` | 6.42 ms | 6.37 ms | 6.38 ms | 6.38 ms | 63.80 ns |
| `serialization.to_value` | 66.27 ms | 66.63 ms | 66.31 ms | 66.31 ms | 663.10 ns |
| `serialization.write_value_to_json_reused_buffer` | 77.36 ms | 78.36 ms | 79.38 ms | 78.36 ms | 783.60 ns |
| `serialization.write_json_to_reused_buffer` | 96.08 ms | 96.74 ms | 97.06 ms | 96.74 ms | 967.40 ns |
| `serialization.to_json_allocate` | 174.45 ms | 175.43 ms | 174.08 ms | 174.45 ms | 1.74 us |
| `serialization.parse_json` | 100.55 ms | 103.98 ms | 99.06 ms | 100.55 ms | 1.01 us |
| `serialization.deserialize_from_variant` | 104.40 ms | 106.64 ms | 107.08 ms | 106.64 ms | 1.07 us |
| `serialization.read_json_from_text_reader` | 152.64 ms | 153.83 ms | 164.93 ms | 153.83 ms | 1.54 us |
| `resources.cache.find` | 754.10 us | 766.80 us | 750.60 us | 754.10 us | 7.54 ns |
| `resources.load_handle_into.cached` | 3.43 ms | 3.40 ms | 3.40 ms | 3.40 ms | 34.00 ns |
| `resources.provider.read_memory_file_system` | 40.07 ms | 38.60 ms | 39.24 ms | 39.24 ms | 392.40 ns |
| `resources.json_loader.load_memory_file_system` | 196.17 ms | 184.75 ms | 188.93 ms | 188.93 ms | 1.89 us |
| `resources.load_handle_into.json_uncached` | 236.40 ms | 233.56 ms | 239.92 ms | 236.40 ms | 2.36 us |

## Allocation Samples

分配统计样本固定使用 `1000` 次迭代，来自第 1 次运行。

| case | allocations | allocations/iter | bytes | bytes/iter | peak | live |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `serialization.to_value` | 0 | 0 | 0 | 0 | 0 | 0 |
| `serialization.write_json_to_reused_buffer` | 1 | 0.001 | 64 | 0.064 | 64 | 0 |
| `serialization.parse_json` | 4000 | 4 | 1840000 | 1840 | 1288 | 0 |
| `resources.load_handle_into.json_uncached` | 11020 | 11.02 | 419458 | 419.458 | 1610 | 0 |

## Notes

- 当前结果适合作为提交 `ec11212` 的本机 release 基线。
- `serialization.to_json_allocate`、`serialization.read_json_from_text_reader`、
  `resources.json_loader.load_memory_file_system` 和 `resources.load_handle_into.json_uncached`
  是后续优先观察的高成本路径。
- `math` 用例是纯计算微基准，不代表真实渲染 workload。
