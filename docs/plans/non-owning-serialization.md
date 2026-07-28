<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 非拥有值序列化计划

## 文档状态

- 当前状态：已完成；
- 实施范围：借用字符串存储、调用方缓冲区写入和外部引用解析；
- 暂不实施：`std::shared_ptr` / `std::weak_ptr` 对象图序列化。

## 目标

非拥有值不采用统一的读写语义，而是根据其依赖的外部生命周期明确声明解码条件：

| 解码条件 | context 来源 | 首批适用类型 |
|---|---|---|
| `borrowed_storage` | `context.state.storage` | `std::string_view` |
| `caller_buffer` | 目标 view 与 `context.config.borrowed_views` | `std::span<T>` |
| `reference_resolver` | `context.resolvers.external_refs` | `objects::weak_ref<T>` |

## 借用字符串存储

- `std::string_view` 写出时按字符串内容处理；
- 普通读入默认拒绝；
- 调用方提供 `string_storage` 后允许读入；
- `string_storage` 的生命周期必须覆盖所有指向其中内容的 view；
- 多个字段分别保存，不能互相覆盖。

## 调用方缓冲区

- `std::span<T>` 写出时按数组快照处理；
- 普通读入默认拒绝；
- 只有显式启用 `context.config.borrowed_views.allow_caller_buffer_write` 才允许写入；
- 首批只支持输入长度与目标 span 长度完全一致；
- 长度不匹配时必须在写入任何元素前返回 `invalid_argument`；
- 所有元素先解码到独立暂存值，读取任一元素失败时调用方缓冲区保持不变；
- `std::span<const T>` 始终禁止读入。

## 外部引用解析

- `objects::weak_ref<T>` 只保存对象身份，不展开对象本体；
- 写出时短暂 `lock()`，保证 resolver 读取对象身份期间目标仍然存活；
- 读入时通过 `external_ref_resolver_ops` 解析对象；
- 解析结果必须是仍绑定 `objects::ref<T>` 控制块的兼容对象，否则返回错误；
- resolver 未命中、动态类型不匹配或对象没有引用控制块时，目标 weak ref 保持原值；
- weak ref 不拥有未解析身份，因此首批不支持 `keep_unresolved`；
- 空或已失效 weak ref 按空值写出。

## 验证

- `string_view` 在缺少和提供借用存储时的行为；
- `span` 默认拒绝、显式允许、长度不匹配和 const 元素；
- `weak_ref` 的空值、运行期整数身份、解析失败和类型不匹配；
- 所有相关测试使用 Windows Clang Shared Release、`-j 1` 构建。
