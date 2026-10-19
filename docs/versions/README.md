<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 版本规划

本目录维护某次版本的固定范围、关联任务依赖、交付形式与发布出口，不复制 Plan 的设计和实施步骤。
当前工程版本由根 [CMakeLists.txt](../../CMakeLists.txt)维护，用户可见变化见
[CHANGELOG](../../CHANGELOG.md)，状态和兼容承诺见[API 政策](../reference/api-stability.md)。

## 当前安排

尚未在本目录固定下一发布版本和范围；本次只建立工作流，不把工程版本写成已发布版本。
确定版本后从[版本模板](../templates/version-plan.md)建立 `vX.Y.Z.md`，并在本页登记。

## 状态与收口

- 规划中：确定范围、非目标、任务依赖、平台和发布阻塞项。
- 实施中：跟踪关联 Plan 的出口，不在版本页维护第二份任务明细。
- 待验收：范围已实现，发布清单仍有需要核实的项。
- 候选已验收，尚未发布：版本准备完成；最终 main SHA 与制品仍按发布流程固定、核实。
- 已发布：有实际 tag/Release、公开交付和复核证据，版本页保留最终范围与链接。
- 取消或被取代：注明原因和替代者，不把未发布范围记为完成交付。

验收条件见[发布清单](../checklists/release-readiness.md)，执行顺序见[发布流程](../guides/release.md)，
实际结果见[记录索引](../records/README.md)。
