<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Object 类型身份命名收敛计划

## 背景

对象类型已经复用 meta registry 的 `type_id` 和 `type_guid`，不存在独立的对象类型身份域。
迁移前的 `object_type_id` 只是 `type_id` 的别名，`object_type_guid` 也只是对象派生类使用的
静态成员命名约定。保留两套名称会让 API 看起来像存在两套标识。

## 目标

- 对象相关公共 API 直接使用 `type_id`；
- 删除 `object_type_id` 别名；
- 对象派生类使用 `static constexpr type_guid static_type_guid` 声明稳定类型 GUID；
- `traits::type_identity<T>` 继续作为名称和 GUID 的统一读取入口；
- `object_id` 和对象实例 GUID 不在本次类型身份迁移范围内。

## 迁移步骤

1. 修改 object/meta 公共头文件和实现；
2. 修改 object manager、registry、handle 与 serialization 接口；
3. 修改测试类型声明和断言；
4. 更新 object、meta、serialization 文档；
5. 使用 Windows Clang Debug 单线程构建并运行相关测试。

## 兼容性

本次直接删除重复别名和旧静态成员约定，不保留 deprecated 过渡入口。项目仍处于允许整理公共
API 的阶段，一次性迁移可以避免新旧命名长期并存。

## 实施结果

- `object_type_id` 已删除，object、manager、registry、handle 和 serialization 统一使用
  `type_id`；
- 对象稳定类型 GUID 声明已统一为 `static_type_guid`，并由
  `traits::type_identity<T>::guid()` 读取；
- Windows Clang Debug 使用单线程完成相关目标和头文件自包含构建；
- object、serialization object、serialization polymorphic 与头文件自包含共 10 项测试通过。
