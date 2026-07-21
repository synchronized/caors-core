<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Benchmark Result - 2026-07-21

## Environment

- commit: `4f5a18c`
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
| `core.string_name.compare` | 128.20 us | 128.30 us | 135.10 us | 128.30 us | 1.28 ns |
| `meta.method.direct_call` | 133.60 us | 122.40 us | 122.60 us | 122.60 us | 1.23 ns |
| `math.geometry` | 270.00 us | 268.10 us | 275.10 us | 270.00 us | 2.70 ns |
| `math.perspective_rh_zo` | 321.80 us | 320.40 us | 319.40 us | 320.40 us | 3.20 ns |
| `resources.cache.find` | 762.10 us | 745.60 us | 739.40 us | 745.60 us | 7.46 ns |
| `math.scalar_aliases` | 780.30 us | 780.50 us | 1.79 ms | 780.50 us | 7.80 ns |
| `serialization.schema_view_find` | 914.90 us | 914.70 us | 902.20 us | 914.70 us | 9.15 ns |
| `math.dmat4.multiply` | 1.08 ms | 1.08 ms | 1.09 ms | 1.08 ms | 10.80 ns |
| `math.quat.rotate` | 1.21 ms | 1.21 ms | 1.53 ms | 1.21 ms | 12.10 ns |
| `math.mat4.multiply` | 1.24 ms | 1.19 ms | 1.25 ms | 1.24 ms | 12.40 ns |
| `math.mat4.transform_vec4` | 1.59 ms | 1.58 ms | 1.71 ms | 1.59 ms | 15.90 ns |
| `math.vec3.dot_cross_normalize` | 1.82 ms | 1.82 ms | 1.81 ms | 1.82 ms | 18.20 ns |
| `math.frustum_cull` | 2.16 ms | 2.14 ms | 2.16 ms | 2.16 ms | 21.60 ns |
| `meta.find_registered` | 2.57 ms | 2.57 ms | 2.74 ms | 2.57 ms | 25.70 ns |
| `meta.resolve` | 2.80 ms | 2.79 ms | 3.25 ms | 2.80 ms | 28.00 ns |
| `math.project_unproject` | 2.95 ms | 2.96 ms | 3.15 ms | 2.96 ms | 29.60 ns |
| `resources.load_handle_into.cached` | 3.41 ms | 4.72 ms | 3.58 ms | 3.58 ms | 35.80 ns |
| `meta.any.from_scalar` | 3.87 ms | 3.62 ms | 3.79 ms | 3.79 ms | 37.90 ns |
| `math.trs` | 6.37 ms | 6.30 ms | 7.04 ms | 6.37 ms | 63.70 ns |
| `meta.any_ref.from_and_try_get` | 8.26 ms | 8.25 ms | 9.83 ms | 8.26 ms | 82.60 ns |
| `core.string_name.hot_lookup` | 10.66 ms | 10.81 ms | 12.68 ms | 10.81 ms | 108.10 ns |
| `serialization.bound_schema_fields` | 13.51 ms | 13.17 ms | 14.38 ms | 13.51 ms | 135.10 ns |
| `meta.method.invoke_value` | 15.74 ms | 15.43 ms | 17.08 ms | 15.74 ms | 157.40 ns |
| `serialization.direct_json_writer_object` | 23.03 ms | 22.99 ms | 23.80 ms | 23.03 ms | 230.30 ns |
| `meta.instance.call_value_template` | 24.26 ms | 24.41 ms | 25.54 ms | 24.41 ms | 244.10 ns |
| `serialization.schema_property_lookup` | 28.08 ms | 28.93 ms | 34.96 ms | 28.93 ms | 289.30 ns |
| `meta.property.member_get_set_typed` | 32.92 ms | 32.92 ms | 36.87 ms | 32.92 ms | 329.20 ns |
| `serialization.manual_variant_object_direct` | 33.68 ms | 35.52 ms | 35.45 ms | 35.45 ms | 354.50 ns |
| `meta.property.accessor_get_set_any` | 37.38 ms | 37.60 ms | 46.48 ms | 37.60 ms | 376.00 ns |
| `resources.provider.read_memory_file_system` | 39.61 ms | 55.15 ms | 39.33 ms | 39.61 ms | 396.10 ns |
| `meta.instance.call_value_span` | 44.83 ms | 45.35 ms | 46.41 ms | 45.35 ms | 453.50 ns |
| `resources.load_handle_into.uncached` | 55.25 ms | 69.63 ms | 53.54 ms | 55.25 ms | 552.50 ns |
| `serialization.properties_get_ref` | 64.04 ms | 66.69 ms | 82.81 ms | 66.69 ms | 666.90 ns |
| `serialization.to_value` | 67.81 ms | 67.83 ms | 70.19 ms | 67.83 ms | 678.30 ns |
| `meta.property.member_get_set_value` | 73.62 ms | 71.91 ms | 77.32 ms | 73.62 ms | 736.20 ns |
| `serialization.write_value_to_json_reused_buffer` | 76.33 ms | 79.60 ms | 84.06 ms | 79.60 ms | 796.00 ns |
| `serialization.field_refs_to_variant` | 78.57 ms | 95.52 ms | 150.73 ms | 95.52 ms | 955.20 ns |
| `meta.method.invoke_result_of_any` | 86.42 ms | 89.24 ms | 89.50 ms | 89.24 ms | 892.40 ns |
| `meta.instance.get_set_value` | 95.14 ms | 97.13 ms | 113.86 ms | 97.13 ms | 971.30 ns |
| `serialization.write_json_to_reused_buffer` | 96.53 ms | 93.37 ms | 95.79 ms | 95.79 ms | 957.90 ns |
| `serialization.parse_json` | 100.26 ms | 104.02 ms | 97.44 ms | 100.26 ms | 1.00 us |
| `serialization.deserialize_from_variant` | 107.85 ms | 122.38 ms | 107.83 ms | 107.85 ms | 1.08 us |
| `meta.instance.call_any_template` | 114.54 ms | 116.27 ms | 126.71 ms | 116.27 ms | 1.16 us |
| `meta.property.accessor_get_set_value` | 117.93 ms | 115.44 ms | 139.42 ms | 117.93 ms | 1.18 us |
| `serialization.read_json_from_text_reader` | 156.99 ms | 216.50 ms | 156.06 ms | 156.99 ms | 1.57 us |
| `serialization.to_json_allocate` | 172.47 ms | 169.66 ms | 170.58 ms | 170.58 ms | 1.71 us |
| `resources.json_loader.load_memory_file_system` | 190.82 ms | 257.75 ms | 187.99 ms | 190.82 ms | 1.91 us |
| `resources.load_handle_into.json_uncached` | 238.81 ms | 305.43 ms | 236.99 ms | 238.81 ms | 2.39 us |

## Allocation Samples

分配统计样本固定使用 `1000` 次迭代，来自第 1 次运行。

| case | allocations | allocations/iter | bytes | bytes/iter | peak | live |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `serialization.to_value` | 0 | 0 | 0 | 0 | 0 | 0 |
| `serialization.write_json_to_reused_buffer` | 1 | 0.001 | 64 | 0.064 | 64 | 0 |
| `serialization.parse_json` | 4000 | 4 | 1840000 | 1840 | 1288 | 0 |
| `resources.load_handle_into.json_uncached` | 11020 | 11.02 | 419458 | 419.458 | 1610 | 0 |

## Notes

- 当前结果适合作为提交 `4f5a18c` 的本机 release 基线。
- 本次新增 `math.frustum_cull` 和 `math.project_unproject`；它们是纯 CPU 微基准，不代表真实渲染 workload。
- `serialization.field_refs_to_variant`、`serialization.read_json_from_text_reader` 和资源 JSON loader 路径在三次运行中波动较明显，后续对比应继续看同机多次中位数。
- `serialization.to_json_allocate`、`serialization.read_json_from_text_reader`、
  `resources.json_loader.load_memory_file_system` 和 `resources.load_handle_into.json_uncached`
  仍是后续优先观察的高成本路径。
