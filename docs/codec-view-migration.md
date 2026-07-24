<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Serialization codec 绑定视图迁移计划

## 背景

当前 `codec` 与 `codec_capability` 都表达类型级 codec 查询，职责重复；同时
`codec_capability::write/read/make_value` 每次调用都要求传入 `any/any_ref`，与 builtin
capability 的统一 `bind(source) -> view` 模型不一致。

## 目标结构

```text
codec_capability
├── type()/can_write()/can_read()/can_make_value()
└── bind(any_ref) -> codec_view

codec_view
├── valid()/writable()/get_type()
├── write(context, writer)
├── read(context, reader)
└── make_value(context)
```

## API 收敛

- 删除重复的 `serialization::codec` 类型；
- `codec_of<T>()` 和 `find_codec(meta::type)` 返回 `codec_capability`；
- 新增继承 `capability_view_base<codec_ops>` 的 `codec_view`；
- 实例级 write/read/make_value 迁移到 `codec_view`；
- `any` 调用入口通过其 `any_ref` 绑定 view，不保留两套实例接口。

## 保留边界

- `codec_ops` 继续是类型擦除协议；
- `codec_traits<T>` 继续是编译期扩展点；
- serialization detail 的已缓存 ops 快速路径保持不变；
- 本次不改变序列化格式和 codec 优先级。

## 验证

- 静态、自定义和内置 codec 查询；
- 可写、只读、错误类型与 any_ref 快捷绑定；
- writer、reader、DOM value 三条路径；
- Windows Clang Debug 单线程构建及相关测试。
