<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 可嵌入 CMake 配置计划

## 状态与范围

状态：已完成。对应 C51-03；版本归属见 [v0.2.0](../versions/v0.2.0.md)。
实施前已有 `caors_` 私有模块/函数命名与父工程配置探针；本轮补齐独立链接模式、依赖隔离和选项命名。
当前实现入口为根、`src/`、`vendor/` 的 CMakeLists；不修改第三方源码来适配宿主。

## 设计与实施

- [x] 设计 CAORS 专属链接类型配置，未指定时跟随 `BUILD_SHARED_LIBS`；显式指定时不改父工程全局值。
  具体选项名与兼容优先级在实现评审中固定，避免两个布尔开关产生冲突。
- [x] 导出宏和插件/测试条件依据 CAORS target 实际 TYPE，排查所有依赖全局 `BUILD_SHARED_LIBS` 的分支。
- [x] 子项目测试默认值与宿主一致性独立处理；Catch2 仅在实际启用测试时配置，ctrack 仅在需要它的
  benchmark 启用时配置，内置调度使用标准库线程，不引入 Asio。关闭功能后验证目标图，而非只检查没有被编译。
- [x] 分别处理宿主提供完整 Catch2 targets、仅基础 target、无 target 的情况。
  不重定义已有目标；缺少 main target 时采用兼容实现或明确诊断，不静默混用两套 Catch2。
- [x] 外部依赖复用必须能验证所需版本及接口；未知版本默认明确拒绝或要求显式配置，
  兼容区间通过实际编译探针确定，不凭名称接受任意 target。
- [x] 将 `USE_LOG`、`USE_DEBUG_OUTPUT`、`USE_LOG_SEVERITY` 等通用选项收敛为 `CAORS_` 名称。
  如保留旧选项，规定新值优先、旧值警告及冲突处理，并更新 presets、指南和迁移说明。
- [x] 扩展 `tests/cmake/` 的父工程回归，验证父工程变量、函数和已有依赖目标不被覆盖。

## 验收

- 宿主共享/CAORS 静态、宿主静态/CAORS 共享两种反向组合配置、构建、运行；检查实际库类型及导出宏。
- 测试、示例、benchmark 全关后，不创建 Catch2/ctrack 相关目标；单独打开功能只引入必要依赖。
- Catch2 完整/仅基础/缺失/不兼容/版本未知的输入分别有成功结果或可理解的配置期诊断。
- 父工程同名选项不改变 CAORS 专属配置，新旧选项冲突行为有确定测试；安装 consumer 无源码树依赖。
- 按构建风险完成 Windows 相关矩阵和完整 Linux CI，证据单独入 Record。

## 已确定取舍

采用 `CAORS_LIBRARY_TYPE=AUTO/STATIC/SHARED`，新 `CAORS_USE_*` 选项优先，旧值仅作默认并提示弃用。外部 Catch2 需完整双 target 且版本为已验证的 3.15.0；ctrack 为 1.1.0；未知或不兼容版本明确拒绝。当前使用规则见[构建指南](../guides/build.md#嵌入宿主与依赖复用)。

## 验收收口

实现与契约验收已完成：标准库后端 Windows 八组完整测试、Linux 八组 CI、重复回归及安装迁移通过。
实际源码提交、平台与适用限制统一见[版本验收记录](../records/2026-10-19-v0.2.0-host-integration.md)。
版本元数据与发布收口由[版本页](../versions/v0.2.0.md)维护；本计划完成不表示已公开发布或 Gneiss 已接入。
