<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 宿主 runtime 与 ABI 集成契约计划

## 状态与范围

状态：待验收，新增实现正在跨平台验收。对应 C51-01；交付范围和依赖以
[v0.2.0](../versions/v0.2.0.md) 为准。现有身份机制与插件检查予以复用，不创建平行 runtime。

已有依据：[模块边界](../concepts/module-boundary.md)、[插件指南](../guides/plugin-integration.md)、
`tests/meta/plugin_identity/`。现有身份测试不等于字符串、跨模块创建/释放及全部部署拓扑已经验收。

## 设计与实施

- [x] 澄清三类拓扑：共享唯一动态 runtime 的模块可在兼容 ABI 下交换对象；单模块私有静态 runtime
  的对象和内部身份不外传；独立 runtime 只交换显式值协议，不直接互传 C++ 对象。
- [x] 修正现有“进程内只允许一个 runtime”的绝对表述，区分共享对象域和完全隔离的私有使用。
  收口到模块边界文档，其他指南只链接，不产生第二份 ABI 政策。
- [x] 明确同版本、编译器/工具集、标准库、CRT、迭代器调试、架构、异常/RTTI、公共类型定义等前提。
  插件加载检查不覆盖普通链接路径；普通 DLL 的一致性由宿主构建与部署负责。
- [x] 扩展 EXE 加两个 DLL 的共享 runtime 用例，覆盖 type_id、string_name、反射查询和对象创建/释放；
  分配端提供释放入口，并检查销毁计数，避免测试本身依赖不明确的跨 CRT delete。
- [x] 分清稳定 GUID、序列化字节与进程内身份；补私有静态嵌入示例及禁止对象外传的边界说明。

## 验收

Windows MSVC/Clang 与 Linux GCC/Clang 的受支持组合验证共享 runtime 身份、字符串哈希/相等、
反射结果及跨模块销毁恰好一次。插件不兼容配置必须给出字段级诊断，普通 DLL 路径不伪称自动检测。
独立 runtime 用例仅验证显式值协议，不通过执行未支持的对象互传来证明安全。
记录实际工具链、链接拓扑、CRT 和源码 SHA，证据归入 [Record](../records/README.md)。

## 已确定取舍

采用共享动态 runtime、单模块私有静态 runtime、独立值协议三类边界；native C++ ABI 前提与普通链接责任由[模块边界](../concepts/module-boundary.md)维护。跨模块对象采用创建方释放协议，不支持任意工具链或多版本对象混用。

## 验收收口

实现与设计任务已完成，完整版本矩阵仍在执行；状态“待验收”不表示全部平台已经通过。
实际提交、失败修复、已执行结果和剩余门槛统一见
[版本验收记录](../records/2026-10-19-v0.2.0-host-integration.md)。
Windows 八组完整候选、最终 Linux 安装矩阵及版本元数据收口前，本计划不标为已完成。
