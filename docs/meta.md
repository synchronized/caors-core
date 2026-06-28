<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta

本文档说明 CAORS 类型元数据层的当前设计。`meta` 不是运行期游戏 ECS，而是围绕 `string_name` 和 `type_id` 组织类型、对象、反射和序列化元信息的共享底座。`string_name` 定义在 `caors/core/string_name.h`，完整说明见 [core.md](core.md)。

## 设计目标

`meta` 第一阶段解决几件事：

- 用 `string_name` 保存运行期去重后的类型名。
- 用 `type_id` 表示由 `string_name` 派生的运行期类型 key。
- 用 `type_guid` 保存可选的稳定类型标识。
- 用 `type_info` 保存类型名、稳定 GUID、大小、对齐和父类型。
- 用全局 `meta_registry` 集中处理基础类型注册、查找和继承关系判断。
- 用全局 `meta_registry` 在同一个 `type_id` 上挂载不同主题的元数据组件。
- 避免对象系统、反射系统和序列化系统各自重复实现类型注册逻辑。

当前不会包含：

- 运行期实体 ECS。
- system 调度。
- 对象生命周期管理器。
- 具体字段、方法或序列化格式。

这些能力应由对象、反射和序列化模块基于 `meta` 扩展。

新代码可以直接包含 `caors/meta.h` 使用完整类型元数据 API；如果只需要局部能力，也可以包含对应子头文件：

- `caors/meta/type_id.h`：`type_id`、`type_guid` 和 `make_type_id()`。
- `caors/meta/type_name.h`：`type_name<T>` 默认类型名映射。
- `caors/meta/type_key.h`：内部 C++ 类型 key。
- `caors/meta/type_info.h`：`type_info` 和 `make_type_info<T>()`。
- `caors/meta/view.h`：`meta_view<T...>` 查询视图。
- `caors/meta/registry.h`：全局 `meta_registry`、注册结果和注册 helper。

## 核心概念

### string_name

`string_name` 是运行期字符串名。相同文本会共享同一个运行期 id，适合名称比较和查找：

```cpp
caors::string_name name{"game.player"};
std::string_view view = name.view();
```

`string_name::id()` 只保证当前进程内唯一，不承诺跨进程或跨版本稳定。

### type_id

`type_id` 是运行期类型标识。`0` 保留为无效值。它由类型名对应的 `string_name::id()` 派生：

```cpp
caors::type_id id = caors::make_type_id("game.player");
```

`type_id` 适合当前进程内的运行期查找，不适合作为序列化长期稳定 ID。需要长期稳定身份时使用 `type_guid`。

### type_guid

`type_guid` 是稳定类型标识，基于 `guid` 包装。它用于序列化、存档、资源引用和插件边界等需要长期稳定识别类型的场景。

```cpp
auto guid = caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
caors::type_info info =
    caors::make_type_info<player>("game.player", caors::type_guid{*guid});
```

需要注意：

- 未显式传入时，`type_info::guid` 为 nil。
- `type_guid` 不应由 `type_id` 或 `string_name::id()` 派生。
- 修改类型名不应自动修改 `type_guid`，否则旧数据无法稳定识别类型。

### type_info

`type_info` 是基础类型描述，包含：

- `id`
- `name`
- `guid`
- `size`
- `alignment`
- `parent_id`

```cpp
caors::type_info info = caors::make_type_info<player>("game.player");
```

`make_type_info<T>()` 会根据类型名填充 `id`，注册表负责校验 `id` 与 `name` 是否一致。

### type_name<T>

`type_name<T>` 是 C++ 类型到 CAORS 类型名的默认映射。内置基础类型已经提供默认名称，例如：

```cpp
caors::type_name<int>::value;   // "int"
caors::type_name<float>::value; // "float"
```

用户类型可以通过特化 `type_name<T>` 提供默认名称，也可以直接通过 `register_type<T>()` 显式注册。模板注册函数会把 C++ 类型和注册后的 `type_info` 绑定起来，后续可以用 C++ 类型查询：

```cpp
caors::register_type<actor>("game.actor");

auto info = caors::find_type<actor>();
```

### meta_registry

`meta_registry` 是进程级全局类型元数据注册表。它直接管理基础类型信息，并允许在同一个 `type_id` 上挂载对象、反射、序列化等元数据组件。普通代码不需要创建 registry 对象：

```cpp
caors::type_register_result result =
    caors::register_type<actor>("game.actor");
```

注册表是 append-only：基础类型注册后不可清理或撤销，重复注册相同类型会返回重复结果。

注册失败会返回明确的 `type_register_result`：

- `success`：注册成功。
- `invalid_type`：类型描述无效。
- `duplicate_id`：同一个运行期 id、C++ 类型或等价类型描述已注册。
- `duplicate_name`：同一个名称已注册。
- `duplicate_guid`：同一个非 nil 稳定 GUID 已注册。
- `missing_parent`：父类型未注册。

使用方式是先注册基础类型，再给该类型挂载组件：

```cpp
struct object_factory_meta {
  bool constructible = false;
};

caors::register_type<actor>("game.actor");

caors::type_id actor_id = caors::make_type_id("game.actor");
caors::get_meta_registry().emplace<object_factory_meta>(actor_id, true);
```

组件 API：

- `emplace<T>(type_id, args...)`：给已注册类型挂载组件。
- `has<T>(type_id)`：判断类型是否拥有组件。
- `find<T>(type_id)`：获取组件副本。
- `view<T...>()`：查询同时拥有指定组件集合的类型。

`meta_component_result` 用于描述组件挂载失败原因：

- `success`：挂载成功。
- `invalid_type`：传入无效 `type_id`。
- `missing_type`：目标类型尚未注册。
- `duplicate_component`：目标类型已经挂载同类组件。

## 与对象和反射的关系

全局 `meta_registry` 是共享元数据层：

- `object_registry` 复用全局基础类型信息，再给同一个 `type_id` 挂载 `object_type` 组件。
- 全局反射注册表复用基础类型信息，再挂载 `reflection_type` 组件。
- 后续 `serialization_registry` 可以挂载序列化相关组件。

这样 `object_register`、`reflection_register` 和 `serialization_register` 不需要重复处理重名、重复 GUID、不一致类型描述和父类型缺失。

## 当前已实现能力

- `type_id`：运行期类型标识。
- `type_guid`：稳定类型标识。
- `string_name`：运行期 interned 名称，定义在 `caors/core/string_name.h`。
- `make_type_id()`：根据类型名生成运行期类型标识。
- `type_name<T>`：基础 C++ 类型到 CAORS 类型名的默认映射。
- `type_info` / `make_type_info<T>()`：基础类型描述。
- `meta_registry`：全局基础类型注册、按 id/name/guid/C++ 类型查询、继承关系判断和组件式元数据挂载。
- `meta_view<T...>`：按组件集合查询类型。
- `get_meta_registry()`：进程内全局元数据注册表。

相关测试位于 `tests/meta/` 和 `tests/core/`，测试组织规则见 [tests/README.md](../tests/README.md)。
