<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Benchmarking

CAORS 使用 `ctrack` 作为轻量 benchmark / profiling 工具。`ctrack` 以源码形式放在
`3rd/ctrack-1.1.0`，CMake 中包装为 `ctrack::ctrack` interface 目标；不会构建
`ctrack` 自带示例、测试和 benchmark。

## 构建和运行

benchmark 默认关闭，避免影响普通开发构建：

```sh
cmake --preset windows-clang-debug -DCAORS_BUILD_BENCHMARKS=ON
cmake --build --preset windows-clang-debug --target caors_benchmarks
build/benchmarks/caors_benchmarks 100000
```

命令行参数是每个用例内部循环次数；不传时默认 `100000`。

Debug benchmark 只适合确认用例能运行、观察 API 相对路径和粗略热点，不适合作为真实性能结论。
正式比较提交之间的性能时使用 Release preset，并固定迭代次数：

```sh
cmake --preset windows-clang-benchmark
cmake --build --preset windows-clang-benchmark --target caors_benchmarks
build/windows-clang-benchmark/benchmarks/caors_benchmarks 100000
```

Linux 下使用对应 release preset：

```sh
cmake --preset linux-clang-benchmark
cmake --build --preset linux-clang-benchmark --target caors_benchmarks
build/linux-clang-benchmark/benchmarks/caors_benchmarks 100000
```

建议同一机器、同一构建目录、同一迭代次数至少运行 3 次，取中位数对比。运行前尽量关闭无关后台任务；
如果要比较不同 API 路径，优先看同一次运行内的相对差异。如果要比较不同提交，记录编译器、preset、
迭代次数和提交号。

## 当前用例

当前 `caors_benchmarks` 覆盖以下稳定热路径：

- `core::string_name`：热路径 intern 查找和轻量比较。
- `meta::resolve<T>()` / `meta::find<T>()`：运行期类型查询。
- `meta::property` / `meta::instance`：成员属性和 getter/setter 属性的 typed、`value` 和 `any`
  读写路径。
- `meta::method` / `meta::instance::call()`：直接 C++ 调用、`invoke_result_of()`、
  `invoke()`、`call()` 和 `call_any()` 的对比。
- `meta::property` / `meta::method_set` 缓存句柄与 `instance` 按名便利入口的对比。按名入口每次
  调用都会查询属性或方法，热点循环应缓存 property / method 视图。
- `meta::any_ref` / `meta::any`：运行期引用创建、类型检查和标量装箱。
- `serialization::to_value()` / `serialize_json_to()` / `to_string()` / `parse_json()` / `deserialize()`：
  反射对象到 `variant` DOM、复用 buffer JSON 写出、一次性 JSON 字符串、JSON parse、DOM 读回对象和
  JSON 文本 reader 直接读回对象的主路径。
- 序列化拆分项：`schema_view_find` 测序列化 schema 视图查询，
  `bound_schema_fields` 测提交后已绑定字段 schema 的遍历，`schema_property_lookup` 测按 schema 字段名查 property，`reflect_properties_get_ref` 测字段引用读取，
  `field_refs_to_variant` 测字段引用转 DOM，`manual_variant_object_direct` 测不经过反射的手动 DOM 构造，
  `write_variant_json_reused_buffer` 测预构造 DOM 写入 JSON，`direct_json_writer_object` 测直接驱动 JSON
  writer 写对象。这些用例用于判断完整 JSON 序列化成本主要来自 schema 注册表、字段查找、反射、DOM 构造、
  DOM 写出、reader 字段索引还是 writer 本身。
- `resources::cache<T>` / `resources::provider` / `assets::json::loader<T>` /
  `resources::load_handle_into()`：cache 查找、已缓存加载、fake loader 未缓存加载、memory file system
  provider 读取，以及 provider -> JSON loader -> cache 的资源加载主路径。

这些 API 默认都是纯内存路径，结果更容易对比不同提交之间的回归。当前资源 JSON loader benchmark
会经过 `memory_file_system`、`file_source`、`provider`、输入流和 JSON parser，但不触碰真实磁盘。

## 分配统计

`caors_benchmarks` 在时间统计之外会额外打印 `Allocation samples`。这些样本使用
`tracking_resource` 记录分配次数、分配字节、峰值和退出样本后的 live 字节数，用来辅助判断：

- `serialization.to_value`：DOM 构造阶段是否产生大量临时分配。
- `serialization.serialize_json_to_reused_buffer`：复用输出 buffer 后是否仍有内部容器分配。
- `serialization.parse_json`：JSON parser 当前需要通过默认 resource 观察分配；这也提示后续 parser
  应继续收敛到显式 `memory_resource`。
- `resources.load_handle_into.json_uncached`：provider -> JSON loader -> cache 路径的总分配情况。

分配统计不是耗时 benchmark。它用于发现分配热点和泄漏风险，正式性能对比仍以 Release benchmark
同机重复运行结果为准。

## 后续适合拆分的专项

- 资源系统：provider、resolver、asset_manager 构建、跨类型 preload 和 async request。
- IO 系统：本地文件系统、挂载路径解析、字节流读写和大文件/小文件混合负载。
- 对象系统：`object_id` 生成、对象注册、handle/ref 查询和对象工厂。
- 序列化文本后端：JSON 文本读写、调用方提供 buffer、不同 field policy。
- 内存系统：`memory::resource`、分配器、策略资源和 debug tracking。

资源、IO 和异步任务受外部环境、线程调度、缓存状态影响更大，建议保持独立目标和独立输入数据。

## 约定

- benchmark 源码放在 `benchmarks/`，目标名使用 `caors_<topic>_benchmarks` 或
  当前聚合目标 `caors_benchmarks`。
- benchmark 目标只在 `CAORS_BUILD_BENCHMARKS=ON` 时加入构建。
- 用例应优先测公开 API。普通动态调用优先覆盖 `meta::value` / capability facade；
  只有在比较精确 C++ 语义或高级扩展成本时才单独测 `any`、`call_any()` 或 raw ops。
- 每个用例应使用固定输入，避免把日志、文件系统、网络或随机数据成本混入基础 API 测量。
