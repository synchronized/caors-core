<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# MessagePack Benchmark Baseline - 2026-07-29

## 环境

- production baseline：`d662205`
- preset：`windows-clang-benchmark`
- target：`caors_benchmarks`
- iterations：`100000`
- runs：`3`
- compiler：`clang version 22.1.8`
- OS：`Microsoft Windows 10 专业版 10.0.19045`
- CPU：`13th Gen Intel(R) Core(TM) i5-13600KF`
- cores / logical processors：`14 / 20`
- raw output：未提交

## 耗时

`time acc` 是每个用例执行 100000 次的总耗时。

| case | run 1 | run 2 | run 3 | range per iter |
| --- | ---: | ---: | ---: | ---: |
| `serialization.parse_messagepack` | 70.25 ms | 66.03 ms | 66.31 ms | 660.3–702.5 ns |
| `serialization.deserialize_from_messagepack_variant` | 247.98 ms | 215.67 ms | 227.78 ms | 2.157–2.480 μs |
| `serialization.read_messagepack_from` | 327.82 ms | 291.28 ms | 302.18 ms | 2.913–3.278 μs |

## 分配样本

分配样本固定执行 1000 次完整 typed read：

| resource | allocations | bytes | peak | live |
| --- | ---: | ---: | ---: | ---: |
| result | 1000（1/次） | 736000（736/次） | 736 | 0 |
| temporary upstream | 2 | 65584 | 65584 | 0 |

`stack_resource` 的逻辑峰值为 1344 字节、1 个 chunk，mark/rewind 各 1000 次。

## 结论

当前小型反射对象的 typed MessagePack 读取只为 DOM 向 result resource 分配一次。把 DOM 整体迁入
temporary resource 会要求 preserved unknown fields 等逃逸值额外深拷贝，当前收益不足。保持现有
result-resource DOM；后续大型 workload 如果证明 DOM 成为瓶颈，优先评估直接 typed reader。
