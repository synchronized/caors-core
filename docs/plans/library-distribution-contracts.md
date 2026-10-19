<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# PIC、符号可见性与库分发计划

## 状态与范围

状态：讨论中，对应 C51-04，依赖 [runtime 契约](runtime-integration-contracts.md)和
[可嵌入构建](embedded-cmake-configuration.md)。版本范围见 [v0.2.0](../versions/v0.2.0.md)。
现有安装、独立 consumer 和可重定位验证继续复用；本计划不重新打开已完成的
[原安装包计划](installed-package-consumer-validation.md)。

## 设计与实施

- [ ] 提供静态 CAORS 的 PIC 配置，明确定义默认值及宿主 `CMAKE_POSITION_INDEPENDENT_CODE` 的优先级。
  增加 Linux 共享宿主私有链接静态 CAORS 的真实链接/运行用例。
- [ ] 评估隐藏默认符号与 inline 可见性，按公共导出声明修复遗漏；检查模板、RTTI、异常及跨 DLL 析构，
  避免只收紧符号却破坏公共接口。
- [ ] 定义动态库 VERSION/SOVERSION 策略，并与 alpha 阶段兼容政策一致。
  CMake 包当前 SameMajorVersion 匹配是否过宽需一并评审；包版本匹配不等同于 ABI 保证。
- [ ] 审计导出符号与真实 DLL/SO 依赖，包括 Asio 实例化和内部实现；允许清单与工具链噪声分类记录，
  不能仅凭 PRIVATE 声明认定符号或静态依赖已隔离。
- [ ] 检查静态导出的系统链接依赖，保持 `find_package(caors)` 脱离源码/构建目录可用。
- [ ] 扩展现有安装验证，加入共享宿主私有使用静态 CAORS；同步动态库部署、版本和限制说明。

## 验收

Windows 与 Linux 的静态/共享、Debug/Release 安装包均执行四类独立 consumer、前缀迁移及运行验证；
Linux 增加 PIC 静态库并入共享宿主，Windows 检查对应 CRT 配置和运行库部署。
在 Record 固定源码 SHA、工具链、导出列表及 DLL/SO 依赖检查命令和结果。
隐藏符号后身份、反射、插件和跨模块释放仍通过；不依赖开发机 PATH 偶然找到 runtime。
不同版本共存若不支持必须写入限制，不通过更改文件名暗示隔离成立。

## 未决与阻塞

alpha 的 SOVERSION 策略、公共符号范围与包兼容匹配规则需明确后才能验收。
预编译 SDK、签名和发布自动化不在本计划范围；源码交付也必须通过本计划安装验证。
当前没有本计划新增符号审计或共享宿主验收结果。
