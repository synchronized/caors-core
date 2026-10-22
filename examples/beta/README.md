<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Beta Examples

本目录展示 beta 前推荐优先验证的资源系统主路径。示例代码只覆盖公共 API 的最小组合，完整设计边界见
`docs/reference/assets.md`、`docs/reference/resources.md` 和 `docs/reference/serialization.md`。

## 推荐阅读顺序

1. `assets_sync_load.cpp`：最小同步加载链路，验证 `asset_manager`、manifest、provider、loader 和
   cache 的基础组合。
2. `assets_async_load.cpp`：同步链路扩展到异步 request、后台加载和主线程 execution context commit。
3. `resource_reload.cpp`：验证已缓存资源的同步重载和失败保留旧资源语义。
4. `asset_pipeline.cpp`：串起 `project_config -> VFS/provider -> asset_manager -> async preload -> resolve -> reload/save`。
5. `config_schema.cpp`：展示 typed factory options 的校验入口。
6. [assets_native_async.cpp](assets_native_async.cpp)：宿主延迟完成 loader、默认调度和 asset_group 共享请求；仅模拟 I/O 事件，不依赖 libuv。

7. [io_chunked_read.cpp](io_chunked_read.cpp)：串行逐块消费、完整结果聚合和调用方显式关闭。

## 主流程

beta 前资源路径按下面公式收敛：

```text
注册资源类型和配置类型
  -> 读取 project_config
  -> project_builder::build()
  -> load / preload
  -> serialization context resolver 解析 resource_ref<T>
  -> reload / save
```

复杂 factory options 优先定义强类型配置结构，再使用
`asset_manager::register_typed_*_factory<Options, Create>()` 注册。asset manager 会读取 DOM、应用属性默认值、
拒绝未知字段并执行 `validate_config()`；raw factory 只做手动读取，不提供内置 schema 校验。

`assets_async_vfs.cpp` 演示 manager → 来源快照 → 原生内存打开 → 有界范围读取 → 显式关闭 → cache 提交。
它使用主动 pump，不代表 libuv 联合接入；取消转交 worker，真实宿主还需提供 loop 寿命和全局预算。

## 构建目标

```sh
cmake --build --preset windows-clang-debug --target caors_example_beta_assets_sync_load
cmake --build --preset windows-clang-debug --target caors_example_beta_assets_async_load
cmake --build --preset windows-clang-debug --target caors_example_beta_resource_reload
cmake --build --preset windows-clang-debug --target caors_example_beta_asset_pipeline
cmake --build --preset windows-clang-debug --target caors_example_beta_config_schema
cmake --build --preset windows-clang-debug --target caors_example_beta_assets_async_vfs
cmake --build --preset windows-clang-debug --target caors_example_beta_assets_native_async
cmake --build --preset windows-clang-debug --target caors_example_beta_io_chunked_read
```
