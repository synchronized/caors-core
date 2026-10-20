<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 操作与开发指南

本目录回答具体任务怎样完成；API 契约见[参考目录](../reference/README.md)，
模块模型见[概念目录](../concepts/README.md)，全局入口见[文档中心](../README.md)。

- [构建、测试、安装与 CMake 接入](build.md)：环境、preset、选项、部署与 consumer 验证。
- [Emscripten 单线程接入](emscripten.md)：开发分支构建、调度和浏览器测试边界。
- [第三方依赖与源码缓存](third-party-dependencies.md)：锁定来源、离线、并发准备、目标复用和恢复。
- [开发规范](development.md)：源码边界、任务分支/PR、版本准备边界与新增模块规则。
- [CI 与验证策略](ci.md)：当前工具、触发、矩阵与按改动选择检查。
- [版本准备与发布](release.md)：main/release 分支、固定 SHA、人工发布及公开制品复核。
- [类型注册与初始化](type-registration.md)：跨模块注册链、顺序、显式 context 和失败处理。
- [动态插件接入与生命周期](plugin-integration.md)：CRT、加载诊断、注册与安全卸载操作。
- [Benchmark 构建与测量](benchmarking.md)：目标、用例、采样和分配统计范围。

指南围绕任务组织，只引用必要接口；详细 API 声明与限制由对应 Reference 维护。

- [接入宿主异步 VFS](async-vfs-integration.md)：拥有式挂载、原生 loop 后端与 loader 的接入步骤。
