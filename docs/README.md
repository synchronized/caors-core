<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# CAORS 文档

本目录存放 CAORS 的专题文档。这里仅作为文档索引，项目介绍和快速开始见 [README.md](../README.md)。

## 项目文档

- [build.md](build.md)：环境要求、CMake preset、构建、测试、安装和 CMake 选项。
- [development.md](development.md)：目录结构、代码规范、格式化、静态检查和新增模块规则。
- [roadmap.md](roadmap.md)：项目路线图。
- [registration.md](registration.md)：类型、对象、反射和序列化的推荐注册顺序与初始化策略。
- [traits.md](traits.md)：底层 traits 扩展点、adapters 边界和用户特化规则。

## 主题文档

- [core.md](core.md)：core 主题，包含 `string_name`、`guid`、`type_guid` 和基础容器规划。
- [error.md](error.md)：统一错误码、错误来源域和 `std::error_code` 适配。
- [memory.md](memory.md)：内存资源、分配器、策略资源和调试跟踪。
- [meta.md](meta.md)：类型元数据、`type_guid`、`meta::type`、`meta::registry` 和元数据组件。
- [object.md](object.md)：对象基类、对象注册表和对象创建销毁入口。
- [reflection.md](reflection.md)：反射类型、字段元信息和手写注册入口。
- [resources.md](resources.md)：资源身份、资源基类、轻量资源引用和非拥有资源索引。
- [serialization.md](serialization.md)：序列化元信息、注册表和基于反射的文本序列化 MVP。
- [value.md](value.md)：`any`、`any_ref`、运行期 view、细粒度头文件边界和 `variant` 数据 DOM。

## 相关入口

- [examples/README.md](../examples/README.md)：示例构建、运行和新增规则。
- [tests/README.md](../tests/README.md)：测试组织、运行和新增规则。
