<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta

本文档说明 CAORS 类型元数据层的当前设计。`meta` 不是运行期游戏 ECS，而是围绕 `string_name` 和
`type_id` 组织 C++ 类型、对象、反射和序列化元信息的共享底座。脚本或动态类型后续应放到独立注册系统，
再通过上层 resolver 统一查询。`string_name` 定义在 `caors/core/string_name.h`，完整说明见
[core.md](core.md)。

## 设计目标

`meta` 第一阶段解决几件事：

- 用 `string_name` 保存运行期去重后的类型名。
- 用 `type_id` 表示由 C++ 类型运行期 key 派生的进程内类型句柄。
- 用 `type_guid` 保存可选的稳定类型标识。
- 用注册表内部基础类型数据保存类型名、稳定 GUID 和父类型。
- 用内部 type descriptor 保存 C++ 类型布局、默认构造、复制、析构、赋值和运行期视图能力。
- 用内部 type descriptor 保存类型形态、通用类型关系和具体访问函数表。
- 用 `meta::type` 作为对外只读类型视图，避免直接暴露注册表内部数据。
- 用全局 `meta::registry` 集中处理基础类型注册、查找和继承关系判断。
- 用全局 `meta::registry` 在同一个 `type_id` 上挂载不同主题的元数据组件。
- 避免对象系统、反射系统和序列化系统各自重复实现类型注册逻辑。

当前不会包含：

- 运行期实体 ECS。
- 脚本或动态类型注册中心。
- system 调度。
- 对象生命周期管理器。
- 具体字段、方法或序列化格式。

这些能力应由对象、反射和序列化模块基于 `meta` 扩展。

新代码可以直接包含 `caors/meta.h` 使用完整类型元数据 API；如果只需要局部能力，也可以包含对应子头文件：

- `caors/meta/type_id.h`：`type_id` 和 `meta::type_guid` 兼容类型别名。
- `caors/core/type_guid.h`：`type_guid` 稳定类型标识。
- `caors/traits/type_name.h`：`traits::type_name<T>` 默认类型名映射。
- `caors/traits/wrapper.h`：`traits::wrapper_traits<T>` 和 `traits::wrapper_access<T>` 主模板。
- `caors/traits/container.h`：顺序、集合、关联、pair、tuple 和 variant 类型语义 traits 主模板。
- `caors/meta/type.h`：`meta::type`、类型语义数据和内部类型描述实现。
- `caors/adapters/stl_traits.h`、`caors/adapters/stl_wrapper.h`：STL/CAORS 类型的默认 traits 特化。
- `caors/meta/view.h`：`meta::view<T...>` 查询视图。
- `caors/meta/registry.h`：全局 `meta::registry`、注册错误状态和注册 helper。

## 核心概念

### string_name

`string_name` 是运行期字符串名。相同文本会共享同一个运行期 entry，适合名称比较和查找：

```cpp
caors::string_name name{"game.player"};
std::string_view view = name.view();
```

`string_name` 不暴露运行期 id，不承诺跨进程或跨版本稳定。

### type_id

`type_id` 是运行期类型标识。`0` 保留为无效值。对于 C++ 类型，它由库内部 C++ type key 派生，
只保证当前进程内可用于注册表索引，不承诺跨进程、跨版本或跨动态库边界稳定。

```cpp
caors::meta::type_<player>("game.player");
caors::type_id id = caors::meta::id_of<player>();
```

类型注册后，也可以通过名称查询当前进程内的运行期 id：

```cpp
caors::meta::type_<player>("game.player");
caors::type_id id = caors::meta::id("game.player");
```

`type_id` 适合当前进程内的运行期查找，不适合作为序列化长期稳定 ID。需要长期稳定身份时使用
`type_guid`。

在 DLL 或插件边界，同一 C++ 类型如果被多个模块分别实例化内部 type key，运行期 `type_id` 不承诺相同。
当前约定是同一个可反射 C++ 类型只由一个模块负责注册；跨模块、跨进程或存档数据里的稳定身份使用
`type_guid`，必要时再结合类型名和版本迁移策略。

