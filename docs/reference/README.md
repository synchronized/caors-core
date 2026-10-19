<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# API 与行为参考

本目录说明当前实现的接口、格式、所有权、生命周期、线程和失败契约。
操作步骤见[指南](../guides/README.md)，设计边界见[概念](../concepts/README.md)，
未来方向见[路线图](../roadmap.md)。

## 基础设施

- [Core](core.md)：string_name、GUID、基础容器和非拥有 locator。
- [编译期配置](config.md)：异常检测、内部分配策略和默认 new 对齐。
- [统一错误](error.md)：error、错误域、result 和 std::error_code 适配。
- [内存系统](memory.md)：resource、allocator、策略、调试与线程约束。
- [数学类型](math.md)：值类型、变换、投影、几何及 Meta/序列化适配。
- [IO](io.md)：路径、URI、字节流、文件系统和路由。
- [Traits 与适配](traits.md)：扩展点、adapters 边界和特化规则。

## 类型、对象与数据

- [Meta](meta.md)：类型身份、registry、metadata 和 capability。
- [Meta 属性与方法](meta-properties-and-methods.md)：字段、属性、全局/成员调用和重载。
- [Meta 值与运行期视图](meta-values.md)：value、any、owned_value、any_ref、argument、view 和 variant DOM。
- [对象系统](objects.md)：对象身份、指针、handle、引用计数和 manager。
- [序列化](serialization.md)：schema、codec、JSON/MessagePack、配置与 resolver。
- [Resources](resources.md)：身份、缓存、依赖、manifest、provider、loader 和请求。
- [Assets](assets.md)：工程配置、asset manager、factory 与 JSON 资源编排。

## 执行与事件

- [Tasks](tasks.md)：任务、轻量 executor、Asio context/pool、request 与显式协程适配。
- [Events](events.md)：delegate、signal、sink、连接和同步 dispatcher。

## 兼容政策

- [API 稳定性](api-stability.md)：当前阶段、稳定候选与实验性范围；Beta Gate 另由执行清单维护。
