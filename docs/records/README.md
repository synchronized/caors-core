<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 实施与验收记录

本目录维护实际执行、验收与历史审计证据；当前 API、方向与任务状态分别见
[文档中心](../README.md)、[路线图](../roadmap.md)和[计划索引](../plans/README.md)。
性能测量继续存放在 [benchmarks/results](../../benchmarks/results/README.md)。

## 版本验收

- [VFS 整体操作与消费迁移](2026-10-22-vfs-operations-migration.md)：整文件写入、元数据、新来源与兼容头隔离，本机验收通过，远端待重新验收。

- [VFS 统一只读入口](2026-10-22-vfs-read-context.md)：vfs/backend/io_context/file/request 主线与共享读取编排，本机原生/Web/安装通过，整体迁移仍实施中。

- [三类日常资源请求接口迁移](2026-10-22-three-request-api.md)：合并 reload/refresh、特殊调度转 group、修复来源快照基准；新代码远端验收待完成。

- [统一资源请求入口收口](2026-10-22-single-load-entry.md)：移除 manager/database 同步加载与双注册槽，本机原生、安装和 Web 通过，远端验收待完成。

- [默认 JSON 工厂异步迁移](2026-10-22-json-factory-migration.md)：统一 JSON loader 名称与默认工厂，原生、安装和 Web 通过，旧同步入口退出及远端验收待完成。

- [异步字节读取协议](2026-10-22-byte-reader-protocol.md)：reader/VFS/JSON 显式异步接入，本机原生、安装与 Web 通过，默认工厂迁移及远端验收待完成。

- [统一 loader 协议](2026-10-22-unified-loader-protocol.md)：统一异步入口与显式同步适配，本机原生/Web/浏览器/安装通过，远端验收待执行。

- [资源核心与来源适配边界](2026-10-22-resource-source-boundary.md)：第二批 provider 边界迁移，本机原生/Web/安装通过，远端验收待执行。

- [加载快照解耦原型验证](2026-10-22-load-snapshot-prototype.md)：首批快照/工程装配迁移，原生矩阵、Web、安装与压力验证通过。

- [IO 与资源 API 整理基线](2026-10-22-io-assets-api-baseline.md)：迁移清单与现有宿主/VFS 路径本机基线，新架构尚未实现。
- [v0.7.0 发布验收](2026-10-21-v0.7.0-acceptance.md)：独立平台 alpha 源码最终提交验收与公开下载复核通过。
- [Emscripten 平台实施与验收](2026-10-20-emscripten-integration.md)：v0.7.0 开发与真实验证状态。
- [Emscripten 兼容性评估](2026-10-20-emscripten-assessment.md)：当前 main 复现、Granit 参考和限制。
- [v0.6.0 发布验收](2026-10-20-v0.6.0-acceptance.md)：最终提交验收与公开制品复核通过。
- [第三方依赖缓存迁移](2026-10-20-dependency-source-cache.md)：v0.6.0 缓存迁移与跨平台集成验收记录。
- [v0.5.0 VFS 后续开发](2026-10-20-v0.5.0-vfs-followups.md)：已发布并完成公开下载复核；最终跨平台验收通过，固定 Linux 性能复验延期。

- [v0.4.0 统一 VFS 开发](2026-10-19-v0.4.0-async-vfs.md)：已发布并复核，记录路由、异步读取、最终 CI 与源码下载证据。

- [v0.3.0 异步 loader](2026-10-19-v0.3.0-async-loader.md)：已发布 alpha 源码，最终验收与公开下载复核通过。

- [v0.2.0 宿主集成](2026-10-19-v0.2.0-host-integration.md)：正在执行，尚未完成版本验收。

## 当前整理记录

- [2026-10-19 文档物理目录与命名整理](2026-10-19-documentation-layout.md)：完成目录归类，取代前次保留平铺结构的决定。

- [2026-10-19 文档职责与唯一维护入口审查](2026-10-19-documentation-audit.md)。

## 历史审计与验证

- [2026-08-04 内存域调用点审计](2026-08-04-memory-domain-audit.md)：重构前归属与风险基线。
- [2026-07-30 至 2026-08-05 Linux 验收历史](history/2026-08-05-linux-validation-history.md)。
- [路线图及专题规划整理前基线](history/2026-10-19-roadmap-baseline.md)：保留原完成清单和未来措辞。
- [未记录执行日期的 API 边界扫描摘要](history/api-boundary-scan-undated.md)：缺少提交与环境，不作当前验收证据。

## 验收证据的既有入口

以下计划仍保留迁移验收摘要和 CI 链接，后续新增详细执行记录写入本目录，避免复制旧证据：

- [Tasks/Asio 收尾验收](../plans/asio-task-redesign.md#2026-10-19-收尾验收)。
- [Resources/Assets 协程收尾验收](../plans/resource-coroutine-pipeline.md#2026-10-19-收尾验收)。

`history/` 只保存旧状态，不定义当前行为或当前待办。日期未知的旧记录须如实标注，不补造验证时间。