注册 C++ 类型时，注册表还会记录 `typeid(T) -> type_id` 的进程内映射。该映射只用于运行期 RTTI
辅助，例如普通 C++ 多态 wrapper 从 `Base*` 查到真实 `Derived` 类型；它同样不承诺跨进程或跨动态库稳定。

### type_guid

`type_guid` 是稳定类型标识，定义在 `caors/core/type_guid.h`，基于 `guid` 包装。它用于序列化、存档、资源引用和插件边界等需要长期稳定识别类型的场景。

```cpp
auto guid = caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
caors::meta::type_<player>("game.player")
    .guid(caors::type_guid{*guid});
```

需要注意：

- 未显式传入时，`meta::type::guid()` 为 nil。
- `type_guid` 不应由 `type_id`、`string_name` 的运行期 entry 或哈希值派生。
- 修改类型名不应自动修改 `type_guid`，否则旧数据无法稳定识别类型。

### meta::type

基础类型数据由全局注册表内部保存，普通代码不直接构造或修改。对外查询统一返回
`meta::type`，它提供以下只读信息：

- `id`
- `name`
- `guid`
- `size`
- `alignment`
- `parent_id`

注册时会记录该 C++ 类型的运行期 `type_id`。`size()` / `alignment()` 不来自基础类型数据本身，
而是从同一 `type_id` 挂载的内部 `meta::detail::type_descriptor` 查询；默认构造、复制、析构和赋值函数
也保存在该 descriptor 中。普通代码不应直接修改或依赖 descriptor 的具体布局。
`meta::id_of<T>()` 只查询已经注册的 C++ 类型，不会创建匿名类型记录；普通代码应在启动注册阶段直接使用
`meta::type_<T>("name")`。
`meta::type_<T>()` 链式 builder 会原地更新全局注册表中已经保存的类型数据；已有 `meta::type`
视图只引用这份稳定数据，不会因为后续 `.guid()` 或 `.parent()` 调用而失效。

`meta::type` 是对外查询返回的只读视图：

```cpp
caors::meta::type type = caors::meta::find<player>();

if (type) {
  caors::type_id id = type.id();
  std::string_view name = type.name().view();
}
```

`meta::type` 不拥有数据，内部引用全局 append-only 注册表中的基础类型数据。需要查询类型信息时
使用 `meta::type`。

### traits::type_name<T>

`traits::type_name<T>` 是 C++ 类型到 CAORS 类型名的默认映射。内置基础类型已经提供默认名称，例如：

```cpp
caors::traits::type_name<int>::value;   // "int"
caors::traits::type_name<float>::value; // "float"
```

用户类型可以通过特化 `traits::type_name<T>` 提供默认名称，也可以直接通过 `meta::type_<T>("name")`
显式注册。模板注册入口会把 C++ 类型和注册后的 `meta::type` 绑定起来，后续可以用 C++ 类型查询：

```cpp
caors::meta::type_<actor>("game.actor");

auto type = caors::meta::find<actor>();
```

也可以使用 RTTR 风格入口：

```cpp
caors::meta::type_<actor>("game.actor")
    .guid(caors::type_guid{*guid});
```

如果类型需要表达 C++ 继承关系，推荐使用 typed parent 注册：

```cpp
caors::meta::type_<player>("game.player")
    .parent<actor>();
```

`.parent<actor>()` 不只保存 `parent_id`，还会保存 `player* -> actor*` 的安全 upcast 回调。普通类
多态序列化读入 `std::unique_ptr<actor>` / `std::shared_ptr<actor>` 时需要这份回调把新建的
`player` 对象交给声明基类 wrapper。`.parent(type_id)` 仍可用于只需要关系查询的场景，但不会提供
typed upcast 能力。

`caors::meta::find(...)` 和 `caors::meta::find<T>()` 只查询已经注册的类型，不会创建临时记录；
不存在时返回无效 `meta::type`。普通类型应通过 `caors::meta::type_<T>("name")` 显式注册。
`meta::id_of<T>()` 同样只查询已注册类型，不会触发运行期隐式注册。

