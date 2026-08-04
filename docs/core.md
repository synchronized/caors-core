<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Core

本文档说明 CAORS core 主题。该主题放置不依赖对象、反射、序列化等上层模块的通用基础类型和基础容器。

## 当前范围

当前已实现：

- `caors/core/string_name.h`：运行期字符串名。
- `caors/core/guid.h`：128-bit 稳定标识值类型。
- `caors/core/type_guid.h`：基于 `guid` 的稳定类型标识。
- `caors/core/containers.h`：基于 CAORS allocator 的基础容器和容器别名。
- `caors/core/locator.h`：非拥有的进程级服务定位器。

后续可以按需要扩展：

- 基础 hash 工具。
- 与 CAORS 内存系统适配的更多基础容器，例如 `stable_vector`。

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

`caors/core/containers.h` 提供使用 CAORS allocator 的轻量别名，以及带内联存储的
`small_vector`：

```cpp
caors::vector<int> values;
caors::list<int> queue;
caors::string text;
caors::hash_map<int, caors::string> names;
caors::small_vector<int, 8> recent_values;
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
auto recent_values = caors::make_small_vector<int, 8>(&tracking);
```

`small_vector<T, N>` 在不超过 `N` 个元素时使用对象内联存储，溢出后通过容器绑定的
`memory_resource` 分配连续缓冲区。它支持常用 vector-like 修改操作、区间构造和资源感知复制/移动；
扩容、插入、收缩及交换都可能使引用和迭代器失效。容器对象布局包含内联区，不能按值穿过稳定 C ABI，
也不得比其非拥有 `memory_resource*` 存活更久。

Meta 将它识别为顺序容器，类型身份包含元素类型和内联容量，例如
`caors.small_vector<int,8>`。JSON 和 MessagePack 只保存元素序列，不保存容量或当前存储模式。
是否在具体组件中替换普通 vector，仍需依据真实元素数量分布和基准结果决定。

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
