<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 文档物理目录与命名整理

## 背景与范围

- 日期：2026-10-19；整理基线：`3ead5fd`，分支：`docs/documentation-standards`。
- 上一轮只在索引中分类，保留了根目录专题正文和三个旧路径导航；本轮按用户要求完成物理归类。
- 本记录取代前次审查中“保留平铺目录和旧路径占位”的决定。现行约束以
  [文档规范](../../DOCUMENTATION_GUIDE.md)为准，具体入口见[文档中心](../README.md)。

## 完成的目录归类

- `docs/` 根目录仅保留 `README.md`、`roadmap.md`，分别承担入口和全局方向摘要。
- 5 份操作文档归入 `guides/`，17 份 API/行为/政策文档归入 `reference/`，1 份架构模型归入 `concepts/`。
- `checklists/`、`plans/`、`records/` 分别保留执行条件、实施设计、真实执行证据；各分类由 README 导航。
- 新增独立教程或 ADR 时再建立对应目录，本次没有建立空教程或决策目录。
- 删除三个旧路径导航页，仓库内引用直接指向正文；旧文件名集中保存在下方迁移表，便于修复外部引用。

## 文件名审查结论

- `value.md` 覆盖多种 Meta 值、argument 与 view，改为 `meta-values.md`，避免被误认为单个 value 类型。
- `meta-properties.md` 同时维护属性、成员/全局方法，改为 `meta-properties-and-methods.md`。
- `object.md` 对应整个 objects 模块，改为 `objects.md`；公共 C++ 头 `caors/object.h` 不受文档名称影响。
- `registration.md` 是类型注册与初始化任务，改为 `type-registration.md`，与插件协议注册区分。
- `dll-plugins.md` 涵盖跨平台动态插件接入及生命周期，改为 `plugin-integration.md`，避免 Windows DLL 范围误解。
- 其余 Reference 使用实际模块或契约名称；Guide 使用操作主题；所有正文名为小写英文连字符。
- 逐份核对计划标题与主题：现有迁移、重构、验证计划名称表达设计范围，即使验收完成也保留原计划名；
  不把计划机械改名为 Record。历史扫描日期未知的记录保留 `undated`，不补造时间。
- README 为目录索引保留约定名称，根路线图属于明确的全局例外；其余正文均位于职责目录。

## 路径迁移表

旧路径为历史信息，新路径链接指向当前正文；不维护旧路径副本。

| 旧路径 | 新正文 | 处理 |
| --- | --- | --- |
| `docs/build.md` | [docs/guides/build.md](../guides/build.md) | 按职责移动 |
| `docs/benchmarking.md` | [docs/guides/benchmarking.md](../guides/benchmarking.md) | 按职责移动 |
| `docs/development.md` | [docs/guides/development.md](../guides/development.md) | 按职责移动 |
| `docs/registration.md` | [docs/guides/type-registration.md](../guides/type-registration.md) | 移动并更名 |
| `docs/dll-plugins.md` | [docs/guides/plugin-integration.md](../guides/plugin-integration.md) | 移动并更名 |
| `docs/module-boundary.md` | [docs/concepts/module-boundary.md](../concepts/module-boundary.md) | 按职责移动 |
| `docs/api-stability.md` | [docs/reference/api-stability.md](../reference/api-stability.md) | 按职责移动 |
| `docs/assets.md` | [docs/reference/assets.md](../reference/assets.md) | 按职责移动 |
| `docs/config.md` | [docs/reference/config.md](../reference/config.md) | 按职责移动 |
| `docs/core.md` | [docs/reference/core.md](../reference/core.md) | 按职责移动 |
| `docs/error.md` | [docs/reference/error.md](../reference/error.md) | 按职责移动 |
| `docs/events.md` | [docs/reference/events.md](../reference/events.md) | 按职责移动 |
| `docs/io.md` | [docs/reference/io.md](../reference/io.md) | 按职责移动 |
| `docs/math.md` | [docs/reference/math.md](../reference/math.md) | 按职责移动 |
| `docs/memory.md` | [docs/reference/memory.md](../reference/memory.md) | 按职责移动 |
| `docs/meta.md` | [docs/reference/meta.md](../reference/meta.md) | 按职责移动 |
| `docs/resources.md` | [docs/reference/resources.md](../reference/resources.md) | 按职责移动 |
| `docs/serialization.md` | [docs/reference/serialization.md](../reference/serialization.md) | 按职责移动 |
| `docs/tasks.md` | [docs/reference/tasks.md](../reference/tasks.md) | 按职责移动 |
| `docs/traits.md` | [docs/reference/traits.md](../reference/traits.md) | 按职责移动 |
| `docs/object.md` | [docs/reference/objects.md](../reference/objects.md) | 移动并更名 |
| `docs/value.md` | [docs/reference/meta-values.md](../reference/meta-values.md) | 移动并更名 |
| `docs/meta-properties.md` | [docs/reference/meta-properties-and-methods.md](../reference/meta-properties-and-methods.md) | 移动并更名 |
| `docs/api_stability.md` | [docs/reference/api-stability.md](../reference/api-stability.md) | 删除旧导航，直接引用正文 |
| `docs/beta.md` | [docs/checklists/beta-readiness.md](../checklists/beta-readiness.md) | 删除旧导航，直接引用正文 |
| `docs/plans/memory-domain-audit.md` | [docs/records/2026-08-04-memory-domain-audit.md](2026-08-04-memory-domain-audit.md) | 删除旧导航，直接引用正文 |

## 内容与引用范围

同步所有自有 Markdown 的入链、出链和现行代码样式路径，分类索引使用新文件名。
旧记录中的历史文件名、提交、命令、测量数据与结论保留，只更新用于导航的实际链接。
前次审查记录添加“已被本次布局整理取代”的说明，不把其当时的验证结果改写为本次结果。

本次整理不改变库代码、CMake 目标、公共头、测试或性能测量；文档中必要的接口示例和说明不因移动而删除。

## 验证范围

检查物理目录、文件名、分类索引的可达性、Markdown 文件链接和标题锚点、旧路径残留、UTF-8/LF/许可证及改动范围。
外部旧书签无法由仓库检查保证，需按迁移表更新；本次没有重新运行文档代码示例或历史基准。

## 本次检查结果

- 92 份自有 Markdown、294 个本地文件链接和标题锚点检查通过。
- 从文档中心可到达全部 67 份 docs 文档，没有排除旧导航页或孤立文档的例外。
- `docs/` 根目录恰好为两个全局入口；分类目录、正文命名和 26 条旧路径的退役/新目标检查通过。
- 23 份移动正文的代码块对比仅发现开发规范中目录示意图注释更新，运行命令和接口示例未变。
- 自有文件改动仅涉及 Markdown，UTF-8、LF、许可证头和 `git diff --check` 检查通过。
- 没有重新执行 CMake、CTest、代码示例或性能基准，没有将历史结果改写为新测试通过。
