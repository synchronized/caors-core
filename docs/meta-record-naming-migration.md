<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta 运行时记录命名迁移计划

## 背景

`type_data`、`property_data` 和 `method_data` 不只是被动数据结构：它们保存 registry 元数据、
类型擦除状态，并通过成员函数完成查询和调用编排。`*_data` 无法清晰表达这一职责，而
`*_wrapper` 又容易与公共轻量 handle 和 C++ 值 wrapper 混淆。

## 目标结构

```text
公共 handle -> 运行时 record -> ops table -> 模板 model
```

本次统一迁移：

- `type_data` -> `type_record`
- `property_data` -> `property_record`
- `method_data` -> `method_record`
- `method_set_data` -> `method_set_record`
- `module_data` -> `module_record`
- `enum_value_data` -> `enum_value_record`

对应内部文件名同步改为 `*_record.h/.cpp`。

## 保留的 data 类型

以下类型继续使用 `*_data`，因为它们主要表达结构化数据或类型擦除载荷，不承担公共 handle
背后的完整运行时记录职责：

- `nested_type_data`
- `capability_data`
- `extension_data`

## 实施步骤

1. 迁移 `type/property` 记录、访问类、静态 model 和 registry 引用；
2. 迁移 `method/method_set/module/enum_value` 记录；
3. 更新源码文件名、include、测试和架构文档；
4. 检查旧记录名称仅在迁移文档中保留；
5. 使用 Windows Clang Debug 单线程完成构建和测试。

## 非目标

- 不改变公共 API 和注册语法；
- 不改变 record 的所有权、生命周期或内存布局设计；
- 不改变 ops table 与模板 model 的职责；
- 不为纯数据结构机械套用 `record` 后缀。

## 实施结果

- 六类运行时记录及对应内部文件已统一为 `*_record`；
- 静态类型节点辅助接口统一为 `static_type_record_*`；
- `nested_type_data`、`capability_data` 和 `extension_data` 按职责继续保留原名；
- 架构术语已统一为 `handle -> record -> ops -> model`；
- Windows Clang Debug 全仓库单线程构建完成，75 项测试全部通过。
