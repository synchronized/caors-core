<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta Wrapper Benchmark Result - 2026-07-31

## Environment

- commit: `22497cb`
- preset: `windows-clang-benchmark`
- target: `caors_benchmarks`
- iterations: `100000`
- runs: `3`
- compiler: `clang version 22.1.8`
- OS: `Microsoft Windows 10 专业版 10.0.19045`
- CPU: `Intel(R) Core(TM) i5-8600K CPU @ 3.60GHz`
- cores / logical processors: `6 / 6`
- raw output: not committed

## Property And Method Runtime Paths

`time acc` 是每个用例执行 `100000` 次的总耗时；`median/iter` 是三轮总耗时的中位数除以迭代次数。
该结果只用于同机、同 preset 的路径拆分，不与其他机器上的历史结果直接比较。

| case | run 1 | run 2 | run 3 | median | median/iter |
| --- | ---: | ---: | ---: | ---: | ---: |
| `meta.instance.call_any_template` | 198.680 ms | 193.470 ms | 194.550 ms | 194.550 ms | 1945.50 ns |
| `meta.instance.call_value_span` | 54.130 ms | 52.220 ms | 52.030 ms | 52.220 ms | 522.20 ns |
| `meta.instance.call_value_template` | 45.750 ms | 45.210 ms | 44.790 ms | 45.210 ms | 452.10 ns |
| `meta.instance.get_set_value` | 170.150 ms | 168.990 ms | 167.140 ms | 168.990 ms | 1689.90 ns |
| `meta.method.default_invoke` | 170.520 ms | 168.580 ms | 166.190 ms | 168.580 ms | 1685.80 ns |
| `meta.method.default_match` | 35.720 ms | 33.910 ms | 34.400 ms | 34.400 ms | 344.00 ns |
| `meta.method.direct_call` | 0.120 ms | 0.125 ms | 0.124 ms | 0.124 ms | 1.25 ns |
| `meta.method.invoke_result_of_any` | 130.530 ms | 132.800 ms | 131.790 ms | 131.790 ms | 1317.90 ns |
| `meta.method.invoke_value` | 27.300 ms | 26.030 ms | 25.550 ms | 26.030 ms | 260.30 ns |
| `meta.method.member_non_const_invoke_result` | 91.750 ms | 89.830 ms | 90.250 ms | 90.250 ms | 902.50 ns |
| `meta.method.numeric_conversion_invoke` | 240.410 ms | 299.580 ms | 247.970 ms | 247.970 ms | 2479.70 ns |
| `meta.method.numeric_conversion_match` | 73.500 ms | 122.560 ms | 72.870 ms | 73.500 ms | 735.00 ns |
| `meta.method.overload_invoke_exact` | 127.920 ms | 128.420 ms | 124.640 ms | 127.920 ms | 1279.20 ns |
| `meta.method.overload_match_exact` | 27.790 ms | 27.140 ms | 27.490 ms | 27.490 ms | 274.90 ns |
| `meta.property.accessor_get_set_any` | 53.250 ms | 50.350 ms | 50.740 ms | 50.740 ms | 507.40 ns |
| `meta.property.accessor_get_set_value` | 150.230 ms | 148.720 ms | 145.370 ms | 148.720 ms | 1487.20 ns |
| `meta.property.member_get_set_compiled` | 18.380 ms | 18.120 ms | 18.410 ms | 18.380 ms | 183.80 ns |
| `meta.property.member_get_set_direct` | 0.129 ms | 0.129 ms | 0.129 ms | 0.129 ms | 1.29 ns |
| `meta.property.member_get_set_typed` | 79.520 ms | 78.700 ms | 78.290 ms | 78.700 ms | 787.00 ns |
| `meta.property.member_get_set_value` | 130.450 ms | 126.810 ms | 128.120 ms | 128.120 ms | 1281.20 ns |
| `meta.property.readonly_get_typed` | 16.120 ms | 16.110 ms | 16.070 ms | 16.110 ms | 161.10 ns |
| `meta.property.reference_accessor_get_set_typed` | 78.640 ms | 79.210 ms | 78.590 ms | 78.640 ms | 786.40 ns |

## Observations

- 缓存后的 compiled member accessor 中位数为 `183.80 ns/iter`，明显低于通用 cached typed
  property 的 `787.00 ns/iter`，说明热点路径预编译 accessor 能有效减少通用分派成本。
- readonly typed getter 为 `161.10 ns/iter`；引用 accessor 的 typed get/set 与 member typed
  get/set 基本同量级。
- 精确重载匹配本身为 `274.90 ns/iter`，完整精确重载调用为 `1279.20 ns/iter`；默认参数和安全
  数值转换会继续增加匹配与参数准备成本。
- 数值转换用例的第 2 轮有明显波动，因此这里只保留三轮中位数，不据此单独提出优化。
- Windows 首轮结果已能区分查找、匹配、分派和装箱路径；是否达到优化触发条件仍需结合 Linux
  Clang 结果判断。
