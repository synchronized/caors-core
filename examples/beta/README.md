<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Beta Examples

本目录展示 beta 前推荐优先验证的资源系统主路径。示例代码只覆盖公共 API 的最小组合，完整设计边界见
`docs/assets.md`、`docs/resources.md` 和 `docs/serialization.md`。

## 推荐阅读顺序

1. `assets_sync_load.cpp`：最小同步加载链路，验证 `asset_manager`、manifest、provider、loader 和
   cache 的基础组合。
2. `assets_async_load.cpp`：同步链路扩展到异步 request、后台加载和主线程 completion queue commit。
3. `resource_reload.cpp`：验证已缓存资源的同步重载和失败保留旧资源语义。
4. `asset_pipeline.cpp`：串起 `project_config -> VFS/provider -> asset_manager -> async preload -> resolve -> reload/save`。
5. `config_schema.cpp`：展示 source/type factory option schema 和 typed config options 的校验入口。

## 主流程

beta 前资源路径按下面公式收敛：

```text
注册资源类型和配置类型
  -> 读取 project_config
  -> asset_manager::build()
  -> load / preload
  -> serialization context resolver 解析 resource_ref<T>
  -> reload / save
```

复杂 factory options 优先定义强类型配置结构，再使用
`assets::read_typed_factory_options<T>()` 读取 DOM、应用属性默认值并执行 `validate_config()`。轻量插件或
无需结构体绑定的入口仍可使用 `factory_option_schema`。

## 构建目标

```sh
cmake --build --preset windows-clang-debug --target caors_example_beta_assets_sync_load
cmake --build --preset windows-clang-debug --target caors_example_beta_assets_async_load
cmake --build --preset windows-clang-debug --target caors_example_beta_resource_reload
cmake --build --preset windows-clang-debug --target caors_example_beta_asset_pipeline
cmake --build --preset windows-clang-debug --target caors_example_beta_config_schema
```
