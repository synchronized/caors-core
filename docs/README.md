<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# CAORS 文档中心

文档按实际职责存放，规范见[项目文档规范](../DOCUMENTATION_GUIDE.md)。
当前 API、实施计划和历史证据分别维护，计划中的能力不代表已经可用。

## 推荐阅读顺序

- 初次接入：[构建安装](guides/build.md) → [运行示例](../examples/README.md) →
  [模块参考](reference/README.md) → [稳定性政策](reference/api-stability.md)。
- 参与开发：[开发规范](guides/development.md) → [文档规范](../DOCUMENTATION_GUIDE.md) →
  [路线图](roadmap.md) → [相关计划](plans/README.md) → [测试组织](../tests/README.md)。

## 当前版本规划

下一目标为 [v0.3.0：异步 loader 与宿主 I/O 接入](versions/v0.3.0.md)，处于规划阶段；
具体设计见[异步资源 loader 计划](plans/asynchronous-resource-loader.md)，规划不代表当前 API 已实现。
[v0.2.0](versions/v0.2.0.md) 的实现验收与发布状态继续由其版本页维护。

## 分类入口

| 目录 | 主要问题 | 入口 |
| --- | --- | --- |
| guides | 如何构建、接入、注册、测量和参与开发 | [操作与开发指南](guides/README.md) |
| reference | 当前 API、格式、配置与行为契约 | [模块与政策参考](reference/README.md) |
| concepts | runtime、模块协作和 ABI 原理 | [架构与原理](concepts/README.md) |
| checklists | 如何重复执行检查，哪些条件必须满足 | [验收清单](checklists/README.md) |
| plans | 单项工作怎样设计、实施和验收 | [计划索引](plans/README.md) |
| records | 实际执行、审计与历史证据 | [记录索引](records/README.md) |
| versions | 单次版本的固定范围、依赖与出口 | [版本规划](versions/README.md) |
| templates | 创建版本与验收文档需要哪些字段 | [文档模板](templates/README.md) |

全局方向见[路线图](roadmap.md)；可运行程序见[示例目录](../examples/README.md)，
行为测试见[测试目录](../tests/README.md)，测量数据见[性能结果](../benchmarks/results/README.md)。
分类目录 README 管理具体条目，本页不再重复完整清单。

用户可见变化见[CHANGELOG](../CHANGELOG.md)，维护者发布步骤见[发布流程](guides/release.md)。
