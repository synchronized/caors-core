<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# API 稳定性草案

CAORS 当前仍处于 alpha 阶段。现阶段允许为了模块边界、命名一致性、所有权和生命周期语义做破坏性 API
调整，不设置弃用过渡期。进入 beta 前再冻结一组主入口，并把实验性入口明确标注为后续可能变化。

## Beta 稳定候选

以下 API 适合作为 beta 第一批稳定候选：

- `core`：`string_name`、`guid`、`type_guid`、基础容器别名和错误/结果类型。
- `memory`：`memory_resource`、`allocator<T>`、全局默认资源、`unique_ptr<T>` 和基础策略资源。
- `io`：`path` / `location` / `uri`、`file_system`、`memory_file_system`、`native_file_system`、
  `mount_file_system`、`file_system_resolver`、同步 stream、`status()` / `list()` / `walk()`。
- `meta`：`registration::type_<T>()`、`registration::enum_<T>()`、`meta::type`、`property`、
  `method` / `method_set`、`instance`、`value`、`any` / `any_ref`、`variant` 和 capability facade。
- `objects`：对象指针、handle、引用计数和 manager/registry 生命周期入口。
- `serialization`：`context`、`serialize()`、`deserialize()`、`to_value()`、`to_string()`、reader/writer、
  JSON 便捷路径、config validation 和资源/object resolver 适配。
- `resources`：`resource`、`resource_ref<T>`、`cache<T>`、`provider`、`loader<T>`、`database`、
  同步 load/reload/preload、异步 request、dependency graph 和 reload 通知 helper。
- `assets`：`project_config`、`asset_manager`、file system/source/type factory、typed factory options、
  JSON project config 适配。
- `events` / `tasks`：同步 signal/dispatcher、executor、thread pool、completion queue 和 request 状态。

## 实验性入口

以下能力仍属于实验性或后续可破坏调整范围：

- 完整 Proxy DSL、模板方法代理、重载自动选择、引用返回策略和跨 DLL capability 协议。
- 完整 schema/metadata 系统，包括版本迁移、字段重命名/删除、未知字段保留和配置驱动运行期对象构造。
- `meta::value` 的小对象优化、动态调用快路径和后续脚本/编辑器 ABI。
- 资源热重载的高层策略、资源包、远程 file server、异步加载取消传播和批量请求调度策略。
- 网络后端、HTTP 文件系统、manifest-backed listing overlay 和编辑器实时替换协议。
- benchmark 输入、输出格式和用例拆分方式。

## 公开边界规则

- 普通用户优先包含聚合头或模块公开头，例如 `caors/meta.h`、`caors/resources.h`、`caors/assets.h`。
- 对外 API 不暴露 `detail` 类型作为参数、返回值、公开数据成员或公开继承基类。
- `detail` 可以出现在公开头的模板实现、私有成员和 friend 声明中，但不作为正常调用路径。
- 类型擦除函数表统一使用 `*_ops` 命名；用户侧优先通过 `*_capability` facade 或绑定后的 view 访问。
- 根命名空间只保留高频、跨模块、语义稳定的便捷别名；source/provider/loader/request/config/helper
  等组合件保留在对应模块命名空间。
- 示例、入门文档主路径和普通测试不直接使用 `meta::detail`、`serialization::detail`、`resources::detail` 或 raw ops；
  高级扩展章节可以解释 raw ops，但必须明确它不是普通用户入口。

## 扫描项

beta 前每轮 API 收敛建议执行以下扫描：

```sh
rg -n "get_raw_ops|deprecated|CAORS_.*DEPRECATED|#include \"caors/.*/detail/\"" include/caors docs examples tests
rg -n "meta::detail|serialization::detail|resources::detail|object::detail|caors::detail" docs examples tests
rg -n "namespace caors::|using .*::" include/caors
```

本轮扫描结论：

- `examples/` 没有 raw ops 或 `detail` 调用，示例路径保持公开 API 优先。
- `docs/meta.md` 仍会描述 `get_raw_ops<T>()` 这类高级扩展入口；这些入口不属于普通用户主路径。
- `tests/meta/static_type_record/`、`tests/meta/global/` 和 `tests/registration/init/` 仍有少量 `detail` 调用，属于注册表和静态类型数据
  内部边界测试，不作为普通测试写法。
- `include/caors/**/detail` 仍作为内部实现头存在，当前不要求删除；重点是避免进入用户示例和高层文档的默认路径。
- 根命名空间别名规则已经写入 `docs/development.md`，后续新增别名前应先确认是否属于高频稳定入口。

## Beta Gate

进入 beta 前至少完成：

- 全量构建、全量测试、示例聚合目标和 benchmark 冒烟运行通过。
- 文档、示例、测试和公开头中的主入口命名一致。
- `detail` 和旧兼容入口扫描无新增普通用户路径。
- `docs/roadmap.md` 中仍未实现的实验性项目不被文档描述为稳定 API。
- 资源、序列化、meta、objects 的最小闭环都有可运行 example 和对应测试。
