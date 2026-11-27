<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 资源、资产与 IO 移除记录

## 范围

依据用户 2026-11-27 明确删除要求，停止宿主资源核心及动态依赖开发。
分支 feature/remove-asset-resource-io；不调整版本、不创建标签、不合并 main。
迁移契约由[退出指南](../guides/application-modules-removal.md)维护。

删除三个公共头目录、实现目录及聚合头，清理资源引用 codec、来源装配、资源及文件专用测试、
示例和基准。序列化保留内存转换，移除文件/流 API；插件保留动态加载与通用对象能力，
删除资源/文件测试扩展及清单文件读取便利入口。

## 验证

Windows Clang Debug 动态库：剩余库实现及示例已编译，全部测试目标仍在构建。
原安装和重定位后安装消费各 5 项通过；新安装前缀确认没有 assets/resources/io 目录及聚合头。
活动源码及构建引用扫描仅保留旧头不存在的负向检查。668 个本地文档链接存在性检查通过。
格式及 git diff --check 通过。原生全量 CTest 和本机 Web 静态库编译仍待结束。
跨平台 CI 尚未验证，不引用旧提交的绿色结果覆盖本次删除。
