<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Benchmark Result - 2026-07-27

## Environment

- commit: `7015a59`
- preset: `windows-clang-benchmark`
- target: `caors_benchmarks`
- iterations: `100000`
- runs: `1`
- compiler: `clang version 22.1.8`
- OS: `Microsoft Windows 10 专业版 10.0.19045`
- CPU: `13th Gen Intel(R) Core(TM) i5-13600KF`
- cores / logical processors: `14 / 20`
- raw output: not committed

## Registry Query Fast Path

`time acc` 是每个用例执行 `100000` 次的总耗时；`per iter` 是该次运行总耗时除以迭代次数。
本次只记录 registry 查询快路径相关用例，用于确认 locked primary、frozen primary 和已发布 layer
之间的相对成本。

| case | time acc | per iter |
| --- | ---: | ---: |
| `meta.registry.locked_find_type_id` | 4.71 ms | 47.10 ns |
| `meta.registry.frozen_find_type_id` | 3.70 ms | 37.00 ns |
| `meta.registry.layer_find_type_id` | 3.70 ms | 37.00 ns |
| `meta.registry.frozen_contains` | 2.83 ms | 28.30 ns |
| `meta.registry.frozen_property_lookup` | 9.22 ms | 92.20 ns |
| `meta.registry.frozen_method_lookup` | 1.14 ms | 11.40 ns |
| `meta.registry.locked_schema_lookup` | 6.08 ms | 60.80 ns |
| `meta.registry.frozen_schema_lookup` | 4.65 ms | 46.50 ns |
| `meta.registry.layer_schema_lookup` | 4.62 ms | 46.20 ns |

## Conclusion

frozen primary 和已发布 layer 的 type/schema 查询均低于默认 locked primary。当前结果支持继续把运行期只读查询
推荐到 frozen registry 或已发布 layer；需要保留热注册的场景仍使用默认 locked append-only 策略。
