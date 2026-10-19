<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 操作与开发指南

本目录回答具体任务怎样完成；API 契约见[参考目录](../reference/README.md)，
模块模型见[概念目录](../concepts/README.md)，全局入口见[文档中心](../README.md)。

- [构建、测试、安装与 CMake 接入](build.md)：环境、preset、选项、部署与 consumer 验证。
- [开发规范](development.md)：源码边界、分支工作流、格式和新增模块规则。
- [类型注册与初始化](type-registration.md)：跨模块注册链、顺序、显式 context 和失败处理。
- [动态插件接入与生命周期](plugin-integration.md)：CRT、加载诊断、注册与安全卸载操作。
- [Benchmark 构建与测量](benchmarking.md)：目标、用例、采样和分配统计范围。

指南围绕任务组织，只引用必要接口；详细 API 声明与限制由对应 Reference 维护。
