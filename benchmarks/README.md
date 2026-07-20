<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# CAORS benchmarks

本目录存放 CAORS 基准程序。benchmark 默认不参与普通构建，需要显式启用：

```sh
cmake --preset windows-clang-debug -DCAORS_BUILD_BENCHMARKS=ON
cmake --build --preset windows-clang-debug --target caors_benchmarks
build/benchmarks/caors_benchmarks 100000
```

当前 benchmark 覆盖 `string_name`、meta 类型查询、属性读写、方法调用、`any` / `any_ref` 和
序列化到 `variant`。方法调用会同时测直接 C++ 调用、`method::invoke_result_of()`、
`method::invoke()`、`instance::call()` 和 `instance::call_any()`；属性读写会同时测 typed、
`meta::value` 和 `any` 路径。资源加载、异步请求、文件系统和网络相关能力更适合后续单独拆专项用例，
避免与基础运行期 API 的耗时混在一起。
