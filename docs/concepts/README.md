<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 架构与原理

本目录维护长期模型、职责和权衡；具体接口见[参考](../reference/README.md)，
接入步骤见[指南](../guides/README.md)，历史方案与执行证据分别见[计划](../plans/README.md)和[记录](../records/README.md)。

- [IO 职责与实现复用](io-boundaries.md)：同步文件能力、异步接入、虚拟路由及保留或删除接口的判断条件。

- [跨编译模块边界](module-boundary.md)：唯一 runtime、native C++ ABI、跨模块数据、视图、capability 和卸载约束。

重要的新架构取舍可按规范新增 ADR；未建立独立决策前，不为目录结构创建空占位文件。
