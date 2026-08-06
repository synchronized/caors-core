<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# CAORS 开发计划

本目录集中存放尚在讨论、实施或已经完成但需要保留设计依据的重构与迁移计划。
面向使用者的稳定主题说明继续放在 `docs/` 根目录。

## 状态索引

### 进行中

- [asio-task-redesign.md](asio-task-redesign.md)：引入 standalone Asio，统一 execution context、
  轻量 executor、主线程 poll、线程池和可选 C++20 coroutine 适配。
- [resource-coroutine-pipeline.md](resource-coroutine-pipeline.md)：让 resources 共享请求支持多 coroutine
  等待，并分阶段收敛 assets 的执行目标与便利 API。
- [size-class-pool.md](size-class-pool.md)：分级中小块池、确定性尺寸路由、上游回退、统计与
  性能验证计划；核心实现、Windows 全矩阵、Linux Clang/GCC/ASan 和代表性基准已完成，等待固定
  Linux 环境的 UBSan、正式性能复验与真实业务 workload。

### 延期复验

- [memory-domain-redesign.md](memory-domain-redesign.md)：四类内存域实现、正式文档及 Windows、
  Linux Clang/GCC/ASan 集成验证已完成；固定 Linux 环境 UBSan 延期执行。
- [meta-wrapper-performance-validation.md](meta-wrapper-performance-validation.md)：Windows 评估与
  阶段性结论已完成；Linux 正式性能复验等待固定环境。
- [meta-erased-wrapper-redesign.md](meta-erased-wrapper-redesign.md)：核心迁移与 Windows 性能复验
  已完成；固定 Linux 环境部署后补充跨平台性能数据。
- [small-vector.md](small-vector.md)：核心容器、meta/serialization 适配、代表性基准和 Windows、
  Linux Clang/GCC/ASan 验证已完成；固定 Linux 环境性能复验延期。

### 已完成

- Runtime 与插件：`plugin-build-compatibility`、`library-plugin-system`、
  `registry-overlay-safe-unload`、`registry-query-fast-path`。
- 构建与内存：`linux-validation-ci`、`installed-package-consumer-validation`、
  `scoped-temporary-memory`、`pmr-resource-adapters`、`memory-debug-guards`。
- Meta：`detail-api-boundary-redesign`、`meta-type-data-redesign`、
  `meta-record-naming-migration`、`meta-wrapper-model-redesign`、`value-boundary-redesign`、
  `enum-view-migration`、`identity-id-unification`、`type-identity-state-redesign`。
- Object：`object-type-identity-migration`。
- Serialization：`codec-view-migration`、`non-owning-serialization`、`messagepack-backend`。

已完成计划继续保留，用作设计决策、迁移范围和验证依据；其中明确列出的未来方向不自动视为新的
进行中任务。

## Runtime 与插件

- [plugin-build-compatibility.md](plugin-build-compatibility.md)：host/plugin 编译器、CRT、
  STL ABI、架构和指针宽度的加载期兼容性校验计划。
- [library-plugin-system.md](library-plugin-system.md)：跨平台动态库封装、稳定插件 ABI、
  manifest、依赖、生命周期、registration scope 与安全卸载计划。
- [registry-query-fast-path.md](registry-query-fast-path.md)：基于不可变 snapshot/layer 的
  registry 查询无锁快路径和插件 layer 调用 guard 收敛计划。

## 构建与验证

- [linux-validation-ci.md](linux-validation-ci.md)：Linux Clang/GCC、Static/Shared、
  Debug/Release、ASan、POSIX 插件卸载与 GitHub Actions 验证计划。
- [installed-package-consumer-validation.md](installed-package-consumer-validation.md)：安装包导出、
  独立 `find_package` consumer、Static/Shared 运行与可重定位验证计划。

## Tasks

- [asio-task-redesign.md](asio-task-redesign.md)：standalone Asio 依赖、execution context、轻量 executor、
  completion queue/thread pool 收敛和可选 C++20 coroutine 适配计划。
- [resource-coroutine-pipeline.md](resource-coroutine-pipeline.md)：资源请求等待协议、coroutine 适配、
  assets 边界、生命周期和性能验证计划。

## Memory

- [memory-domain-redesign.md](memory-domain-redesign.md)：内部长期元数据、registry layer、同步临时
  工作区与用户结果内存的统一生命周期域计划。
- [memory-domain-audit.md](memory-domain-audit.md)：全仓资源入口、owner、释放时机、线程与插件边界的
  阶段 0 审计基线。
- [scoped-temporary-memory.md](scoped-temporary-memory.md)：支持嵌套 checkpoint/rewind 的
  线程局部临时资源，以及 serialization 双资源接入计划。
- [pmr-resource-adapters.md](pmr-resource-adapters.md)：CAORS 与标准 PMR 资源的双向
  非拥有适配、失败语义和生命周期边界计划。
- [memory-debug-guards.md](memory-debug-guards.md)：显式调试资源的前后越界检测、分配/释放填充和
  违规报告计划。
- [size-class-pool.md](size-class-pool.md)：面向中小型混合分配的非线程安全分级块池、上游回退和
  碎片率验证计划。

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
- [meta-wrapper-performance-validation.md](meta-wrapper-performance-validation.md)：量化
  property/method wrapper 的布局、注册分配和运行时调用成本，并据此决定是否优化。
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
