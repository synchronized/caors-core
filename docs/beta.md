<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Beta Checklist

CAORS 当前仍处于 beta 前收口阶段。项目早期不为破坏性 API 调整保留兼容入口；进入 beta 前需要冻结一组推荐主入口，并把仍可变化的实验性能力明确写入文档。

## 推荐稳定主入口

- `core`：`string_name`、`guid`、`type_guid`、`result<T>`、CAORS 容器别名。
- `memory`：`memory_resource`、`allocator<T>`、`make<T>()`、`destroy<T>()` 和基础调试资源。
- `meta`：`registration::type_()` / `registration::enum_()`、`meta::type`、属性、方法、constructor/factory capability、`meta::value` / `any` / `any_ref`。
- `serialization`：`serialize`、`deserialize`、`to_value`、`to_string`、reader/writer、`context` 配置和 resolver。
- `io`：`file_system`、`input_stream` / `output_stream`、native/memory/mount/resolver 后端。
- `resources`：`resource`、`ref<T>`、`handle<T>`、`cache<T>`、`provider`、`database`、同步/异步加载和依赖图。
- `assets`：`project_config`、`project_config_loader`、`asset_manager`、typed factory 注册入口和 JSON 适配。
- `tasks`：`executor`、`execution_context`、`inline_executor`、`thread_pool`、`completion_queue` 和基础
  request。
- `events`：`delegate`、`signal`、`sink`、`connection`、`dispatcher`。
- `math`：基础向量、矩阵、四元数、几何和相机 helper。

## Beta 前必须完成

- 主文档、示例和测试都使用同一套推荐 API，不再展示已删除或不推荐的兼容入口。
- `docs/api_stability.md`、`docs/roadmap.md` 和主题文档对实验性能力的描述一致。
- Debug 全量测试通过；Release benchmark 能稳定生成一次基线数据。
- `examples/beta/` 覆盖资源主路径：project config、VFS/provider、同步加载、异步加载、预加载、resolver、reload/save 和 typed factory options。
- 公开头中的高级入口有清楚边界说明：所有权、生命周期、线程安全和失败策略。

## 仍属于实验性

- `meta::value` fast path 扩展到 math 类型或更完整动态调用优化。
- 完整 Proxy DSL、模板方法代理、重载自动选择和跨 DLL capability 协议。
- registry replace、dry-run 校验、事务式批量注册和热重载注册。
- 二进制序列化后端、跨平台字节序策略和更完整 schema migration。
- 资源包、远程资源、调试 file server、导入管线、资源数据库和编辑器工具层。
- 任务调度的优先级、阻塞等待、协程适配、任务图、work stealing 和平台异步 IO。

## 建议验证命令

```sh
cmake --build --preset windows-clang-debug
ctest --preset windows-clang-debug --output-on-failure
cmake --build --preset windows-clang-release --target caors_benchmarks
```
