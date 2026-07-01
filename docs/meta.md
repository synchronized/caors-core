<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta

本文档说明 CAORS 类型元数据层的当前设计。`meta` 不是运行期游戏 ECS，而是围绕 `string_name` 和 `type_id` 组织类型、对象、反射和序列化元信息的共享底座。`string_name` 定义在 `caors/core/string_name.h`，完整说明见 [core.md](core.md)。

## 设计目标

`meta` 第一阶段解决几件事：

- 用 `string_name` 保存运行期去重后的类型名。
- 用 `type_id` 表示全局注册表分配的运行期类型句柄。
- 用 `type_guid` 保存可选的稳定类型标识。
- 用 `meta_type_data` 保存类型名、稳定 GUID、大小、对齐和父类型。
- 用 `meta_type` 作为对外只读类型视图，避免直接暴露注册表内部数据。
- 用全局 `meta_registry` 集中处理基础类型注册、id 分配、查找和继承关系判断。
- 用全局 `meta_registry` 在同一个 `type_id` 上挂载不同主题的元数据组件。
- 避免对象系统、反射系统和序列化系统各自重复实现类型注册逻辑。

当前不会包含：

- 运行期实体 ECS。
- system 调度。
- 对象生命周期管理器。
- 具体字段、方法或序列化格式。

这些能力应由对象、反射和序列化模块基于 `meta` 扩展。

新代码可以直接包含 `caors/meta.h` 使用完整类型元数据 API；如果只需要局部能力，也可以包含对应子头文件：

- `caors/meta/type_id.h`：`type_id`、`type_guid` 和按名称查询 id 的 `make_type_id()`。
- `caors/meta/type_name.h`：`type_name<T>` 默认类型名映射。
- `caors/meta/type_key.h`：内部 C++ 类型 key。
- `caors/meta/type.h`：`meta_type_data`、`meta_type` 和 `make_meta_type_data<T>()`。
- `caors/meta/view.h`：`meta_view<T...>` 查询视图。
- `caors/meta/registry.h`：全局 `meta_registry`、注册错误状态和注册 helper。

## 核心概念

### string_name

`string_name` 是运行期字符串名。相同文本会共享同一个运行期 id，适合名称比较和查找：

```cpp
caors::string_name name{"game.player"};
std::string_view view = name.view();
```

`string_name::id()` 只保证当前进程内唯一，不承诺跨进程或跨版本稳定。

### type_id

`type_id` 是运行期类型标识。`0` 保留为无效值。它由全局 `meta_registry` 分配，类似 ECS
entity，只保证当前进程内唯一，不承诺跨进程或跨版本稳定。

```cpp
caors::type_id id = caors::type_id_of<player>();
```

类型注册后，也可以通过名称查询当前进程内的运行期 id：

```cpp
caors::register_type<player>("game.player");
caors::type_id id = caors::make_type_id("game.player");
```

`type_id` 适合当前进程内的运行期查找，不适合作为序列化长期稳定 ID。需要长期稳定身份时使用
`type_guid`。

### type_guid

`type_guid` 是稳定类型标识，基于 `guid` 包装。它用于序列化、存档、资源引用和插件边界等需要长期稳定识别类型的场景。

```cpp
auto guid = caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
caors::meta_type_data data =
    caors::make_meta_type_data<player>("game.player", caors::type_guid{*guid});
```

需要注意：

- 未显式传入时，`meta_type_data::guid()` 为 nil。
- `type_guid` 不应由 `type_id` 或 `string_name::id()` 派生。
- 修改类型名不应自动修改 `type_guid`，否则旧数据无法稳定识别类型。

### meta_type_data 和 meta_type

`meta_type_data` 是底层注册描述，包含：

- `id`
- `name`
- `guid`
- `size`
- `alignment`
- `parent_id`
- `cpp_type`
- `clone_value` / `destroy_value` / `assign_value`

```cpp
caors::meta_type_data data = caors::make_meta_type_data<player>("game.player");
```

`make_meta_type_data<T>()` 只创建待注册描述，`id` 保持无效。真正的 `type_id` 在写入全局
`meta_registry` 时分配。模板路径会同时记录 C++ 类型 key，以及可用时的复制、析构和赋值函数。
C++ 类型可以先通过 `type_id_of<T>()` 创建匿名记录，后续注册名称或 GUID 时会补充到同一个类型记录上。

`meta_type` 是对外查询返回的只读视图：

```cpp
std::optional<caors::meta_type> type = caors::find_type<player>();

if (type) {
  caors::type_id id = type->id();
  std::string_view name = type->name().view();
}
```

`meta_type` 不拥有数据，内部引用全局 append-only 注册表中的 `meta_type_data`。普通代码不应持有
或修改 `meta_type_data`，需要查询类型信息时使用 `meta_type`。

### type_name<T>

`type_name<T>` 是 C++ 类型到 CAORS 类型名的默认映射。内置基础类型已经提供默认名称，例如：

```cpp
caors::type_name<int>::value;   // "int"
caors::type_name<float>::value; // "float"
```

