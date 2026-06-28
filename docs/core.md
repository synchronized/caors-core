<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Core

本文档说明 CAORS core 主题。该主题放置不依赖对象、反射、序列化等上层模块的通用基础类型和基础容器。

## 当前范围

当前已实现：

- `caors/core/string_name.h`：运行期字符串名。

后续可以按需要扩展：

- `type_guid` / `uuid`：跨版本和序列化使用的稳定标识。
- 基础 hash 工具。
- 与 CAORS 内存系统适配的基础容器，例如 string、list、hash_map。

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
- 序列化和资源引用应使用后续显式 `type_guid` / `uuid`。

相关测试位于 `tests/core/`，测试组织规则见 [tests/README.md](../tests/README.md)。
