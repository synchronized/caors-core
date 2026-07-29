<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# CAORS 开发计划

本目录集中存放尚在讨论、实施或已经完成但需要保留设计依据的重构与迁移计划。
面向使用者的稳定主题说明继续放在 `docs/` 根目录。

## Runtime 与插件

- [plugin-build-compatibility.md](plugin-build-compatibility.md)：host/plugin 编译器、CRT、
  STL ABI、架构和指针宽度的加载期兼容性校验计划。
- [library-plugin-system.md](library-plugin-system.md)：跨平台动态库封装、稳定插件 ABI、
  manifest、依赖、生命周期、registration scope 与安全卸载计划。
- [registry-query-fast-path.md](registry-query-fast-path.md)：基于不可变 snapshot/layer 的
  registry 查询无锁快路径和插件 layer 调用 guard 收敛计划。

## Memory

- [scoped-temporary-memory.md](scoped-temporary-memory.md)：支持嵌套 checkpoint/rewind 的
  线程局部临时资源，以及 serialization 双资源接入计划。

## Meta

- [detail-api-boundary-redesign.md](detail-api-boundary-redesign.md)：meta 内部 detail API
  分层与边界收敛计划。
- [meta-type-data-redesign.md](meta-type-data-redesign.md)：静态类型数据与 registry 注册数据
  重构需求、外部方案调研和分阶段实施计划。
- [meta-record-naming-migration.md](meta-record-naming-migration.md)：meta 运行时记录的
  `*_record` 命名迁移计划。
- [meta-wrapper-model-redesign.md](meta-wrapper-model-redesign.md)：meta 内部 wrapper、ops
  和模板 model 的行为内聚计划。
- [meta-erased-wrapper-redesign.md](meta-erased-wrapper-redesign.md)：将 property、method 的
  模板状态从固定 record/ops 中抽离为 opaque wrapper 的后续迁移计划。
- [value-boundary-redesign.md](value-boundary-redesign.md)：保留 `type_record` 单一事实源，
  收敛 `any_ref`、object factory 和 adapter/ODR 边界的开发计划。
- [enum-view-migration.md](enum-view-migration.md)：enum capability 与实例 view 的迁移计划。
- [identity-id-unification.md](identity-id-unification.md)：统一 type、capability
  等进程内 ID 的底层发号逻辑，并保留外层强类型 ID 边界。

## Object

- [object-type-identity-migration.md](object-type-identity-migration.md)：object 类型身份命名
  收敛计划。

## Serialization

- [codec-view-migration.md](codec-view-migration.md)：serialization codec 的 capability/view
  职责收敛计划。
- [non-owning-serialization.md](non-owning-serialization.md)：借用字符串、调用方 span 缓冲区和
  对象弱引用的显式反序列化条件。
- [messagepack-backend.md](messagepack-backend.md)：零第三方依赖的标准 MessagePack
  writer、DOM parser 和 typed serialization 后端计划。