### meta::detail::type_descriptor

`meta::detail::type_descriptor` 是已注册 C++ 类型的内部能力描述。注册表按 `type_id`
保存指向静态 descriptor 的指针，不把 descriptor 当作普通 generic component 复制存储。descriptor
由布局、值操作、类型语义、主访问能力和对象引用能力几部分组成；未注册结构类型也可以通过 `any`、
`any_ref` 或反射字段携带的静态 descriptor 访问同一套能力。

descriptor 内部按职责拆分：

- `type_layout`：保存 `size` 和 `alignment`。
- `value_ops`：保存默认构造、复制、析构和赋值的类型擦除函数。
- `type_semantics`：保存该类型在反射、序列化和 value 模块中的运行期形态。
- `type_capability`：保存一个主访问能力，例如枚举、间接值、顺序容器、集合、关联容器、pair、tuple 或 variant。
- `objects::detail::object_ref_data`：对象引用能力，它独立于主访问能力。

`type_semantics` 描述以下语义：

- `kind`：值、枚举、裸指针、wrapper、顺序容器、集合容器、关联容器、二元组、tuple-like 或 variant-like 类型。
- `traits::wrapper_semantics`：wrapper 或裸指针的处理语义，例如内联值或不支持。
- `indirect`：该类型是否具备一层间接值语义，例如裸指针或 wrapper。
- `nullable`：运行期值是否可能为空。

具体访问函数表由 `type_capability` 或对象引用能力持有，用来描述枚举转换、容器、指针、wrapper、对象引用、
pair、tuple 和 variant 访问能力：

- `meta::detail::indirect_data`：裸指针或 wrapper 的内部值访问、持值检测、重置和确保持值函数。
- `meta::detail::enum_data`：枚举类型和整数之间的转换函数。
- `objects::detail::object_ref_data`：对象引用类型和对象指针之间的读写函数，和 `type_capability` 独立保存。
- `meta::detail::sequence_data` / `set_data` / `associative_data` / `pair_data` / `tuple_data` / `variant_data`：容器、二元组、tuple-like 和 variant-like 的运行期访问函数。

统一 `value_type` / `key_type` 不再作为 `type_descriptor` 顶层字段保存。普通值和枚举直接使用
descriptor 自身的 `type_id`；裸指针、wrapper、容器、pair、tuple 和 variant 的子类型信息由对应
访问函数表保存，并通过 `field`、`sequence_view`、`pair_view` 等公开视图查询。

类型形态和通用类型关系只从 descriptor 查询，不再把语义数据、枚举数据、容器数据或间接访问数据作为
独立 meta 组件挂载到注册表。反射字段会保存声明类型的 `type_id` 和 descriptor，因此
`std::vector<T>`、`std::optional<T>`、`std::tuple<...>` 这类结构类型不要求
自身注册为 meta 类型。未注册类型不会拥有类型名、GUID、反射字段或序列化组件，但仍可通过
字段、`any` / `any_ref` 或嵌套容器元素携带的 descriptor 使用布局、值操作和运行期视图能力。

meta 不会因为存在 `traits::type_name<T>` 就在运行期隐式创建业务类型。需要稳定名称、GUID、对象工厂、
反射字段列表或多态序列化身份的 C++ 类型仍应通过 `meta::type_<T>("name")` 显式注册。

### meta::registry

`meta::registry` 是进程级全局类型元数据注册表。它直接管理基础类型信息，并允许在同一个 `type_id` 上挂载对象、反射、序列化等元数据组件。普通代码不需要创建 registry 对象：

```cpp
caors::error result = caors::meta::type_<actor>("game.actor").result();
```

注册表只面向 C++ 类型。脚本或动态类型不应直接写入当前注册表，后续应放到独立系统，再由上层 resolver
统一查询。注册表是 append-only：基础类型注册后不可清理或撤销，重复注册相同类型会返回重复结果。
当前组件同样按 append-only 方式挂载：同一类型的同类组件注册后不可替换。反射和序列化这类组件
反射字段数据也按“注册后冻结”处理，外部视图只在该约束下承诺进程生命周期内稳定。

