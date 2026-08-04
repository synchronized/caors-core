<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Windows Clang small_vector 基准

## 环境与方法

- 日期：2026-08-04；
- 分支：`feature/small-vector`；
- preset：`windows-clang-benchmark`，Release；
- 编译器：Clang 22.1.8；
- 每个 workload 执行 100,000 轮，完整程序独立运行三次；
- 下表使用三次总耗时的中位数；分配统计单独执行 10,000 轮，不计入计时结果。

这些 microbenchmark 用于判断内联命中和已知容量场景的边界，不代表真实业务收益。真正接入组件前仍需
使用对应 workload 复验。

## 对象大小

| 类型 | `sizeof` |
| --- | ---: |
| `small_vector<int, 8>` | 64 B |
| `vector<int>` | 32 B |
| `small_vector<64-byte value, 4>` | 288 B |

内联区会直接增大容器对象。大量长生命周期空容器或低命中实例可能因对象膨胀得不偿失。

## 结果

| workload | `small_vector` | 普通 `vector` | 预留容量 `vector` | 判断 |
| --- | ---: | ---: | ---: | --- |
| 追加 4 个 `int`，全部内联 | 0.554 ms | 22.49 ms | 5.84 ms | 内联版本相对预留容量对照约快 10.5 倍 |
| 90% 追加 4 个、10% 追加 12 个 | 1.42 ms | 24.44 ms | — | 偶尔溢出仍约快 17.2 倍 |
| 追加 12 个 `int` | 7.41 ms | 40.83 ms | 6.57 ms | 比未知长度 vector 快，但比精确 reserve 慢约 12.8% |
| 追加 64 个 `int` | 50.69 ms | 75.23 ms | 8.09 ms | 频繁增长明显不适用，比精确 reserve 慢约 6.3 倍 |
| 追加 4 个 64-byte 值 | 1.05 ms | 23.59 ms | — | 无分配路径快，但每个容器对象增至 288 B |

固定上限 `std::array<int, 8>` 加独立 size 语义的简化对照为 0.141 ms；它没有动态增长能力，因此只作为
固定容量下限参考，不能直接替换 vector-like 容器。

## 分配结果

| workload | 每轮分配次数 | 每轮累计分配字节 |
| --- | ---: | ---: |
| `small_vector<int, 8>` 追加 4 个 | 0 | 0 B |
| `small_vector<int, 8>` 追加 12 个 | 1 | 48 B |
| 普通 `vector<int>` 追加 4 个 | 4 | 40 B |

## 结论

- 已知绝大多数实例不超过内联容量时，`small_vector` 能稳定消除动态分配并显著降低短工作集耗时；
- 元素数量经常远超内联容量且调用方已知最终大小时，普通 vector 配合 `reserve()` 明显更合适；
- 大元素会快速放大容器对象，只适合实例数量有限且内联命中率很高的场景；
- 当前结果支持保留该容器 API，但不足以授权批量替换现有 `core::vector`；实际接入仍需独立评审。
