<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Enum 绑定视图迁移计划

## 文档状态

- 当前状态：已完成；
- 实施范围：enum capability 与实例 view 职责收敛及相关测试。

## 背景

sequence、set、associative、pair、tuple、variant 和 indirect capability 均通过
`capability.bind(any_ref) -> view` 绑定实例。enum capability 仍要求每次转换都传入 source，
属于旧式 facade 遗留，并非性能优化。

## 目标

- 新增绑定实际枚举实例的 `enum_view`；
- `enum_capability` 只保留类型级枚举信息和 `bind()`；
- `to_i64/to_u64/from_i64/from_u64` 移入 `enum_view`；
- `enum_value` 继续表示 registry 中注册的枚举项，不与实例 view 合并；
- 支持通过 `any_ref::get_capability<enum_capability>()` 直接获得绑定 view。

## 目标结构

```text
enum_capability
├── is_signed()
├── value_count()/value_at()/find_value()
└── bind(any_ref) -> enum_view

enum_view
├── valid()/writable()/get_type()
├── to_i64()/to_u64()
└── from_i64()/from_u64()
```

## 验证

- capability facade 与 any_ref 直接绑定测试；
- 错误类型、只读 source 和无效 view 测试；
- Windows Clang Debug 单线程构建及相关测试。

## 实施结果

- `enum_view` 已基于通用 `capability_view_base<enum_ops>` 实现；
- enum 实例转换和写入已从 `enum_capability` 迁移到绑定 view；
- capability bind、any_ref 快捷绑定、只读 source 与错误类型测试均已覆盖；
- Windows Clang Debug 的 capability 与头文件自包含测试通过。
