<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Core

本文档说明 CAORS core 主题。该主题放置不依赖对象、反射、序列化等上层模块的通用基础类型和基础容器。

## 当前范围

当前已实现：

- `caors/core/string_name.h`：运行期字符串名。
- `caors/core/guid.h`：128-bit 稳定标识值类型。
- `caors/core/containers.h`：基于 `caors::allocator<T>` 的 `caors::string` 和 `caors::vector<T>`。

后续可以按需要扩展：

- 基础 hash 工具。
- 与 CAORS 内存系统适配的更多基础容器，例如 list、hash_map、small_vector、stable_vector。

## string_name

`string_name` 会把文本加入进程内字符串表，相同文本共享同一个运行期 id：

```cpp
caors::string_name first{"game.actor"};
caors::string_name second{"game.actor"};

first == second; // true
```

它适合用于类型名、字段名、方法名、属性名等运行期名称比较和查找。

需要注意：

- `string_name::id()` 只保证当前进程内唯一。
- `string_name` 不承诺跨进程、跨版本或跨动态库边界稳定。
- 序列化和资源引用应使用显式 `type_guid` 或业务自己的稳定 GUID。

## guid

`guid` 是通用 128-bit 值类型，用于需要跨进程、跨版本稳定识别的场景：

```cpp
auto parsed = caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
if (parsed) {
  std::string text = parsed->string();
}

constexpr caors::guid stable = caors::guid::from_bytes(caors::guid::byte_array{
    0x2f, 0x6d, 0x1f, 0x64, 0x7a, 0x3a, 0x4d, 0xb2,
    0x9a, 0x2d, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc,
});
```

需要注意：

- 默认构造得到 nil GUID：`00000000-0000-0000-0000-000000000000`。
- `guid::from_bytes()` 可用于声明编译期稳定 GUID。
- `guid::parse()` 只接受标准 8-4-4-4-12 格式，失败时返回 `std::nullopt`。
- `guid::string()` 输出小写标准格式。

相关测试位于 `tests/core/`，测试组织规则见 [tests/README.md](../tests/README.md)。

## 基础容器

`caors/core/containers.h` 当前提供轻量别名，而不是完整自研容器：

```cpp
caors::vector<int> values;
caors::string text;
```

这些别名使用 `caors::allocator<T>`，因此会通过当前默认 `memory_resource` 分配内存。需要显式资源时
可以直接传入 allocator：

```cpp
caors::tracking_resource tracking(&caors::get_system_memory_resource());
caors::vector<int> values{caors::allocator<int>{&tracking}};
```

当前只覆盖序列化 DOM 等近期需求。后续如果需要稳定地址、短数组优化或哈希查询，再分别评估
`stable_vector`、`small_vector` 和 `hash_map` 等容器。
