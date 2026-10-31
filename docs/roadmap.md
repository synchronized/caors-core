<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# CAORS 路线图

本页维护后续方向、依赖和阶段摘要，不枚举当前 API，也不维护已完成提交清单。
单项任务的范围与状态由[计划索引](plans/README.md)指向的 Plan 维护，当前能力见
[文档中心](README.md)，稳定性边界见[API 政策](reference/api-stability.md)。

最新版本为 [v0.7.0：Emscripten 单线程平台接入](versions/v0.7.0.md)，
已发布 alpha 源码并完成验收与公开下载复核；详细范围与发布证据由版本页关联。
[v0.3.0：异步 loader 与宿主 I/O 接入](versions/v0.3.0.md)保留其范围及发布证据。
[v0.4.0：统一 VFS 与异步范围读取](versions/v0.4.0.md)保留其范围及发布证据；固定 Linux 性能复验继续由[来源快照计划](plans/provider-snapshot-cost.md)跟踪。
[v0.2.0](versions/v0.2.0.md) 已完成实现验收，公开发布状态由其版本页维护，既有延期复验继续由原计划跟踪。
[v0.5.0：资源显式刷新与分块读取](versions/v0.5.0.md)保留其范围、发布证据与固定 Linux 性能复验延期。
CAORS 独立 Web 平台已交付；Gneiss 联调继续由关联计划跟踪，不计为已验收。
以下其他候选方向尚不构成承诺排期，启动时再明确范围和验收。
原有逐 API 完成清单归档到[整理前基线](records/history/2026-10-19-roadmap-baseline.md)。

## 0. Core 基础设施

基础身份、容器、错误和 IO 底座已提供；当前用法见[core](reference/core.md)、[error](reference/error.md)、[IO](reference/io.md)。
`small_vector` 的正式性能与业务接入评估继续按[容器计划](plans/small-vector.md)跟踪。
`stable_vector` 仅在真实地址稳定需求出现后另立范围。
IO 按[收敛计划](plans/vfs-api-consolidation.md)减少重复路由及来源转换，保留同步能力并复用底层实现；不以删除 file_system 为硬性出口。

## 0.5. Math 基础数学类型

现有类型与适配以[math 参考](reference/math.md)为准。后续评估：

- 基于实际脚本、编辑器或动态调用负载，决定 vec2/vec3/vec4/quat 是否进入 meta::value 快路径。
  较大类型优先维持 any/any_ref 或 capability 路径，测量方法见[Benchmark 指南](guides/benchmarking.md#math-fast-path-判断)。
- 根据真实渲染负载评估 mat4 乘法展开、SIMD 和 affine transform 优化。
- 在已有双精度、整型向量和相机 helper 基础上评估更完整相机、碰撞和坐标系辅助能力。

## 1. 类型元数据底座

当前模型、注册和查询见[meta](reference/meta.md)。后续 Proxy 方向包括减少 ops/view/model 样板、
模板方法、重载选择、引用返回与借用生命周期策略；无宏 DSL、未来静态反射或代码生成按实际需求评估。
独立稳定跨 ABI capability 协议不是现有能力，约束见[模块边界](concepts/module-boundary.md)。

## 2. 定制化分配器

当前资源与策略见[memory](reference/memory.md)。分级池、内存域和固定环境复验由
[计划索引](plans/README.md)跟踪，不在本页复制验证矩阵。
候选扩展包括同步包装器和可选第三方分配后端；引入前需证明工作负载收益并说明依赖影响。

## 3. 对象系统

当前身份、指针、handle、manager 和 resolver 见[对象参考](reference/objects.md)。
后续评估完整对象/场景图存档的版本、共享身份、循环引用和加载修复策略，以及线程安全对象容器或数据库。

## 4. Meta 属性和方法

当前字段、调用和能力契约见[属性与方法参考](reference/meta-properties-and-methods.md)。后续动态访问优化复用
类型元数据及值系统方向，以[既有性能记录](../benchmarks/results/README.md)和业务负载决定独立计划。

## 5. 序列化

JSON、MessagePack、codec 和 resolver 的现状及限制见[序列化参考](reference/serialization.md)。
后续 schema 演进、字段迁移与未知字段保留需要先定义兼容和失败行为，不能以已有底座替代完整承诺。

## 6. 资源系统

当前资源、清单、provider/database 和 Assets 编排见[resources](reference/resources.md)、[assets](reference/assets.md)。
Tasks/Resources 协程迁移和延期验证的状态见[计划索引](plans/README.md)。后续方向：

- 异步 loader 已按[计划](plans/asynchronous-resource-loader.md)交付；资源包、远程资源与异步 saver 仍为独立候选。
- 跨 cache/version 的引用失效、弱引用和调试诊断策略。
- 跨对象、场景和资源包的依赖图，以及 reload 影响范围计算。
- 编辑器 file server 与 runtime remote file system，保持逻辑 URI 与具体后端解耦。
- 进程内与跨进程文件服务协议、热更新、导入管线、资源数据库及工具层。
  仅在需要独立生命周期、后台工作或 UI 协议时拆分专用模块。

## 7. 任务调度

统一 context、轻量 executor 和协程请求等待已有迁移验收；内置后端已[移除 Asio](plans/standard-thread-backends.md)，延期扩展验证见
[Tasks 计划](plans/asio-task-redesign.md)。当前 API 见[tasks](reference/tasks.md)。
优先级、阻塞等待、取消传播、timer/超时、任务图、work stealing 和平台异步 I/O 作为独立需求评估。
Task SBO、operation allocator 和批量调度仅在独立实验及真实负载证明收益后实施。

## 8. 事件与委托

当前同步模型见[events](reference/events.md)。跨线程事件队列和异步投递需另行明确所有权、线程与调度边界，
不把现有单线程约束隐式扩展为线程安全保证。
