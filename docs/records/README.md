<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 实施与验收记录

本目录维护实际执行、验收与历史审计证据；当前 API、方向与任务状态分别见
[文档中心](../README.md)、[路线图](../roadmap.md)和[计划索引](../plans/README.md)。
性能测量继续存放在 [benchmarks/results](../../benchmarks/results/README.md)。

## 版本验收

- [v0.4.0 统一 VFS 开发](2026-10-19-v0.4.0-async-vfs.md)：集成验收中，记录路由、异步读取和资源链路证据。

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
