<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# CAORS benchmarks

本目录存放 CAORS 基准程序。benchmark 默认不参与普通构建，需要显式启用：

```sh
cmake --preset windows-clang-debug -DCAORS_BUILD_BENCHMARKS=ON
cmake --build --preset windows-clang-debug --target caors_benchmarks
build/benchmarks/caors_benchmarks 100000
```

正式比较建议使用独立 release benchmark preset，避免和日常 Debug 构建目录混用：

```sh
cmake --preset windows-clang-benchmark
cmake --build --preset windows-clang-benchmark --target caors_benchmarks
build/windows-clang-benchmark/benchmarks/caors_benchmarks 100000
```

Debug benchmark 只用于冒烟和粗略观察。正式比较性能时应使用 release preset，并保持同一机器、
同一迭代次数重复运行，详细规则见 `docs/benchmarking.md`。

benchmark 源码按主题拆分在 `core/`、`meta/`、`serialization/`、`resources/`、`memory/`
等子目录中，跨主题 fixture 和公共辅助函数放在 `support/`。当前仍使用一个聚合目标
`caors_benchmarks`，方便一次运行观察不同模块的相对成本。

当前 benchmark 覆盖 `string_name`、meta 类型查询、属性读写、方法调用、`any` / `any_ref`、
序列化到 `variant` / JSON 文本、schema 查找、property 查找、反射字段读取、手动 DOM 构造、DOM 写 JSON、
直接 JSON writer、JSON parse / DOM 读回，以及资源 cache / provider / JSON loader 主路径。方法调用会同时测
直接 C++ 调用、`method::invoke_result_of()`、`method::invoke()`、
`instance::call()` 和 `instance::call_any()`；属性读写会同时测 typed、`meta::value` 和 `any`
路径。程序末尾会额外输出若干分配统计样本，用于判断是否存在明显的临时对象或 buffer 分配热点。
异步请求、磁盘文件系统和网络相关能力更适合后续单独拆专项用例，避免与基础运行期 API 的耗时混在一起。