用户类型可以通过特化 `type_name<T>` 提供默认名称，也可以直接通过 `register_type<T>()` 显式注册。模板注册函数会把 C++ 类型和注册后的 `meta_type` 绑定起来，后续可以用 C++ 类型查询：

```cpp
caors::register_type<actor>("game.actor");

auto type = caors::find_type<actor>();
```

也可以使用 RTTR 风格入口：

```cpp
caors::meta<actor>()
    .type("game.actor")
    .guid(caors::type_guid{*guid});
```

### meta_registry

`meta_registry` 是进程级全局类型元数据注册表。它直接管理基础类型信息，并允许在同一个 `type_id` 上挂载对象、反射、序列化等元数据组件。普通代码不需要创建 registry 对象：

```cpp
caors::error result = caors::register_type<actor>("game.actor");
```

注册表是 append-only：基础类型注册后不可清理或撤销，重复注册相同类型会返回重复结果。
当前组件同样按 append-only 方式挂载：同一类型的同类组件注册后不可替换。反射和序列化这类组件
内部字段数据也按“注册后冻结”处理，外部视图只在该约束下承诺进程生命周期内稳定。

注册表维护几类索引：

- `type_id -> meta_type_data`：运行期主索引。
- `cpp_type_key -> type_id`：C++ 类型查询索引，允许匿名类型提前获得 id；C++ 类型操作函数保存在
  对应的 `meta_type_data` 内。
- `type_name -> type_id`：名称查询索引，可选。
- `type_guid -> type_id`：稳定身份查询索引，可选。

注册失败会返回 `caors::error`，`domain()` 为 `error_domain::meta`，`value()` 为明确的通用错误码：

- `errc::invalid_type`：类型描述无效。
- `errc::duplicate_id`：同一个运行期类型记录、C++ 类型或等价类型描述已注册。
- `errc::duplicate_name`：同一个名称已注册。
- `errc::duplicate_guid`：同一个非 nil 稳定 GUID 已注册。
- `errc::missing_parent`：父类型未注册。

使用方式是先注册基础类型，再给该类型挂载组件：

```cpp
struct object_factory_meta {
  bool constructible = false;
};

caors::register_type<actor>("game.actor");

caors::type_id actor_id = caors::find_type<actor>()->id();
caors::get_meta_registry().emplace<object_factory_meta>(actor_id, true);
```

组件 API：

- `emplace<T>(type_id, args...)`：给已注册类型挂载组件。
- `has<T>(type_id)`：判断类型是否拥有组件。
- `find<T>(type_id)`：获取组件副本。
- `find_ptr<T>(type_id)`：获取组件只读指针。组件由注册表以 `unique_ptr` 保存，地址在 append-only
  注册表生命周期内保持稳定。
- `view<T...>()`：查询同时拥有指定组件集合的类型。

组件挂载同样返回 `caors::error`，`domain()` 为 `error_domain::meta`：

- `errc::invalid_type`：传入无效 `type_id`。
- `errc::missing_type`：目标类型尚未注册。
- `errc::duplicate_component`：目标类型已经挂载同类组件。

## 与对象和反射的关系

全局 `meta_registry` 是共享元数据层：

- `object_registry` 复用全局基础类型信息，再给同一个 `type_id` 挂载 `object_type` 组件。
- 全局反射注册表复用基础类型信息，再挂载 `reflection_type_data` 组件。
- 后续 `serialization_registry` 可以挂载序列化相关组件。

这样 `object_register`、`reflection_register` 和 `serialization_register` 不需要重复处理重名、重复 GUID、不一致类型描述和父类型缺失。
跨模块查询可以通过 `meta_view<T...>()` 完成，例如查询同时拥有 `reflection_type_data` 和
`serialization_type_data` 的类型。该视图保存查询时匹配的 `type_id` 列表，不持有组件引用；需要
读取组件内容时再通过 `find<T>()` 获取组件副本，或者由对应模块 API 返回稳定只读视图。

## 当前已实现能力

- `type_id`：运行期类型标识。
- `type_guid`：稳定类型标识。
- `string_name`：运行期 interned 名称，定义在 `caors/core/string_name.h`。
- `type_id_of<T>()`：获取指定 C++ 类型的运行期类型 id，必要时先创建匿名记录。
- `make_type_id()`：根据已注册类型名查询运行期类型标识。
- `type_name<T>`：基础 C++ 类型到 CAORS 类型名的默认映射。
- `meta_type_data` / `make_meta_type_data<T>()`：底层类型注册描述。
- `meta_type`：全局注册表中的类型只读视图。
- `meta<T>()`：RTTR 风格的链式元类型注册入口。
- `meta_registry`：全局基础类型注册、按 id/name/guid/C++ 类型查询、继承关系判断和组件式元数据挂载。
- `meta_view<T...>`：按组件集合查询类型。
- `get_meta_registry()`：进程内全局元数据注册表。

相关测试位于 `tests/meta/` 和 `tests/core/`，测试组织规则见 [tests/README.md](../tests/README.md)。