推荐生命周期：

```cpp
void register_game_types() {
  caors::meta::type_<int>("int");
  caors::meta::type_<actor>("game.actor");
  caors::reflection::class_<actor>(caors::meta::find<actor>())
      .field("id", &actor::id)
      .commit();
}

int main() {
  register_game_types();
  caors::meta::freeze_registry();
  run_app();
}
```

也可以把局部注册函数挂到自动注册列表，由 `meta::freeze_registry()` 统一执行：

```cpp
CAORS_META_REGISTRATION(register_actor_meta) {
  caors::meta::type_<int>("int");
  caors::meta::type_<actor>("game.actor");

  return caors::reflection::class_<actor>(caors::meta::find<actor>())
      .field("id", &actor::id)
      .commit();
}
```

`meta::freeze_registry()` 会先执行所有通过 `CAORS_META_REGISTRATION` 或内部延迟注册入口收集到的注册函数，
再校验 meta 层索引和父类型关系，最后把注册表切换为只读。冻结后，基础类型注册、组件挂载都会失败；
查询接口仍可使用。

`CAORS_META_REGISTRATION(name)` 只是便利入口，不建议作为唯一主流程。它依赖静态对象把函数加入列表：
静态库中未被链接进最终程序的翻译单元不会执行注册；注册函数执行顺序也不应被依赖。需要精确控制初始化、
测试隔离或插件加载时，优先使用显式 `register_all()`。

注册表维护几类索引：

- `type_id -> 内部基础类型数据`：运行期主索引，只保存身份类信息。
- `type_id -> const meta::detail::type_descriptor*`：布局、值操作、类型语义、主访问能力和对象引用能力。
- `typeid(T) -> type_id`：当前进程内 RTTI 查询索引，用于普通 C++ 多态辅助。
- `type_id -> parent upcast`：typed parent 注册保存的直接父类 upcast 回调。
- `type_name -> type_id`：名称查询索引，可选。
- `type_guid -> type_id`：稳定身份查询索引，可选。

注册失败会返回 `caors::error`，`domain()` 为 `error_domain::meta`，`value()` 为明确的通用错误码：

- `errc::invalid_type`：类型描述无效。
- `errc::duplicate_id`：同一个运行期类型记录已注册。
- `errc::duplicate_name`：同一个名称已注册。
- `errc::duplicate_guid`：同一个非 nil 稳定 GUID 已注册。
- `errc::missing_parent`：父类型未注册。
- `errc::frozen`：注册表已冻结，不能继续注册类型或挂载组件。

使用方式是先注册基础类型，再给该类型挂载组件：

```cpp
struct object_factory_meta {
  bool constructible = false;
};

caors::meta::type_<actor>("game.actor");

caors::type_id actor_id = caors::meta::find<actor>().id();
caors::meta::get_registry().emplace<object_factory_meta>(actor_id, true);
```

组件 API：

- `emplace<T>(type_id, args...)`：给已注册类型挂载组件。
- `has<T>(type_id)`：判断类型是否拥有组件。
- `find<T>(type_id)`：获取组件副本。
- `find_ptr<T>(type_id)`：获取组件只读指针。组件由注册表以 `unique_ptr` 保存，地址在 append-only
  注册表生命周期内保持稳定。
- `ensure_component<T>(type_id, args...)`：按需组件创建 helper，组件已存在时返回现有指针，不存在时创建并返回
  新组件指针。
- `component_status<T>(type_id)`：查询组件诊断状态，区分无效类型、未注册类型、缺失组件和已挂载组件。
- `view<T...>()`：查询同时拥有指定组件集合的类型。

容器 descriptor 会在具体容器支持 `begin()` / `end()` 时额外保存类型擦除 iterator 函数，用于
`set_view` 和 `associative_view` 的 range-for 遍历。只提供 traits callback 的自定义容器仍可通过
`for_each_ref()` 遍历。容器 view 只依赖 descriptor，不再为了创建 view 把每一种容器组合额外挂载为
meta 组件。

