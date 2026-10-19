<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# API 稳定性与公开边界

CAORS 当前处于 alpha 阶段，允许为模块边界、命名、所有权和生命周期语义调整公共 API，
暂不设置弃用过渡期。下方候选目录不构成稳定承诺；进入 beta 需要通过
[Beta 验收清单](../checklists/beta-readiness.md)。候选与实验性范围统一在本页维护。

## Beta 稳定候选

| 模块 | 候选范围 | 当前行为参考 |
| --- | --- | --- |
| core | 字符串名、GUID、结果、基础容器 | [core](core.md)、[error](error.md) |
| memory | resource、allocator、默认资源、基础策略和调试资源 | [memory](memory.md) |
| io | path/location/uri、文件系统、同步 stream、status/list/walk | [io](io.md) |
| meta | registration、type、property/method、instance、value/any/any_ref/variant、capability facade | [meta](meta.md)、[属性与方法](meta-properties-and-methods.md)、[值](meta-values.md) |
| objects | 对象身份、指针、handle、引用计数和 manager/registry 生命周期 | [object](objects.md) |
| serialization | context、序列化入口、reader/writer、JSON/MessagePack、配置校验和 resolver | [serialization](serialization.md) |
| resources | 身份、引用、cache/provider/loader/database、同步与异步请求、依赖与通知、manifest 枚举叠加 | [resources](resources.md) |
| assets | project_config、asset_manager、source/type factory、typed options、JSON 适配 | [assets](assets.md) |
| events | 同步 signal/dispatcher、连接与委托 | [events](events.md) |
| tasks | task、轻量 executor、外部 executor 适配、execution_context、inline_executor()、thread_pool、request；协程适配显式包含 | [tasks](tasks.md) |
| math | 基础数学值类型、变换、几何与相机 helper | [math](math.md) |

具体接口与限制以对应参考和公共头为准；候选表示评估范围，不保证某个版本冻结全部接口。
Tasks/Resources 协程等待和 MessagePack 已有实现，不再归入“尚未实现”项目。
任务公共抽象与内置基础后端的职责分开；适用场景、调度限制和验证边界统一见
[Tasks 定位与适用范围](tasks.md#定位与适用范围)。内置后端的功能验收不等于性能或长期运行保证。

## 实验性入口与后续方向

- Proxy DSL 的模板方法、重载选择、引用返回与借用策略等增强。
- schema 版本迁移、字段重命名/删除及未知字段保留的进一步扩展。
- 数学类型动态值快路径、脚本/编辑器 ABI 和更高层动态调用优化。
- 高层资源热重载策略、资源包、远程 file server、网络/HTTP 后端和批量异步请求调度。
- 调度优先级、任务图、work stealing 和平台异步 I/O 等调度扩展。
- benchmark 的用例组织、输入和输出格式。

这里不把已有底座等同于完整上层系统，也不承诺以上方向的排期；详细方向见[路线图](../roadmap.md)。
独立稳定的跨编译器 capability 协议不是现有能力，native C++ ABI 约束见
[模块边界](../concepts/module-boundary.md)。

## 公开边界规则

API 可见性、detail、根命名空间别名和 facade 规则统一由
[开发规范](../guides/development.md#公开-api-边界)维护；跨二进制传参与 ABI 约束统一由
[模块边界](../concepts/module-boundary.md)维护。本页只维护稳定性和候选范围。

## 扫描与验收

扫描命令、Beta Gate 和未验证项记录要求统一见[Beta 验收清单](../checklists/beta-readiness.md)。
[历史扫描摘要](../records/history/api-boundary-scan-undated.md)没有被验证提交信息，不能作为当前检查通过的结论。
