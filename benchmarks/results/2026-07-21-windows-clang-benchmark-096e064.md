<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Benchmark Result - 2026-07-21

## Environment

- commit: `096e064`
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
| `meta.method.direct_call` | 125.40 us | 129.40 us | 122.40 us | 125.40 us | 1.25 ns |
| `core.string_name.compare` | 128.30 us | 130.50 us | 129.00 us | 129.00 us | 1.29 ns |
| `math.geometry` | 269.80 us | 270.90 us | 269.50 us | 269.80 us | 2.70 ns |
| `math.perspective_rh_zo` | 318.20 us | 318.70 us | 321.10 us | 318.70 us | 3.19 ns |
| `math.meta.method_vec3_direct` | 674.20 us | 674.30 us | 700.70 us | 674.30 us | 6.74 ns |
| `resources.cache.find` | 770.50 us | 769.70 us | 770.50 us | 770.50 us | 7.71 ns |
| `math.scalar_aliases` | 780.90 us | 780.20 us | 782.90 us | 780.90 us | 7.81 ns |
| `serialization.schema_view_find` | 844.00 us | 847.10 us | 1.02 ms | 847.10 us | 8.47 ns |
| `math.dmat4.multiply` | 1.14 ms | 1.39 ms | 1.10 ms | 1.14 ms | 11.40 ns |
| `math.mat4.multiply` | 1.19 ms | 1.19 ms | 1.19 ms | 1.19 ms | 11.90 ns |
| `math.quat.rotate` | 1.20 ms | 1.21 ms | 1.21 ms | 1.21 ms | 12.10 ns |
| `math.mat4.transform_vec4` | 1.58 ms | 1.59 ms | 1.60 ms | 1.59 ms | 15.90 ns |
| `math.vec3.dot_cross_normalize` | 2.26 ms | 1.82 ms | 1.81 ms | 1.82 ms | 18.20 ns |
| `math.frustum_cull` | 2.54 ms | 2.14 ms | 2.21 ms | 2.21 ms | 22.10 ns |
| `meta.find_registered` | 2.61 ms | 2.58 ms | 2.61 ms | 2.61 ms | 26.10 ns |
| `meta.resolve` | 2.79 ms | 3.03 ms | 2.95 ms | 2.95 ms | 29.50 ns |
| `math.project_unproject` | 3.20 ms | 2.95 ms | 3.03 ms | 3.03 ms | 30.30 ns |
| `resources.load_handle_into.cached` | 3.40 ms | 3.41 ms | 3.78 ms | 3.41 ms | 34.10 ns |
| `meta.any.from_scalar` | 3.69 ms | 3.71 ms | 4.09 ms | 3.71 ms | 37.10 ns |
| `math.meta.any_quat` | 10.37 ms | 5.07 ms | 5.03 ms | 5.07 ms | 50.70 ns |
| `math.trs` | 6.30 ms | 6.34 ms | 6.31 ms | 6.31 ms | 63.10 ns |
| `meta.any_ref.from_and_try_get` | 8.30 ms | 9.61 ms | 8.41 ms | 8.41 ms | 84.10 ns |
| `math.meta.any_ref_vec3` | 8.68 ms | 8.74 ms | 8.71 ms | 8.71 ms | 87.10 ns |
| `math.meta.any_vec3` | 10.31 ms | 10.58 ms | 10.11 ms | 10.31 ms | 103.10 ns |
| `core.string_name.hot_lookup` | 11.61 ms | 10.81 ms | 11.58 ms | 11.58 ms | 115.80 ns |
| `serialization.bound_schema_fields` | 14.64 ms | 13.36 ms | 13.43 ms | 13.43 ms | 134.30 ns |
| `meta.method.invoke_value` | 15.55 ms | 15.97 ms | 15.37 ms | 15.55 ms | 155.50 ns |
| `serialization.direct_json_writer_object` | 23.25 ms | 23.01 ms | 23.82 ms | 23.25 ms | 232.50 ns |
| `meta.instance.call_value_template` | 23.89 ms | 25.13 ms | 23.16 ms | 23.89 ms | 238.90 ns |
| `serialization.schema_property_lookup` | 30.88 ms | 29.10 ms | 29.64 ms | 29.64 ms | 296.40 ns |
| `math.meta.property_vec3_typed` | 33.87 ms | 34.17 ms | 34.14 ms | 34.14 ms | 341.40 ns |
| `math.meta.property_vec3_any` | 36.43 ms | 34.49 ms | 34.43 ms | 34.49 ms | 344.90 ns |
| `math.meta.property_mat4_typed` | 34.41 ms | 34.77 ms | 57.41 ms | 34.77 ms | 347.70 ns |
| `serialization.manual_variant_object_direct` | 34.51 ms | 35.09 ms | 36.21 ms | 35.09 ms | 350.90 ns |
| `meta.property.member_get_set_typed` | 33.35 ms | 35.67 ms | 56.52 ms | 35.67 ms | 356.70 ns |
| `meta.property.accessor_get_set_any` | 38.12 ms | 39.78 ms | 38.18 ms | 38.18 ms | 381.80 ns |
| `resources.provider.read_memory_file_system` | 40.42 ms | 40.96 ms | 40.02 ms | 40.42 ms | 404.20 ns |
| `meta.instance.call_value_span` | 45.83 ms | 44.90 ms | 43.67 ms | 44.90 ms | 449.00 ns |
| `math.meta.method_vec3_any` | 64.19 ms | 66.78 ms | 68.52 ms | 66.78 ms | 667.80 ns |
| `serialization.properties_get_ref` | 71.83 ms | 66.87 ms | 66.50 ms | 66.87 ms | 668.70 ns |
| `serialization.to_value` | 77.98 ms | 70.00 ms | 67.97 ms | 70.00 ms | 700.00 ns |
| `serialization.write_value_to_json_reused_buffer` | 79.21 ms | 80.34 ms | 81.79 ms | 80.34 ms | 803.40 ns |
| `meta.property.member_get_set_value` | 80.50 ms | 75.21 ms | 106.32 ms | 80.50 ms | 805.00 ns |
| `serialization.field_refs_to_variant` | 82.59 ms | 82.17 ms | 80.30 ms | 82.17 ms | 821.70 ns |
| `meta.method.invoke_result_of_any` | 89.55 ms | 88.09 ms | 85.99 ms | 88.09 ms | 880.90 ns |
| `serialization.write_json_to_reused_buffer` | 97.97 ms | 96.51 ms | 98.29 ms | 97.97 ms | 979.70 ns |
| `serialization.parse_json` | 98.51 ms | 102.23 ms | 101.69 ms | 101.69 ms | 1.02 us |
| `meta.instance.get_set_value` | 115.32 ms | 102.90 ms | 97.82 ms | 102.90 ms | 1.03 us |
| `serialization.deserialize_from_variant` | 108.95 ms | 106.21 ms | 106.46 ms | 106.46 ms | 1.06 us |
| `resources.load_handle_into.uncached` | 110.63 ms | 62.41 ms | 112.36 ms | 110.63 ms | 1.11 us |
| `meta.instance.call_any_template` | 117.32 ms | 115.35 ms | 113.26 ms | 115.35 ms | 1.15 us |
| `meta.property.accessor_get_set_value` | 120.39 ms | 118.70 ms | 117.23 ms | 118.70 ms | 1.19 us |
| `serialization.read_json_from_text_reader` | 158.63 ms | 161.84 ms | 160.82 ms | 160.82 ms | 1.61 us |
| `math.meta.method_vec3_value` | 172.55 ms | 173.02 ms | 173.95 ms | 173.02 ms | 1.73 us |
| `serialization.to_json_allocate` | 176.42 ms | 181.41 ms | 184.13 ms | 181.41 ms | 1.81 us |
| `resources.json_loader.load_memory_file_system` | 199.30 ms | 197.17 ms | 195.03 ms | 197.17 ms | 1.97 us |
| `math.meta.property_vec3_value` | 223.38 ms | 224.11 ms | 217.14 ms | 223.38 ms | 2.23 us |
| `resources.load_handle_into.json_uncached` | 246.52 ms | 418.47 ms | 240.26 ms | 246.52 ms | 2.47 us |

