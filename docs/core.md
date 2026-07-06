<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Core

本文档说明 CAORS core 主题。该主题放置不依赖对象、反射、序列化等上层模块的通用基础类型和基础容器。

## 当前范围

当前已实现：

- `caors/core/string_name.h`：运行期字符串名。
- `caors/core/guid.h`：128-bit 稳定标识值类型。
- `caors/core/type_guid.h`：基于 `guid` 的稳定类型标识。
- `caors/core/containers.h`：基于 CAORS allocator 的基础容器别名。
- `caors/core/locator.h`：非拥有的进程级服务定位器。

后续可以按需要扩展：

- 基础 hash 工具。
- 与 CAORS 内存系统适配的更多基础容器，例如 list、small_vector、stable_vector。

## string_name

`string_name` 会把文本加入进程内字符串表，相同文本共享同一个运行期 entry：

```cpp
caors::string_name first{"game.actor"};
caors::string_name second{"game.actor"};

first == second; // true
```

它适合用于类型名、字段名、方法名、属性名等运行期名称比较和查找。

需要取回字符串时：

```cpp
caors::string text = first.string(resource); // 使用 CAORS allocator
std::string std_text = first.to_string();    // 使用标准库字符串
```

需要注意：

- `string_name` 不承诺跨进程、跨版本或跨动态库边界稳定。
- `string_name` 不暴露运行期 id；比较和哈希只适合当前进程内使用。
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

## type_guid

`type_guid` 是专门用于类型身份的稳定 GUID 包装。它定义在 `caors/core/type_guid.h`，根命名空间下保留
`caors::type_guid` 作为常用值类型别名：

```cpp
auto parsed = caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
caors::type_guid id{*parsed};
```

需要注意：

- `type_guid` 不等于 `meta::type_id`；前者用于跨进程、跨版本稳定识别，后者只用于当前进程内注册表索引。
- `type_guid` 不应由 `string_name` 或运行期 hash 自动派生。
- 没有长期序列化或资源引用需求的临时类型可以保持 nil。

相关测试位于 `tests/core/`，测试组织规则见 [tests/README.md](../tests/README.md)。

## 基础容器

`caors/core/containers.h` 当前提供轻量别名，而不是完整自研容器：

```cpp
caors::vector<int> values;
caors::list<int> queue;
caors::string text;
caors::hash_map<int, caors::string> names;
```

这些别名使用 `caors::allocator<T>`，不会修改进程级 `std::pmr` 默认资源，也不会影响宿主程序或其他库。
直接默认构造 `caors::vector<T>` / `caors::list<T>` / `caors::string` / `caors::hash_map<K, V>` 时会捕获当前
CAORS 默认 `memory_resource`。

需要显式资源时，可以直接传入 `caors::allocator<T>`，或使用工厂函数传入 `memory_resource`：

```cpp
caors::tracking_resource tracking(&caors::get_system_memory_resource());
caors::vector<int> values{caors::allocator<int>{&tracking}};

auto default_values = caors::make_vector<int>();
auto queue = caors::make_list<int>(&tracking);
auto text = caors::make_string(&tracking);
auto map = caors::make_hash_map<int, caors::string>(&tracking);
```

当前先覆盖序列化 DOM、注册表和元数据等近期需求。后续如果需要稳定地址或短数组优化，再分别评估
`stable_vector`、`small_vector` 等容器。

## 服务定位器

`locator<T>` 是一个轻量的进程内服务定位器，用于保存默认资源管理器、文件系统、配置服务等全局可选服务：

```cpp
struct file_system {};

file_system fs;
caors::locator<file_system>::set(&fs);

auto* current = caors::locator<file_system>::get();
```

它只保存非拥有指针，不负责创建或销毁服务对象。调用方必须保证服务对象在定位器保存期间保持存活。

测试或临时替换服务时可以使用 `with()` 自动恢复旧值：

```cpp
caors::locator<file_system>::with(&fs, [] {
  auto* current = caors::locator<file_system>::get();
});
```

需要注意：

- `get()` 未设置时返回 `nullptr`，不会自动构造服务。
- `set()` 和 `reset()` 会返回之前保存的指针，便于调用方恢复。
- `locator<T>` 不加锁；推荐只在初始化阶段、关闭阶段或测试隔离代码中修改。
- 多线程运行期切换服务时需要上层自行同步。