组件挂载同样返回 `caors::error`，`domain()` 为 `error_domain::meta`：

- `errc::invalid_type`：传入无效 `type_id`。
- `errc::missing_type`：目标类型尚未注册。
- `errc::duplicate_component`：目标类型已经挂载同类组件。
- `errc::frozen`：注册表已冻结。

`component_status<T>()` 不修改注册表，适合在模块重复注册、热加载诊断或报错前检查当前状态。当前注册表仍是
append-only 语义，不支持组件替换；如果后续支持热重载，应在替换 API 上显式处理旧组件生命周期和读者同步。

## 线程安全边界

`meta::registry` 的注册、查询和组件挂载入口内部仍会加锁，避免注册表容器自身出现 data race。推荐在启动期
或模块加载期单线程完成注册，并调用 `meta::freeze_registry()`。冻结后的注册表只允许只读查询，返回的
`meta::type` 和各模块视图是指向注册表内部数据的轻量只读视图，适合长期保存和并发读取。

当前不会支持运行期并发写注册表。脚本类型、异步加载、卸载和热重载应放到独立注册系统中处理。

## 与对象和反射的关系

全局 `meta::registry` 是共享元数据层：

- `objects::registry` 复用全局基础类型信息，再给同一个 `type_id` 挂载 `objects::detail::type_data` 组件，并对外返回 `objects::type` 视图。
- 全局反射注册表复用基础类型信息，再挂载内部反射组件，并通过 `reflection::type` / `reflection::field` 对外查询。
- 后续 `serialization::registry` 可以挂载序列化相关组件。

这样对象、反射和序列化的全局注册入口不需要重复处理重名、重复 GUID、不一致类型描述和父类型缺失。
跨模块查询可以通过 `meta::view<T...>()` 完成。反射这类内部组件应通过对应模块 API 返回稳定只读视图；
序列化等公开组件仍可通过 `meta::view<serialization::type_data>()` 查询。

## 当前已实现能力

- `type_id`：运行期类型标识。
- `type_guid`：稳定类型标识。
- `string_name`：运行期 interned 名称，定义在 `caors/core/string_name.h`。
- `meta::id_of<T>()`：查询已注册 C++ 类型的运行期类型 id；未注册时返回无效 id。
- `meta::id()`：根据已注册类型名查询运行期类型标识。
- `traits::type_name<T>`：基础 C++ 类型到 CAORS 类型名的默认映射。
- `meta::detail::type_descriptor`：布局、值操作、类型语义、主访问能力和对象引用能力。
- `traits::wrapper_traits<T>` / `traits::wrapper_access<T>`：wrapper 类型语义和内部值访问扩展点。
- `traits::sequence_traits<T>` / `traits::set_traits<T>` / `traits::associative_traits<T>` / `traits::pair_traits<T>` / `traits::tuple_traits<T>` / `traits::variant_traits<T>`：容器、二元组、tuple-like 和 variant-like 类型语义扩展点。
- `meta::type`：全局注册表中的类型只读视图。
- `meta::type_<T>()`：RTTR 风格的链式元类型注册入口；默认不注册类型，需要继续调用 `.type("name")`。
- `meta::type_<T>("name")`：推荐的显式命名注册入口。
- `meta::type_<T>().parent<Base>()`：保存 C++ 父类型关系和安全 upcast 回调。
- `meta::registry`：全局基础类型注册、按 id/name/guid 或 C++ 类型查询、继承关系判断和组件式元数据挂载。
- `meta::view<T...>`：按组件集合查询类型。
- `meta::run_registrations()`：执行已收集的延迟元数据注册函数。
- `CAORS_META_REGISTRATION(name)`：定义一个延迟注册函数，函数体需要返回 `caors::error`。
- `meta::get_registry()`：进程内全局元数据注册表。
- `meta::freeze_registry()` / `meta::is_registry_frozen()`：执行延迟注册函数、冻结全局注册表并查询冻结状态。

相关测试位于 `tests/meta/` 和 `tests/core/`，测试组织规则见 [tests/README.md](../tests/README.md)。