## Allocation Samples

分配统计样本固定使用 `1000` 次迭代，来自第 1 次运行。

| case | allocations | allocations/iter | bytes | bytes/iter | peak | live |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `serialization.to_value` | 0 | 0 | 0 | 0 | 0 | 0 |
| `serialization.write_json_to_reused_buffer` | 1 | 0.001 | 64 | 0.064 | 64 | 0 |
| `serialization.parse_json` | 4000 | 4 | 1840000 | 1840 | 1288 | 0 |
| `resources.load_handle_into.json_uncached` | 11020 | 11.02 | 419458 | 419.458 | 1610 | 0 |

## Math Dynamic Path Notes

- 纯 C++ `math.meta.method_vec3_direct` 中位数是 `6.74 ns/iter`，只是直接调用 `actor.offset()` 的对照组。
- `math.meta.any_ref_vec3` 是 `87.10 ns/iter`，和通用 `meta.any_ref.from_and_try_get` 的 `84.10 ns/iter`
  接近，说明单独构造 `any_ref` 的成本目前不是 math fast path 的主要矛盾。
- `math.meta.any_vec3` 是 `103.10 ns/iter`，`math.meta.any_quat` 是 `50.70 ns/iter`。第一轮数据里
  `any` 拥有式封装成本低于完整 property/method value 路径，后续需要继续用真实 workload 验证。
- `math.meta.property_vec3_typed`、`property_vec3_any`、`property_mat4_typed` 都在 `340-350 ns/iter`
  左右，和已有 `meta.property.member_get_set_typed` 同量级。
- `math.meta.method_vec3_value` 是 `1.73 us/iter`，`math.meta.property_vec3_value` 是 `2.23 us/iter`。
  当前 `meta::value` 对数学类型走 `any` / `any_ref` 兜底，这两条才是后续判断 `vec3/quat`
  fast path 是否值得做的重点。

## Notes

- 当前结果适合作为提交 `096e064` 的本机 release 基线。
- 本次新增 math 动态访问路径，用于对比直接 C++、typed 反射、`any`、`any_ref` 和 `meta::value`
  兜底路径的相对成本。
- 当前不建议马上把数学类型加入 `meta::value` fast path；先继续积累脚本式动态调用、编辑器属性访问和
  序列化转换 workload 数据。
- `resources.load_handle_into.json_uncached` 第 2 次运行波动明显，取中位数后仍和旧基线同量级；资源 JSON
  loader 路径后续仍应继续作为高成本路径观察。
