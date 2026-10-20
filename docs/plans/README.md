<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# CAORS 开发计划

本目录集中存放尚在讨论、实施或已经完成但需要保留设计依据的重构与迁移计划。
当前 API 契约放在 `docs/reference/`，操作指南放在 `docs/guides/`，本目录仅维护实施方案与状态。

计划是单项工作的范围、设计和待办状态维护入口；本页和路线图仅保留摘要与链接。
新增或收尾计划遵循 [项目文档规范](../../DOCUMENTATION_GUIDE.md)：详细执行证据写入独立记录，
性能数据保存在 `benchmarks/results/`，已验证的长期行为更新到对应专题文档。
延期复验应注明剩余范围、原因、恢复条件及是否阻塞本次验收，不能将延期写成已验证通过。

## 状态索引

### v0.6.0 实施中

- [第三方依赖源码缓存迁移](dependency-source-cache.md)：锁定下载、离线、并发发布与父项目复用。

### 版本集成已验收

上一轮版本的范围、批次和依赖见 [v0.2.0](../versions/v0.2.0.md)。以下六项实现与契约验收已完成；版本元数据与公开发布状态由版本页维护：

- [runtime 集成契约](runtime-integration-contracts.md)：C51-01，共享与私有 runtime 边界及跨模块验收。
- [可嵌入 CMake](embedded-cmake-configuration.md)：C51-03，独立链接类型、依赖隔离与选项命名。
- [库分发契约](library-distribution-contracts.md)：C51-04，PIC、导出符号、版本和安装扩展验证。
- [资源模块寿命](resource-module-lifetime.md)：C51-02，强弱引用、allocator、异步回调与卸载。
- [独立订阅取消](resource-request-subscriptions.md)：C51-05，共享加载与消费者终态隔离。
- [外部 executor](external-executor-adapter.md)：C51-06，宿主投递、关闭与任务所有权。

### v0.3.0 已完成开发验收

- [异步资源 loader](asynchronous-resource-loader.md)：异步协议、默认调度与 asset_group 已完成实现和集成验收；公开状态见版本页。

### v0.4.0 已完成开发验收

- [统一 VFS 与异步范围读取](asynchronous-vfs.md)：核心协议与资源链路已实现并完成集成验收；发布状态见版本页。

### v0.5.0 已完成限定范围

- [资源显式刷新](resource-refresh.md)：C51-08，已完成本机及跨平台集成验收。
- [分块读取与逐块消费](chunked-file-reading.md)：C51-09，已完成本机及跨平台集成验收。
- [来源快照成本评估](provider-snapshot-cost.md)：C51-10，Windows 对比与同次捕获优化已完成，固定 Linux 性能复验已获准延期。

### 待实施

- [release-automation.md](release-automation.md)：人工发布流程的工具化范围，尚未排期或实现脚本/Release workflow。

### 进行中

- [size-class-pool.md](size-class-pool.md)：分级中小块池、确定性尺寸路由、上游回退、统计与
  性能验证计划；核心实现、Windows 全矩阵、Linux Clang/GCC/ASan 和代表性基准已完成，等待固定
  Linux 环境的 UBSan、正式性能复验与真实业务 workload。

### 延期复验

- [asio-task-redesign.md](asio-task-redesign.md)、
  [resource-coroutine-pipeline.md](resource-coroutine-pipeline.md)：迁移、Windows 八组验证、Linux 完整
  CI、ASan/UBSan、安装包及 Windows 调度/等待基准已完成；固定 Linux 环境正式性能复验、
  真实业务负载和完整无异常运行矩阵保留为后续扩展验证。

- [memory-domain-redesign.md](memory-domain-redesign.md)：四类内存域实现、正式文档及 Windows、
  Linux Clang/GCC/ASan 集成验证已完成；固定 Linux 环境 UBSan 延期执行。
- [meta-wrapper-performance-validation.md](meta-wrapper-performance-validation.md)：Windows 评估与
  阶段性结论已完成；Linux 正式性能复验等待固定环境。
- [meta-erased-wrapper-redesign.md](meta-erased-wrapper-redesign.md)：核心迁移与 Windows 性能复验
  已完成；固定 Linux 环境部署后补充跨平台性能数据。
- [small-vector.md](small-vector.md)：核心容器、meta/serialization 适配、代表性基准和 Windows、
  Linux Clang/GCC/ASan 验证已完成；固定 Linux 环境性能复验延期。

### 已完成

- [标准库调度后端](standard-thread-backends.md)：删除 Asio；Windows 八组、Linux 完整 CI、重复回归和安装迁移通过。

- Tasks 与 Resources：`asio-task-redesign`、`resource-coroutine-pipeline` 的迁移验收；
  固定环境性能复验等扩展验证见上方延期复验。

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

- [registry-overlay-safe-unload.md](registry-overlay-safe-unload.md)：插件 registry layer、动态库 lease、
  覆盖与安全卸载的已完成设计和迁移依据。
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

- [asynchronous-resource-loader.md](asynchronous-resource-loader.md)：宿主异步 I/O 的启动/完成协议、同步兼容及共享请求接入。
- [asio-task-redesign.md](asio-task-redesign.md)：standalone Asio 依赖、execution context、轻量 executor、
  completion queue/thread pool 收敛和可选 C++20 coroutine 适配计划。
- [resource-coroutine-pipeline.md](resource-coroutine-pipeline.md)：资源请求等待协议、coroutine 适配、
  assets 边界、生命周期和性能验证计划。

## Memory

- [memory-domain-redesign.md](memory-domain-redesign.md)：内部长期元数据、registry layer、同步临时
  工作区与用户结果内存的统一生命周期域计划。
- [scoped-temporary-memory.md](scoped-temporary-memory.md)：支持嵌套 checkpoint/rewind 的
  线程局部临时资源，以及 serialization 双资源接入计划。
- [pmr-resource-adapters.md](pmr-resource-adapters.md)：CAORS 与标准 PMR 资源的双向
  非拥有适配、失败语义和生命周期边界计划。
- [memory-debug-guards.md](memory-debug-guards.md)：显式调试资源的前后越界检测、分配/释放填充和
  违规报告计划。
- [size-class-pool.md](size-class-pool.md)：面向中小型混合分配的非线程安全分级块池、上游回退和
  碎片率验证计划。

## Meta

- [type-identity-state-redesign.md](type-identity-state-redesign.md)：运行期类型身份状态、静态类型模型、
  注册表与 DLL 身份一致性的已完成迁移依据。
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

## 相关证据

重构前审计与历史验证由[记录索引](../records/README.md)维护，不再作为实施计划分类。
