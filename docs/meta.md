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
- 用注册表内部 meta 类型记录保存类型名、稳定 GUID、父类型、属性和成员方法。
- 用 `meta::type` 暴露 C++ 类型布局、默认构造、复制、析构、赋值和运行期视图能力。
- 用 `meta::type` 暴露类型形态、通用类型关系和具体访问能力。
- 用可选 factory capability 保存资源感知构造策略，供普通类多态反序列化等需要运行期创建值的路径使用。
- 用 `meta::type` 作为对外只读类型视图，避免直接暴露注册表内部数据。
- 用 `meta::context` 定位当前使用的 `meta::registry`，默认上下文覆盖普通全局用法。
- 用 `meta::registry` 集中处理 meta 类型注册、查找、继承关系判断和扩展数据索引。
- 避免对象系统、反射系统和序列化系统各自重复实现类型注册和属性/方法索引逻辑。

当前不会包含：

- 运行期实体 ECS。
- 脚本或动态类型注册中心。
- system 调度。
- 对象生命周期管理器。
- 序列化格式。

属性和成员方法现在直接保存在 `meta` 类型记录上；不再保留独立 `reflection` 模块。
对象、资源和序列化等领域能力仍由对应模块基于 `meta` 扩展。

新代码可以直接包含 `caors/meta.h` 使用完整类型元数据 API；如果只需要局部能力，也可以包含对应子头文件：

- `caors/meta/type_id.h`：`type_id` 和 `meta::type_guid` 兼容类型别名。
- `caors/core/type_guid.h`：`type_guid` 稳定类型标识。
- `caors/traits/type_name.h`：`traits::type_name<T>` 默认类型名映射。
- `caors/traits/wrapper.h`：`traits::wrapper_traits<T>` 和 `traits::wrapper_access<T>` 主模板。
- `caors/traits/container.h`：顺序、集合、关联、pair、tuple 和 variant 类型语义 traits 主模板。
- `caors/meta/type.h`：`meta::type` 只读类型视图和类型语义枚举。
- `caors/meta/type_creation.h`：调用 `meta::type::create()` / `create_value()` 时需要的完整依赖头。
- `caors/meta/context.h`：`meta::context` 和 `meta::default_context()` 注册表定位入口。
- `caors/meta/query.h`：`meta::resolve<T>()`、`meta::find<T>()`、`meta::id_of<T>()` 等查询入口。
- `caors/registration.h`：`registration::type_<T>(name)` 和 `registration::enum_<T>(name)` 跨模块注册入口。
- `caors/meta/property.h`、`caors/meta/method.h`：属性和成员方法只读视图。
- `caors/registration/type_builder.h`：普通类型的 meta、serialization 和 object 分段注册。
- `caors/meta/capability_traits.h`：capability key、traits 和类型/实例能力查询扩展点。
- `caors/meta/static_capability.h`：按 C++ 类型聚合静态能力的模块化扩展点。
- `caors/meta/capability_facade.h`：类型级 capability facade 基类，以及 factory/constructor facade。
- `caors/meta/type_ops.h`：内建 ops 聚合头；更细粒度可以包含 `caors/meta/ops/*.h`。
- `caors/meta/value_ops.h`：值布局、生命周期、factory 和 constructor 的底层 ops 结构体。
- `caors/meta/value.h`：`meta::value`、`meta::any`、`meta::any_ref`、运行期容器 view 和
  `meta::variant` 聚合头。
- `caors/adapters/stl_traits.h`、`caors/adapters/stl_wrapper.h`：STL/CAORS 类型的默认 traits 特化。
- `caors/meta/extension_view.h`：`meta::extension_view<T...>` 查询视图。
- `caors/meta/registry.h`：`meta::registry`、注册错误状态和注册 helper。

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
caors::registration::type_<player>("game.player");
caors::type_id id = caors::meta::id_of<player>();
```

类型注册后，也可以通过名称查询当前进程内的运行期 id：

```cpp
caors::registration::type_<player>("game.player");
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
caors::registration::type_<player>("game.player")
    .guid(caors::type_guid{*guid});
```

需要注意：

- 未显式传入时，`meta::type::guid()` 为 nil。
- `type_guid` 不应由 `type_id`、`string_name` 的运行期 entry 或哈希值派生。
- 修改类型名不应自动修改 `type_guid`，否则旧数据无法稳定识别类型。

### meta::type

meta 类型记录由当前 `meta::context` 指向的注册表内部保存，普通代码不直接构造或修改。对外查询统一返回
`meta::type`，它提供以下只读信息：

- `id`
- `name`
- `guid`
- `size`
- `alignment`
- `parent_id`

`meta::type` 是当前类型系统的公共视图。`meta::resolve<T>()` 和 `meta::id_of<T>()`
可以为未显式注册的 C++ 类型返回有效类型视图和运行期 `type_id`。
`size()` / `alignment()`、默认构造、复制、析构和赋值能力也通过这个视图查询。
`registered()` 表示该类型是否已经通过 `registration::type_<T>("name")` 写入稳定注册数据；只有已注册类型
才有稳定注册名、GUID、父类型和可挂载 extension。
`meta::find<T>()` 只查询已经注册的 C++ 类型；未注册类型即使能解析 descriptor，也会返回空视图。
`registration::type_<T>("name")` 链式 builder 在 `commit()` 时一次性发布完整类型数据；发布后的
`type_data` 不再通过 builder 修改。已有 `meta::type` 视图引用稳定注册数据，重复提交同一类型会返回重复错误。
属性和成员方法也挂在同一份类型数据上，应在同一次注册提交前通过 `.meta()` 段写入。

`meta::type` 是对外查询返回的只读视图：

```cpp
caors::meta::type type = caors::meta::resolve<player>();

if (type) {
  caors::type_id id = type.id();
  std::string_view name = type.name().view();
}
```

属性和成员方法可以直接通过 `meta::type` 查询：

```cpp
struct actor {
  int health = 100;
  int add_health(int value);
};

caors::registration::type_<actor>("game.actor")
    .meta()
    .property("health", &actor::health)
    .method("add_health", &actor::add_health)
    .commit();

caors::meta::type type = caors::meta::find<actor>();
caors::meta::property health = type.find_property("health");
caors::meta::method add_health = type.find_method("add_health").single();
```

属性、方法和实例也提供 `meta::value` 高层运行期入口，适合脚本、编辑器、消息系统等动态调用场景。
直接使用 `meta::value{42}` 会保存为运行期 `int64_t`。属性读取、属性写入、方法参数和方法返回会优先在
常见 tagged value 与目标 C++ 类型之间直接转换；当前覆盖布尔、整数、浮点、`string_name`、字符串和
`meta::object*`。`instance::call("fn", 5)` 采用高层动态语义，会优先把参数构造成 `meta::value`；
复杂 C++ 类型、引用参数和需要精确 C++ 类型的调用仍应使用 `meta::value::from_any(T)`、`set_any()`
或 `call_any()`。

调用入口按语义分层：

- `instance::get()` / `property::get_value()`：返回 `meta::value`，常见类型直接走 tagged value。
- `instance::set()` / `property::try_set_value()`：从 `meta::value` 写入，适合脚本、编辑器和消息层；
  `instance.set("health", 120)` 这类常见 C++ 标量调用也会先转换为 `meta::value`。
- `instance::call()` / `method::invoke()`：使用 `meta::value` 参数和返回值，作为普通动态调用入口。
- `get_any()` / `set_any()` / `call_any()` / `invoke_result_of()`：保留精确 C++ 类型、引用返回和高级调试语义。

```cpp
actor instance{};
caors::meta::instance object = caors::meta::view(instance);

object.set("health", caors::meta::value{120});

std::array<caors::meta::value, 1> args{
    caors::meta::value{5},
};
caors::meta::invoke_value_result result = object.call("add_health", args);

if (result.has_value()) {
  caors::meta::value returned = result.value();
}
```

按名字访问的 `instance::get()`、`instance::set()` 和 `instance::call()` 是便利入口，会在每次调用时
按名称查询属性或方法。初始化、编辑器面板刷新、脚本偶发调用可以直接使用；热点循环应先缓存
`meta::property`、`meta::method_set` 或具体 `meta::method`，循环里只绑定实例和参数：

```cpp
caors::meta::type type = caors::meta::find<actor>();
caors::meta::property health = type.find_property("health");
caors::meta::method_set add_health = type.find_method("add_health");

actor instance{};
caors::meta::instance object = caors::meta::view(instance);

health.try_set_value(object, caors::meta::value{120});

std::array<caors::meta::value, 1> args{caors::meta::value{5}};
caors::meta::invoke_value_result add_result = add_health.invoke(object, args);
```

高级精确 C++ 入口仍然保留：属性拷贝值使用 `property::get_any()` 返回 `any`，属性引用使用
`property::get_ref()` 返回 `any_ref`，属性精确写入使用 `property::try_set_any()`，方法完整调用结果使用 `invoke_result_of()` 返回
`invoke_result`。`invoke_value_result` 返回高层 `meta::value`，常见返回值会直接保存为 tagged value，
复杂返回值才回退为 `value::from_any()`，同时仍保留失败原因和 `void` 成功语义。

`find_property()` 和 `find_method()` 默认只查询当前类型声明的成员。需要查询父类成员时显式传入
`meta::member_search::include_bases`；同名成员由子类优先返回，遍历 API 仍只遍历本类型成员：

```cpp
auto inherited_id = caors::meta::find<player>().find_property(
    "id", caors::meta::member_search::include_bases);
```

`meta::type` 不拥有数据；类型已注册时引用所属 registry 中 append-only 保存的 meta 类型记录。
未注册类型仍可以作为 C++ 类型视图参与 value、反射字段和序列化内部调度。需要查询类型信息时使用
`meta::type`。

类型查询按用途分为两条路径：

- `meta::resolve<T>()`：解析 C++ 类型视图，优先返回已注册 `meta::type`；没有注册时退回
  C++ 类型视图。反射、序列化、value 和对象模块在只需要布局、类型形态、内置 ops、
  static capability 或 codec 时统一使用这个入口。
- `meta::find<T>()` / `meta::find(name/guid)`：查询已注册类型。需要稳定名称、GUID、父类型、
  factory 或模块 extension 索引时使用这个入口；未注册 C++ 类型返回空视图。

### meta::context

`meta::context` 是 meta API 的 registry 定位器。普通代码直接使用默认上下文：

```cpp
caors::registration::type_<actor>("game.actor").commit();
caors::meta::type type = caors::meta::find<actor>();
```

需要测试隔离、工具沙箱或插件注册隔离时，可以创建独立 registry 并显式传入 context：

```cpp
caors::meta::registry registry;
caors::meta::context ctx{registry};

caors::registration::type_<actor>(ctx, "game.actor").commit();
caors::meta::type type = caors::meta::find<actor>(ctx);
```

同一个 C++ 类型在不同 registry 中可以有独立的注册数据。`meta::resolve<T>(ctx)` 返回的
未注册类型视图也会绑定到该 context 的 registry，因此后续通过 `meta::type` 查询动态
capability 时会查找所属 registry。默认 `meta::resolve<T>()`、`meta::find<T>()`、
`registration::type_<T>(name)` 和 `registration::enum_<T>(name)` 都等价于显式传入
`meta::default_context()`。

### traits::type_name<T>

`traits::type_name<T>` 是 C++ 类型到 CAORS 类型名的默认映射。内置基础类型已经提供默认名称，例如：

```cpp
caors::traits::type_name<int>::value;   // "int"
caors::traits::type_name<float>::value; // "float"
```

用户类型可以通过特化 `traits::type_name<T>` 提供默认显示名，也可以直接通过
`registration::type_<T>("name")` 显式注册。默认显示名只用于属性、`any` 和容器视图的调试/展示回退，
不等价于 meta 注册；模板注册入口会把 C++ 类型和注册后的 `meta::type` 绑定起来，后续可以用
C++ 类型查询：

```cpp
caors::registration::type_<actor>("game.actor").commit();

auto type = caors::meta::find<actor>();
```

也可以使用 RTTR 风格入口：

```cpp
caors::registration::type_<actor>("game.actor")
    .guid(caors::type_guid{*guid})
    .commit();
```

如果类型需要表达 C++ 继承关系，推荐使用 typed parent 注册：

```cpp
caors::registration::type_<player>("game.player")
    .parent<actor>()
    .commit();
```

`.parent<actor>()` 不只保存 `parent_id`，还会保存 `player* -> actor*` 的安全 upcast 回调。普通类
多态序列化读入 `std::unique_ptr<actor>` / `std::shared_ptr<actor>` 时需要这份回调把新建的
`player` 对象交给声明基类 wrapper。`.parent(type_id)` 仍可用于只需要关系查询的场景，但不会提供
typed upcast 能力。

如果类型需要表达 C++ 嵌套类型关系，先把 nested 类型作为独立 meta 类型注册，再在 owner 的 meta
段记录关系：

```cpp
struct outer {
  struct inner {};
  enum class kind { local };
};

caors::registration::type_<outer::inner>("game.outer.inner").commit();
caors::registration::enum_<outer::kind>("game.outer.kind").commit();

caors::registration::type_<outer>("game.outer")
    .meta()
    .nested<outer::inner>("inner")
    .nested("kind", caors::meta::find<outer::kind>())
    .commit();

caors::meta::type outer_type = caors::meta::find<outer>();
caors::meta::type inner_type = outer_type.find_nested("inner");
```

`nested_count()` / `nested_name_at(index)` / `nested_at(index)` 用于遍历本类型声明的 nested 列表；
`find_nested(name)` 按 owner 内部使用的短名查询。nested 目标必须已经注册，否则提交返回
`errc::missing_type`。nested 关系不会把 owner 和 nested 类型合并成同一个生命周期单元，也不会隐式注册
nested 类型。

如果类型需要在运行期由序列化层创建，可以注册资源感知 factory：

```cpp
void* construct_player(caors::memory_resource* resource);
void destroy_player(caors::memory_resource* resource, void* value) noexcept;

caors::registration::type_<player>("game.player")
    .guid(player_guid)
    .parent<actor>()
    .factory(&construct_player, &destroy_player);
```

`.factory()` 无参数重载使用 `memory::make<T>()` / `memory::destroy<T>()` 注册默认构造策略，要求 `T`
可默认构造。带函数指针的重载用于不可默认构造类型，`destroy` 必须匹配 `construct` 的分配策略。
应用层需要创建运行期值时优先使用 `meta::type::create()`，它会先通过
`meta::type::get_capability<meta::factory_capability>()` 查询这里注册的 factory，再回退到 descriptor
默认构造。如果确实只需要 meta 层创建结果，可以使用 `factory_capability::construct(resource)` 直接返回
拥有生命周期的 `meta::any`；需要先填充对象再转移所有权时，使用
`factory_capability::construct_owned(resource)` 返回 `meta::owned_value`。裸指针构造和销毁只存在于底层
`factory_ops` / ops/model 实现中；需要把外部高级构造结果转成 RAII 句柄时，使用
`meta::make_owned_value()`。公开 facade 不暴露 `void*`，`any::default_construct()` 只表达 descriptor
默认构造能力，不查询 factory。

如果类型需要按运行期参数列表构造，可以注册 constructor capability：

```cpp
struct actor {
  explicit actor(int health) : health(health) {}
  int health = 0;
};

caors::registration::type_<actor>("game.actor")
    .constructor<int>()
    .commit();
```

`constructor<Args...>()` 使用 `std::span<const meta::argument>` 参数列表做精确匹配：参数数量必须一致，
参数类型必须和 `Args...` 去 cv/ref 后一致；非 const 左值引用参数要求通过 `caors::arg(ref)`
传入可写引用。
`meta::type::create(arguments)` 会在 factory 之后查询 constructor，构造成功后同样由 `any`
保存匹配的内部销毁策略。也可以使用 `constructor_capability::construct(arguments, resource)` 直接获得
`meta::any`，或使用 `constructor_capability::construct_owned(arguments, resource)` 获得可延迟转移的
`meta::owned_value`。当前版本只支持一个 constructor 能力，右值引用参数需要通过
`caors::move_arg(...)` 显式传入；构造函数重载、默认参数和隐式类型转换后续再扩展。

`caors::meta::resolve<T>()` 是 C++ 类型到 `meta::type` 视图的入口；类型未显式注册时仍可能返回有效
`meta::type`，但 `registered()` 为 false。`caors::meta::find<T>()` 是 C++ 类型到已注册类型的入口，
未注册时返回空视图。`caors::meta::find(id/name/guid)` 是运行期查询入口：按 `name` / `guid`
只能找到已注册类型，按 `id` 可以找到未注册 C++ 类型视图或已注册类型。
普通业务类型仍应通过 `caors::registration::type_<T>("name")` 显式注册，再把得到的 `meta::type` 交给
对象或序列化模块挂载 extension。

### 普通入口：facade 和 view

普通业务代码优先使用公开 facade，而不是直接操作内部 descriptor 或 raw ops：

- 类型查询使用 `meta::resolve<T>()` 或 `meta::find<T>()`。
- 类型级能力使用 `meta::type::get_capability<Capability>()`，例如
  `get_capability<meta::sequence_capability>()`。
- 实例级能力同样优先传入 capability 类型，例如
  `any_ref.get_capability<meta::sequence_capability>()` 返回 `sequence_view`，
  `any_ref.get_capability<objects::ptr_capability>()` 返回 `objects::ptr_view`。
- 跨 meta / object 的高层访问优先使用 `meta::type`、`meta::instance` 和对象模块自己的
  `objects::*` 入口。
- 公开 capability facade 不直接暴露 `void*`。需要返回未知类型值时使用 `meta::any`，
  需要引用已有对象时使用 `meta::any_ref` 或绑定后的 `*_view`；`void*` 只用于底层
  `*_ops`、model 和 `meta::detail` helper。

只有实现新的 capability facade、调试工具、序列化桥接或性能敏感适配层时，才直接使用
`meta::type::get_raw_ops<Ops>()`。

### 分层边界：descriptor、registry、capability

当前 meta 层按公共 API、注册表和能力三类边界分工：

- `meta::type` 是统一类型视图。调用方通过 `meta::resolve<T>()`、`meta::find<T>()`、`meta::find(id)` 和
  `meta::type::get_capability<C>()` 查询类型身份、注册数据和能力。
- `meta::registry` 是注册后元数据和模块数据索引。只有显式 `registration::type_<T>("name")` 注册后的类型，
  才拥有稳定名称、GUID、父类型关系、属性、方法、对象 extension 和序列化 extension 等全局可查询数据。
- static capability 用于类型固有能力。能力由模块在编译期按 `T` 生成，例如
  `objects::ptr<T>` 访问函数、对象 handle 函数表和 `codec_traits<T>` 生成的 codec 函数表。
  它们通过 `meta::type` 查询，不要求类型先注册到 registry。
- dynamic capability 用于用户或插件扩展能力。它挂在已注册类型的 `type_data` 上，
  适合矩阵、图、脚本桥接、编辑器扩展等 CAORS 不内置的 concept table。公开写入入口是
  `registration::type_<T>("name").capability(ops)`，由 CAORS 持有 `ops` 生命周期。
  `registration::type_<T>("name").factory()` 也是该机制的高级封装，会把构造策略注册为内部
  `meta::factory_ops`，公开查询时返回 `meta::factory_capability`。

默认查询顺序是：`meta::type::get_capability<C>()` 先查静态能力，再回退到
注册类型数据上的动态能力。普通调用方应使用这个统一入口，不直接关心能力来自 C++ 类型事实
还是注册类型数据。
如果只剩 `type_id`，应先通过 `meta::find(id)` 找回 `meta::type` 视图；只有确实需要直接查运行期扩展时，
才使用 `meta::capabilities::find<C>(id)`。

设计约束：

- C++ 类型事实只保存可由 `T` 推导的能力，不保存反射属性、序列化属性配置或对象注册数据。
- registry 保存注册后事实和模块 extension，不复制未注册 C++ 类型视图中的布局、子类型和内置 ops 数据。
- static capability 不应承担用户插件协议；它适合编译期可见、进程内静态生命周期的模块能力。
- dynamic capability 不是 meta extension，也不替代业务类型注册；需要稳定名称、GUID 或跨模块 extension 查询时仍要注册
  meta 类型。

### 内部结构：descriptor 和 ops

`meta::type` 的未注册 C++ 类型视图由内部 descriptor 支撑。descriptor 描述某个 `T` 的
`type_id`、默认显示名、布局、构造 / 析构 / 复制 / 赋值、类型语义，以及 CAORS 内置访问能力。
它只表示“这个 C++ 类型可以被描述”，不表示类型已经完成业务注册。

注册表按 `type_id`
保存指向静态 descriptor 的指针，不把 descriptor 当作普通 generic component 复制存储。descriptor
由布局、值操作、类型语义、内置 ops 和静态能力表几部分组成；未注册结构类型也可以通过 `any`、
`any_ref` 或反射属性携带的静态 descriptor 访问同一套能力。
普通用户通过 `meta::type`、`meta::resolve<T>()`
和 `meta::type::get_capability<T>()` 查询公开能力；需要实现自定义 facade、序列化桥接、编辑器适配或
性能敏感路径时，可以使用高级扩展入口 `get_raw_ops<T>()` 查询 raw ops 函数表。

descriptor 内部按职责拆分：

- `type_layout`：保存 `size` 和 `alignment`。
- `value_ops`：保存默认构造、复制、析构和赋值的类型擦除函数；相关函数指针类型嵌套在
  `value_ops` / `factory_ops` 内部，定义在 `meta/value_ops.h`，公开 factory 查询使用
  `factory_capability`。
- `type_semantics`：保存该类型在反射、序列化和 value 模块中的运行期形态。
- `builtin_ops`：保存一个固定内置互斥函数表，例如枚举、间接值、顺序容器、集合、关联容器、pair、tuple 或 variant。
- `static_capabilities`：保存模块声明的静态能力表，例如对象指针、对象句柄访问函数表和
  serialization codec 函数表。

文件边界上，`meta/detail/type_descriptor.h` 保存内部 descriptor 结构和查询 helper；
`meta/detail/type_data.h` 保存注册后的内部类型记录、属性和方法集合；
`meta/detail/type_descriptor_name.h` 负责 descriptor 默认显示名生成；
`meta/detail/type_descriptor_model.h` 负责 descriptor holder、语义填充和构建实现；
`meta/detail/builtin_ops_model.h` 作为内置 ops 聚合入口，只负责引入
`meta/detail/builtin/*.h` 并选择当前类型的内置 ops；`builtin/enum_ops_model.h`、
`builtin/indirect_ops_model.h`、`builtin/sequence_ops_model.h`、`builtin/set_ops_model.h`、
`builtin/associative_ops_model.h`、`builtin/pair_ops_model.h`、`builtin/tuple_ops_model.h` 和
`builtin/variant_ops_model.h` 分别负责对应能力的具名 static 回调和 `*_ops_model<T>`
生成；`builtin/container_iterator_ops_model.h` 提供通用 `container_iterator_ops`、
`container_iterator_ops_model<T>` 和 set / associative 复用的类型擦除迭代器 holder；
`meta/detail/parent_cast_ops.h` 只保留类型层级 parent cast 能力；`meta/type_semantics.h`
只保留类型语义判断 helper。

#### ops / capability 术语约定

- `capability` 表示能力槽位或可查询能力，例如“这个类型是否具备序列化 codec 能力”。
- `*_ops` 表示某个 capability 的具体类型擦除操作表，通常由函数指针和少量只读元数据组成，例如
  `sequence_ops`、`ptr_ops`、`handle_ops` 和 `codec_ops`。
- `*_capability` 表示类型级公开 facade，封装 `meta::type` 和内部 `*_ops`，提供更适合用户代码调用的
  方法。可绑定的 capability 会声明 `view_type`；例如 `meta::sequence_capability`
  可以绑定 `any_ref` 后得到 `sequence_view`，
  `serialization::codec_capability` 可以直接读写序列化数据。
- 外部实例访问优先使用绑定后的 `*_view`，例如
  `any_ref.get_capability<meta::sequence_capability>()` 或
  `any_ref.get_capability<objects::ptr_capability>()`。`*_ops` 是 raw ops 函数表，面向高级扩展与内部适配，
  不作为普通业务代码的首选入口。
- `*_fn` 类型别名应尽量嵌套在对应 `*_ops` 内，例如 `value_ops::assign_fn`、
  `factory_ops::construct_fn`、`codec_ops::write_fn` 和 `ptr_ops::get_fn`；除
  `registration_fn` 这类独立回调外，不在模块命名空间顶层继续暴露函数指针别名。
- `*_ops_model<T>` 表示针对具体 C++ 类型 `T` 生成 `*_ops` 的 model 层。model 负责把 `T`
  的成员函数、traits 或 codec 策略转换成稳定生命周期的静态 `*_ops` 表；`*_ops_ptr_of<T>()`
  只作为内部取表入口。
- `*_ops_model<T>` 内部优先使用具名 static 回调组装函数表，避免把类型擦除回调散落在匿名 lambda
  或临时初始化表达式里。局部遍历适配可以使用短 lambda，但对外保存到 `*_ops` 的函数指针应来自
  明确命名的 model 回调。
- descriptor 内置 ops 同样遵循这套规则：`enum_ops_model<T>`、`indirect_ops_model<T>`、
  `sequence_ops_model<T>`、`set_ops_model<T>`、`associative_ops_model<T>`、`pair_ops_model<T>`、
  `tuple_ops_model<T>` 和 `variant_ops_model<T>` 负责生成内置访问函数表；tuple 和 variant 会再用
  按索引展开的 element / alternative model 生成每个元素或候选类型的函数表。
- `*_data` 保留给注册表 extension、配置数据或纯元数据结构，不用于命名一组运行期分发函数。
- `*_ops` 不是 C++ ABI 层面的虚函数表，不依赖继承；它只是 CAORS 内部统一的函数表命名。
- `capability_info` 保存 capability 的进程内 key、可选稳定名称、可选稳定 GUID 和版本号。`capability_key`
  仍是当前进程内查询 key，稳定元信息只用于诊断、工具链和未来插件协议协商。

`type_semantics` 描述以下语义：

- `kind`：值、枚举、裸指针、wrapper、顺序容器、集合容器、关联容器、二元组、tuple-like 或 variant-like 类型。
- `traits::wrapper_semantics`：wrapper 或裸指针的处理语义，例如内联值或不支持。
- `indirect`：该类型是否具备一层间接值语义，例如裸指针或 wrapper。
- `nullable`：运行期值是否可能为空。

具体访问函数表由 `builtin_ops` 或模块 ops 持有，用来描述枚举转换、容器、指针、wrapper、
对象引用、pair、tuple 和 variant 访问能力。当前 descriptor 内置 ops 由 CAORS 定义，并且同一类型只会持有一个内置 ops；内部通过
`descriptor_ops<T>(descriptor)` 查询：

- `meta::indirect_ops`：裸指针或 wrapper 的内部值访问、持值检测、重置和确保持值函数。
- `meta::enum_ops`：枚举类型和整数之间的转换函数。
- `meta::sequence_ops` / `set_ops` / `associative_ops` / `pair_ops` / `tuple_ops` / `variant_ops`：容器、二元组、tuple-like 和 variant-like 的运行期访问函数。

模块静态能力通过 `meta::capabilities::module_static_capability_list<Module, T>` 声明。
`meta::capabilities::static_capability_modules<T>` 是模块 tag 聚合入口，默认包含 object、serialization
和 custom 三个模块 tag；外部模块可以特化 `extension_static_capability_modules<T>` 追加自己的 module tag，
再由 `static_capability_list<T>` 合并所有模块贡献的 `type_list<Capability...>`。`meta::type` 查询链路会把这些能力整理成一张静态
`capability_entry` 表。公开查询优先使用 `meta::type::get_capability<T>()` 或
`meta::type::has_capability<T>()`；该入口会先查询 C++ 类型静态能力，再回退到运行期
capability 索引。对象指针、对象句柄和 serialization codec 都通过 `meta::type` 的统一入口查询公开 facade；
内部 `ptr_ops`、`handle_ops` 和 `codec_ops` 只作为实现细节保留。

动态扩展能力通过 `registration::type_<T>("name").capability(ops)`、`registration::type_<T>("name").capability<Capability>()`
或内部稳定指针入口挂接到注册类型数据；类型必须通过 `registration::type_<T>("name").commit()`
注册。`meta::capabilities` 命名空间只保留静态能力提供者、
运行期查询和诊断工具。`capability(ops)` 会把一份用户自定义 concept table 复制或移动到注册类型数据中，
由 CAORS 持有其生命周期；查询仍通过 `meta::type::get_capability<Capability>()` 或
`meta::capabilities::find<Capability>(type_id)` 查询。默认用户 capability 返回只读指针；内置公开
facade 可以通过 `type_capability_traits` 返回轻量对象。拿得到 C++ 类型 `T` 时，也可以通过
`meta::capabilities::get<Capability, T>()` 直接查询静态能力。serialization codec 会作为静态能力接入
`meta::type` 查询链路，并通过 `serialization::codec_capability` facade 暴露；当运行期只剩 `type_id` 时，应先通过
meta registry 找回 `meta::type`。

capability 和 registry 的边界：

- C++ 类型静态能力适合“编译期已知 C++ 类型即可生成”的只读函数表，例如对象指针访问函数、
  句柄访问函数和 `codec_traits<T>` 生成的 codec 操作表。
- `meta::capabilities` 的动态查询适合“只剩 `type_id`，但仍需要查询某种用户外挂动态能力”的场景；
  内置 object / serialization 能力优先通过 `meta::type` 静态能力查询。
- 公开写入动态能力时优先使用 `registration::type_<T>("name").capability(ops)`，让 CAORS 持有用户自定义
  concept table。内部模块如果已经拥有进程生命周期的静态函数表，可以使用指针注册入口。
  如果能力来自 `static_capability<Capability, T>`，可以在类型注册后使用
  `registration::type_<T>("name").capability<Capability>()`。
- 普通调用方优先使用 `meta::type::get_capability<T>()`。该入口统一处理 C++ 类型静态能力和注册类型数据
  fallback，避免业务代码分别理解两套存储位置。
- capability 不是 meta extension。它不拥有类型注册数据，也不表示该 C++ 类型已经注册到
  `meta::registry`。需要稳定名称、GUID、父类型、反射属性、序列化属性配置或对象工厂时，仍应显式注册
  meta 类型和对应模块 extension。
- 当前 `capability_key` 是进程内模板静态地址，只适合当前进程内查询，不承诺跨动态库 ABI、跨进程或
  存档稳定。需要插件协议、脚本模块或工具链发现能力时，应特化 `capability_traits<T>` 提供稳定
  name/guid/version，并在协议层处理冲突和版本兼容。
- 多模块静态能力不应都挤到 `custom_static_capability_module`。外部模块应定义自己的 module tag，特化
  `module_static_capability_list<Module, T>` 贡献能力，再通过 `extension_static_capability_modules<T>`
  把 module tag 加入指定 C++ 类型的静态能力聚合列表。

统一 `value_type` / `key_type` 不再作为 descriptor 顶层属性保存。普通值和枚举直接使用
descriptor 自身的 `type_id`；裸指针、wrapper、容器、pair、tuple 和 variant 的子类型信息由对应
访问函数表保存，并通过 `field`、`sequence_view`、`pair_view` 等公开视图查询。

类型形态和通用类型关系只从 descriptor 查询，不再把语义数据、枚举 ops、容器 ops 或间接访问 ops 作为
独立 meta extension 挂载到注册表。反射属性会保存声明类型的 `type_id` 和 descriptor，因此
`std::vector<T>`、`std::optional<T>`、`std::tuple<...>` 这类结构类型不要求
自身注册为 meta 类型。未注册类型不会拥有 GUID、反射属性、对象工厂或序列化 extension；如果 descriptor
能通过 traits 推导默认显示名，`type_name()` 仍可返回该名称。无默认显示名的未注册业务类型名称为空，
但仍可通过属性、`any` / `any_ref` 或嵌套容器元素携带的 descriptor 使用布局、值操作和运行期视图能力。

meta 不会因为存在 `traits::type_name<T>` 就在运行期隐式创建业务类型。需要稳定名称、GUID、对象工厂、
反射属性列表或多态序列化身份的 C++ 类型仍应通过 `registration::type_<T>("name")` 显式注册。

### meta::registry

`meta::registry` 是进程级全局类型元数据注册表。它直接管理 meta 类型记录，并在同一个 `type_id` 下保存属性、方法、对象、序列化等元数据。普通代码不需要创建 registry 对象：

```cpp
caors::error result = caors::registration::type_<actor>("game.actor").commit();
```

注册表只面向 C++ 类型。脚本或动态类型不应直接写入当前注册表，后续应放到独立系统，再由上层 resolver
统一查询。注册表是 append-only：meta 类型注册后不可清理或撤销，重复注册相同类型会返回重复结果。
当前对象、序列化等扩展数据同样按 append-only 方式挂载：同一类型的同类扩展注册后不可替换。
属性和方法数据也按“注册后不替换”处理，外部视图只在该约束下承诺进程生命周期内稳定。

registry 使用边界：

- 用 registry：需要全局稳定查询、按名称或 GUID 查找、跨模块 extension 查询、父类型关系、对象工厂、
  反射属性列表、序列化属性配置或多态构造。
- 不用 registry：只需要当前 C++ 类型的布局、构造/析构、容器视图、wrapper 视图、枚举转换、
  静态 capability 或 `any` / `any_ref` 的局部值访问。
- 模块注册入口应接收已经存在的 `meta::type`，或者由调用方先显式 `registration::type_<T>("name")` 注册业务
  类型。序列化等模块不应为了挂载 extension 隐式创建业务类型注册记录。
- `meta::resolve<T>()` 返回 C++ 类型视图，未注册类型也可能有效；`registered()` 才表示
  registry 中已经存在稳定注册数据。
- `meta::find<T>()` 只查询已注册 C++ 类型；未注册类型返回空视图。
- `meta::find(name)` 和 `meta::find(type_guid)` 只查询已注册类型；未注册类型视图没有稳定 name/guid
  索引。
- registry 写入推荐集中在启动期、模块加载期或测试初始化阶段，之后调用 `meta::init_registry()` 做延迟注册和一致性校验；
  进入业务主循环和工作线程后只做只读查询。

推荐生命周期：

```cpp
void register_game_types() {
  caors::registration::type_<actor>("game.actor")
      .meta()
      .property("id", &actor::id)
      .commit();
}

int main() {
  register_game_types();
  caors::meta::init_registry();
  run_app();
}
```

也可以把局部注册函数挂到自动注册列表，由 `meta::init_registry()` 统一执行：

```cpp
CAORS_META_REGISTRATION(register_actor_meta) {
  caors::registration::type_<actor>("game.actor")
      .meta()
      .property("id", &actor::id)
      .commit();

  return caors::error{};
}
```

`meta::init_registry()` 会先执行所有通过 `CAORS_META_REGISTRATION` 或内部延迟注册入口收集到的注册函数，
再校验 meta 层索引和父类型关系。它不会把注册表切换为只读；后续注册仍允许，但调用方需要自行保证不会和运行期查询形成业务竞态。

`CAORS_META_REGISTRATION(name)` 只是便利入口，不建议作为唯一主流程。它依赖静态对象把函数加入列表：
静态库中未被链接进最终程序的翻译单元不会执行注册；注册函数执行顺序也不应被依赖。需要精确控制初始化、
测试隔离或插件加载时，优先使用显式 `register_all()`。

注册表维护几类索引：

- `type_id -> 内部 meta 类型记录`：运行期主索引，只保存身份类信息。
- `type_id -> 内部类型 descriptor`：布局、值操作、类型语义、内置 ops 和静态能力表。
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
- `errc::missing_type`：nested 类型或其它被引用类型未注册。

业务代码使用 registration builder 注册类型数据和模块扩展：

```cpp
struct object_factory_meta {
  bool constructible = false;
};

caors::registration::type_<actor>("game.actor")
    .meta()
    .property("id", &actor::id)
    .serialization()
    .field("id")
    .commit();
```

### meta::module

`meta::module` 是挂在 meta registry 上的全局命名空间，用来注册不依赖实例的全局属性和全局函数。
类的 `static` 成员变量和 `static` 成员函数也按同一套 global 入口注册到 module 上；它们不属于某个
`meta::type` 的成员属性或成员方法，调用时不需要传入 instance。

```cpp
int version = 1;

int add(int left, int right) {
  return left + right;
}

int offset = 10;

caors::meta::module("game")
    .property("version", &version)
    .function("add", &add)
    .function("add_offset", [offset](int value) { return value + offset; });

caors::meta::module_view game = caors::meta::find_module("game");
caors::meta::property version_property = game.find_property("version");
caors::meta::method_set add_function = game.find_method("add");

version_property.try_set_any(caors::any::from(2));

int left = 1;
int right = 2;
std::array<caors::argument, 2> args{
    caors::arg(left),
    caors::arg(right),
};
caors::meta::invoke_result sum = add_function.invoke_result_of(args);
```

module 和 class 的边界：

- `registration::type_<T>(name)`：注册 C++ 类型、成员属性、成员方法、父类型、factory 和类型级 capability。
- `meta::module(name)`：注册全局/静态属性和全局/静态函数，查询时通过 module 名称进入。
- `meta::module(name).function(name, callable)` 支持函数指针、静态成员函数指针、唯一
  `operator()` 签名的 lambda 和函数对象。带捕获 lambda 会由 module method 持有；泛型 lambda
  或重载 `operator()` 的函数对象需要先包装成唯一签名。
- module 上的 `property::get_any()`、`property::get_value()`、`property::get_ref()`、`property::try_set_any()`
  和 `property::try_set_value()` 不接收对象地址。
- module 上的 `method::invoke_result_of(args)` 和 `method_set::invoke_result_of(args)` 不接收对象地址；成员方法使用
  `invoke_result_of(object, args)`。
- module 上也可以使用 `property::get_value()` / `property::try_set_value(meta::value)` 和
  `method_set::invoke(span<const meta::value>)` 这类高层 `meta::value` 入口。
- 调用结果统一使用 `meta::invoke_result` 表达，可以区分调用失败、`void` 成功、拥有值返回和引用返回。

### 属性 schema

`meta::property_schema` 是属性自带的通用约束，直接保存在属性数据中。它适合表达配置、编辑器和工具层都能
理解的稳定语义，不用于描述具体序列化格式：

```cpp
struct texture_config {
  caors::string format;
  int priority = 1;
};

caors::registration::type_<texture_config>("game.texture_config")
    .meta()
    .property("format", &texture_config::format,
              caors::meta::registration::required(),
              caors::meta::registration::non_empty(),
              caors::meta::registration::one_of({"json", "binary"}))
    .property("priority", &texture_config::priority,
              caors::meta::registration::range(1, 100))
    .commit();

caors::meta::property format =
    caors::meta::find<texture_config>().find_property("format");
const caors::meta::property_schema& schema = format.schema();
```

当前内置属性 schema 包括：

属性 schema 的注册 token 位于 `meta::registration` 命名空间：

- `registration::required()`：属性必须有有效值。
- `registration::non_empty()`：属性不能为空，主要用于字符串、`string_name` 等文本属性。
- `registration::range_min(value)` / `registration::range_max(value)` /
  `registration::range(min, max)`：属性数值范围。
- `registration::one_of({...})`：属性允许值集合。

属性 schema 只放常见、跨模块可理解的约束。序列化属性名、属性 id、版本迁移、跳过属性等格式或持久化
策略应由 serialization 模块自己的 options 处理；不通用的工具扩展继续使用 metadata。

### metadata

`meta::metadata_map` 是轻量键值附加信息，值使用 `meta::variant` 保存，适合给类型、属性或方法补充工具层、
配置层和编辑器提示信息。类型级 metadata 直接保存在 meta 类型记录上：

```cpp
caors::registration::type_<actor>("game.actor")
    .metadata("category", "gameplay")
    .metadata("spawnable", true)
    .commit();

caors::meta::type type = caors::meta::find<actor>();
const caors::meta::metadata_value* category =
    type.find_metadata(caors::string_name{"category"});
```

metadata 推荐使用 RTTR 风格的绑定语法。`meta::registration::metadata(key, value)` 创建一条写入项：

- `registration::type_<T>("name").metadata(key, value)` 写入类型 metadata。
- `registration::type_<T>("name").meta().property(..., metadata(...))` 写入属性 metadata。
- `registration::type_<T>("name").meta().method(..., metadata(...))` 写入方法 metadata。
- 紧跟在 `registration::enum_<T>().value(...)` 后面的 `(...)` 写入枚举项 metadata。

普通类型属性和方法 metadata 通过 `registration::type_<T>("name").meta()` 分段注册：

```cpp
caors::registration::type_<actor>("game.actor")
    .metadata("category", "gameplay")
    .meta()
    .property("health", &actor::health,
              caors::meta::registration::metadata("display", "Health"),
              caors::meta::registration::metadata("range_min", 0),
              caors::meta::registration::metadata("range_max", 100))
    .method("damage", &actor::damage,
            caors::meta::registration::metadata("scriptable", true))
    .commit();
```

枚举项也使用同样的绑定风格。`registration::value(name, value)` 适合在 `operator()` 中批量注册，
`.value(name, value)` 适合链式追加并给枚举项挂 metadata：

```cpp
enum class actor_state {
  idle,
  running,
  hidden,
};

caors::registration::enum_<actor_state>("game.actor_state")
    (caors::registration::value("idle", actor_state::idle),
     caors::registration::value("running", actor_state::running))
    .value("hidden", actor_state::hidden)
    (caors::meta::registration::metadata("editor_hidden", true))
    .commit();

caors::meta::type state_type = caors::meta::find<actor_state>();
caors::meta::enum_capability values =
    state_type.get_capability<caors::meta::enum_capability>();
caors::meta::enum_value_view hidden = values.find_value("hidden");
```

枚举项按注册顺序保存，通过 `meta::enum_capability` 查询：
`values.value_count()`、`values.value_at(index)`、`values.find_value(name)` 或
`values.find_value(enum_value)`。重复枚举项名称返回 `errc::duplicate_name`；重复数值允许作为别名，
按数值查询时返回第一条注册项。`meta::type` 本身不暴露枚举专用 API，具体类型级行为统一走
`get_capability<Capability>()`。

metadata 不参与类型身份，不替代 `type_name` / `type_guid` / capability。需要决定运行期行为时，优先使用
明确的 capability、反射属性、属性 schema、序列化 options 或资源配置对象；metadata 只承载可选标签和
非标准工具提示。

## 高级扩展

### 自定义 capability

动态 capability 是高级扩展接口，不是普通 meta extension。公开写入入口挂在类型 builder 上：

```cpp
struct my_ops {
  int value = 0;
};

caors::registration::type_<actor>("game.actor")
    .capability(my_ops{42})
    .commit();
```

更接近 poly / concept table 的用法是把一组类型擦除函数注册为能力：

```cpp
struct matrix_ops {
  std::size_t (*rows)(const void*) = nullptr;
  std::size_t (*columns)(const void*) = nullptr;
  meta::any_ref (*element)(void*, std::size_t, std::size_t) = nullptr;
};

caors::registration::type_<matrix>("math.matrix")
    .capability(matrix_ops{
        &matrix_rows,
        &matrix_columns,
        &matrix_element,
    })
    .commit();

if (const matrix_ops* ops = caors::meta::find<matrix>().get_capability<matrix_ops>()) {
  // 调用方把对象地址交给 concept table，具体类型仍由注册时的 model 负责解释。
}
```

普通 capability、公开 facade 和 raw ops 的查询入口刻意分开：

- `meta::type::get_capability<T>()` / `has_capability<T>()`：面向普通运行期扩展能力。默认返回
  `const T*`，内置公开 facade 可特化为直接返回能力对象。
- `meta::type::get_capability<meta::sequence_capability>()`、`get_capability<objects::ptr_capability>()`
  和 `get_capability<serialization::codec_capability>()`：面向类型级公开 facade。
- `meta::type::get_raw_ops<T>()` / `has_raw_ops<T>()`：面向 `*_ops` 这类类型擦除函数表。调用点需要自行处理
  空指针、对象绑定和类型匹配。
- 实例级访问优先使用 capability 查询绑定后的 view，例如
  `any_ref::get_capability<meta::sequence_capability>()` 或
  `any_ref::get_capability<objects::ptr_capability>()`，避免业务代码直接传递 `void*`。

### raw ops

`meta::type::get_raw_ops<Ops>()` 返回类型擦除函数表指针，适合实现公开 capability facade、序列化或
编辑器适配器、调试工具，以及极少数需要绕过 facade 的性能敏感路径。它不是临时兼容入口，也不是废弃
API；它的约束是更接近 concept table，需要调用方明确绑定对象并保证类型匹配。

使用 raw ops 时应遵守以下边界：

- 返回值可能为空，调用前需要检查。
- raw ops 只描述类型能力，不绑定具体对象实例。
- 传入 `void*` / `const void*` 时，调用方必须保证对象实际类型和 ops 对应类型一致。
- 普通业务代码优先使用 `get_capability<Capability>()` 或
  `any_ref::get_capability<Capability>()`。
- 对外 API 如果需要暴露能力，优先包一层 `*_capability` 或 `*_view`，不要直接要求用户操作 raw ops。

典型模式是把 raw ops 封装进一个类型级 facade，再由 facade 绑定到具体实例；完整示例见下方用户自定义
capability 一节。

类型级 facade 可以再绑定到具体实例：

```cpp
std::vector<int> values{1, 2, 3};

auto sequence =
    caors::meta::resolve<std::vector<int>>().get_capability<caors::meta::sequence_capability>();
auto view = sequence.bind(caors::meta::any_ref::from(values));
```

用户自定义 capability 默认直接返回 `const Capability*`。如果希望对外也像内置能力一样返回 facade，
可以按能力形态分成两类：

- 纯类型级能力继承 `meta::type_capability_base<Ops>`，例如 factory、codec 或编辑器元信息查询。
- 绑定实例后使用的能力继承 `meta::capability_view_base<Ops>`，例如矩阵元素访问、容器视图或对象指针视图。

纯类型级能力只需要定义 `*_ops` 和 facade 类，不需要手写 `type_capability_traits`：

```cpp
struct matrix_factory_ops {
  matrix* (*create)() = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return create != nullptr;
  }
};

class matrix_factory : public caors::meta::type_capability_base<matrix_factory_ops> {
public:
  using base_type = caors::meta::type_capability_base<matrix_factory_ops>;
  using base_type::base_type;

  [[nodiscard]] matrix* create() const {
    return valid() ? ops_->create() : nullptr;
  }
};

auto factory = caors::meta::resolve<matrix>().get_capability<matrix_factory>();
matrix* value = factory.create();
```

实例级能力推荐同时提供类型级 facade 和实例级 view：定义 `*_ops` 函数表，让 view 继承
`meta::capability_view_base<Ops>`，再让 capability 继承 `meta::type_capability_base<Ops, View>`。
这样 `meta::type::get_capability<Capability>()` 返回类型级 facade，
`any_ref::get_capability<Capability>()` 返回绑定后的实例 view，并且不需要手写 `view_type` 和 `bind()`。

```cpp
struct matrix_ops {
  std::size_t (*rows)(const void*) = nullptr;
  std::size_t (*columns)(const void*) = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return rows != nullptr && columns != nullptr;
  }
};

class matrix_view : public caors::meta::capability_view_base<matrix_ops> {
public:
  using base_type = caors::meta::capability_view_base<matrix_ops>;
  using base_type::base_type;

  [[nodiscard]] std::size_t rows() const noexcept { return ops()->rows(data()); }
  [[nodiscard]] std::size_t columns() const noexcept { return ops()->columns(data()); }
};

class matrix_capability : public caors::meta::type_capability_base<matrix_ops, matrix_view> {
public:
  using base_type = caors::meta::type_capability_base<matrix_ops, matrix_view>;
  using base_type::base_type;
};
```

注册后可以直接从类型或实例查询：

```cpp
caors::registration::type_<matrix>("game.matrix")
    .capability(matrix_ops{&matrix_rows, &matrix_columns})
    .commit();

auto matrix_type = caors::meta::resolve<matrix>();
auto capability = matrix_type.get_capability<matrix_capability>();

matrix value{};
auto view = capability.bind(caors::meta::any_ref::from(value));
auto rows = view.rows();

auto direct_view = caors::meta::any_ref::from(value).get_capability<matrix_capability>();
```

这个 helper 解决的是重复编写 `type_capability_traits<Facade>`、`capability_view_traits<Capability>` 和基础存储的问题。
实例查询应传入 capability 类型；`any_ref::get_capability<View>()` 这类直接传 view 的旧查询方式不再保留。

推荐优先使用 Proxy 风格的 convention builder。它把“成员名”和“函数签名”分开描述，用户不需要手写
`ops`、`view`、调用参数名或 model：

```cpp
matrix make_matrix();

CAORS_META_DEF_MEM_DISPATCH(matrix_rows, rows);
CAORS_META_DEF_MEM_DISPATCH(matrix_columns, columns);
CAORS_META_DEF_MEM_DISPATCH(matrix_get, get);
CAORS_META_DEF_MEM_DISPATCH(matrix_set, set);
CAORS_META_DEF_TYPE_DISPATCH(matrix_create, create);
CAORS_META_DEF_FREE_TYPE_DISPATCH(matrix_make, make, &make_matrix);

using matrix_capability =
    caors::meta::capability_builder::add_convention<matrix_rows, std::size_t() noexcept>::
        add_convention<matrix_columns, std::size_t() noexcept>::
            add_convention<matrix_get, int(std::size_t, std::size_t)>::
                add_convention<matrix_set, void(std::size_t, std::size_t, int)>::
                    add_type_convention<matrix_create, matrix()>::
                        add_type_convention<matrix_make, matrix()>::build;

caors::registration::type_<matrix>("game.matrix")
    .capability<matrix_capability>()
    .commit();

auto type = caors::meta::resolve<matrix>();
auto capability = type.get_capability<matrix_capability>();
auto created = capability.create();
auto made = capability.make();

matrix value{};
auto view = caors::meta::any_ref::from(value)
    .get_capability<matrix_capability>();

view.set(1, 1, 42);
```

`CAORS_META_DEF_MEM_DISPATCH(dispatch_type, member_name)` 会定义一个 dispatch 类型。`add_convention`
使用该 dispatch 和 `R(Args...)` / `R(Args...) noexcept` 签名生成 capability。注册时如果目标
C++ 类型存在同名成员函数，`registration::type_<T>("name").capability<Capability>()` 会自动生成并注册
对应 ops；实例访问时 `any_ref::get_capability<Capability>()` 返回绑定后的 view。

`CAORS_META_DEF_TYPE_DISPATCH(dispatch_type, member_name)` 定义类型级 dispatch。`add_type_convention`
会把同名静态成员函数接到类型级 facade 上，适合 `create()`、固定形状、类型级配置查询这类不需要绑定实例的能力。
如果类型级函数来自全局函数或自由函数，使用
`CAORS_META_DEF_FREE_TYPE_DISPATCH(dispatch_type, facade_method, function_pointer)`，第三个参数通常写
`&function_name`，生成的 facade 方法名由第二个参数决定。底层也提供
`caors::meta::proxy::free_function_dispatch<&function_name>` 作为函数来源扩展点，但它本身不生成具名 facade
方法，普通用户优先使用宏。
当前版本不处理重载自动选择，遇到重载成员时仍需要后续更完整的 Proxy DSL 或显式 model。

简单成员函数映射统一使用
`CAORS_META_DEF_MEM_DISPATCH + capability_builder::add_convention`。需要稳定具名
`ops/view/capability` 类型、额外状态、错误策略或特殊生命周期时，应手写
`*_ops`、`*_view`、`*_capability` 和必要的 `*_ops_model<T>`，或直接注册一份 `ops`。

如果能力由 `static_capability<Capability, T>` 提供，也可以只声明能力类型：

```cpp
caors::registration::type_<actor>("game.actor")
    .capability<my_ops>()
    .commit();
```

### registry extension 查询

- `has_extension<T>(type_id)`：判断类型是否拥有扩展数据。
- `find_extension<T>(type_id)`：获取扩展数据副本。
- `find_extension_ptr<T>(type_id)`：获取扩展数据只读指针。扩展数据存放在对应 `type_data`
  内，地址在 append-only 注册表生命周期内保持稳定。
- `extension_count<T>()`：统计拥有指定扩展数据的类型数量。
- `extension_view<T...>()`：查询同时拥有指定扩展数据集合的类型。

extension 写入不作为 `meta::registry` 公共 API。普通业务代码通过
`registration::type_<T>(name)` 的 `.meta()`、`.serialization()`、`.object()` 或 `.capability()`
收集需要挂载的数据，并在 `commit()` 时一次性发布。底层测试或内部桥接如果必须直接写 extension，应使用
`meta::detail` 内部入口。

容器类型能力会在 `meta::type` 视图中按需提供，用于 `set_view` 和 `associative_view`
的 range-for 遍历。运行期遍历统一通过 iterator 完成；自定义容器如果需要暴露集合或关联容器遍历能力，应在对应
traits 中提供可适配的 iterator 操作。容器 view 只依赖 `meta::type` 能力，不再为了创建 view
把每一种容器组合额外挂载为 meta 扩展数据。

扩展数据挂载同样返回 `caors::error`，`domain()` 为 `error_domain::meta`：

- `errc::invalid_type`：传入无效 `type_id`。
- `errc::missing_type`：目标类型尚未注册。
- `errc::duplicate_component`：目标类型已经挂载同类扩展数据。

当前注册表仍是 append-only 语义，不支持扩展数据替换；如果后续支持热重载，应在替换 API 上显式处理旧数据生命周期和读者同步。

扩展数据的事实源是 `type_data` 内部的 extension map；`registry` 只维护按 extension key 到
`type_id` 列表的索引，用于 `extension_view<T...>()` 这类跨模块查询。普通代码不应把 extension
当成独立 ECS component 使用，它只是注册后类型数据的模块扩展槽。

## 线程安全边界

`meta::registry` 的注册和查询入口内部仍会加锁，避免注册表容器自身出现 data race。推荐在启动期
或模块加载期单线程通过 registration builder 完成注册，并调用 `meta::init_registry()` 做延迟注册和一致性校验。进入运行期后，业务主循环和工作线程应只做只读查询；
返回的 `meta::type` 和各模块视图是指向注册表内部数据的轻量只读视图，适合长期保存和并发读取。

当前不会保证运行期并发写注册表的业务一致性。脚本类型、异步加载、卸载和热重载应放到独立注册系统中处理。

## 与对象和反射的关系

全局 `meta::registry` 是共享元数据层：

- `objects::registry` 复用全局 meta 类型记录，再给同一个 `type_id` 挂载
  `objects::object_factory_capability`，并对外返回 `objects::type` 视图。
- 属性和方法直接保存在 meta 类型记录关联的数据中，并通过 `meta::type` / `meta::property` / `meta::method` 对外查询；
  `meta::type` / `meta::property` 是兼容视图。
- `serialization::registry` 复用全局 meta 类型记录查询序列化 extension；extension 由统一
  `registration::type_<T>(name).serialization()` 在注册提交时写入。

这样对象、反射和序列化的全局注册入口不需要重复处理重名、重复 GUID、不一致类型描述和父类型缺失。
跨模块查询可以通过 `meta::registry::extension_view<T...>()` 完成。属性和方法应通过 `meta::type`
返回稳定只读视图；序列化等公开扩展数据仍可通过
`meta::get_registry().extension_view<serialization::type_schema>()` 查询。

## 当前已实现能力

- `type_id`：运行期类型标识。
- `type_guid`：稳定类型标识。
- `string_name`：运行期 interned 名称，定义在 `caors/core/string_name.h`。
- `meta::resolve<T>()`：查询 C++ 类型视图；未注册类型也可返回有效视图。
- `meta::find<T>()`：查询已注册 C++ 类型；未注册类型返回空视图。
- `meta::type::registered()`：判断类型视图是否带有稳定注册数据。
- `meta::id_of<T>()`：查询 C++ 类型的运行期类型 id；类型不要求已经显式注册。
- `meta::id()`：根据已注册类型名查询运行期类型标识。
- `traits::type_name<T>`：基础 C++ 类型到 CAORS 类型名的默认映射。
- 内部 descriptor：布局、值操作、类型语义、内置 ops 和静态能力表，不属于公开 API。
- `traits::wrapper_traits<T>` / `traits::wrapper_access<T>`：wrapper 类型语义和内部值访问扩展点。
- `traits::sequence_traits<T>` / `traits::set_traits<T>` / `traits::associative_traits<T>` / `traits::pair_traits<T>` / `traits::tuple_traits<T>` / `traits::variant_traits<T>`：容器、二元组、tuple-like 和 variant-like 类型语义扩展点。
- `meta::type`：类型只读视图，注册数据可选。
- `registration::type_<T>("name")`：跨模块链式元类型注册入口；调用 `.commit()` 后发布类型数据。
- `registration::type_<T>("name")`：推荐的显式命名注册入口。
- `registration::enum_<T>("name")`：枚举类型注册入口；调用 `.commit()` 后发布枚举类型数据，注册后
  `meta::type::kind()` 为 `meta::type_kind::enumeration`。
- `registration::value(name, value)` / `registration::enum_<T>().value(name, value)`：注册枚举项；查询时通过
  `meta::enum_capability` 返回的 `meta::enum_value_view` 访问名称、整数值和 metadata。
- `registration::type_<T>(name).parent<Base>()`：保存 C++ 父类型关系和安全 upcast 回调。
- `registration::type_<T>(name).factory()` / `.factory(construct, destroy)`：给已注册类型注册运行期 factory，保存运行期值构造和销毁策略；查询时使用 `meta::type::get_capability<meta::factory_capability>()`，推荐通过 `factory_capability::construct()` 获取拥有生命周期的 `any`。
- `registration::type_<T>(name).constructor<Args...>()`：注册运行期参数构造策略；查询时使用
  `meta::type::get_capability<meta::constructor_capability>()`，普通调用优先走
  `meta::type::create(arguments)` 或 `constructor_capability::construct(arguments)`。
- `meta::registry`：全局 meta 类型注册、按 id/name/guid 或 C++ 类型查询、继承关系判断和 extension 元数据挂载。
- `meta::value`：高频动态运行期值，内联保存常见标量、`string_name` 和 `meta::object*`，
  通过 `any_ref` 表达非拥有 C++ 引用，通过 `any` 兜底表达拥有的精确 C++ 类型值；它面向脚本、
  编辑器、消息和动态调用，可持久化数据树仍使用 `meta::variant`。`property::get_value()`、
  `property::try_set_value(meta::value)`、`method::invoke()`、`method_set::invoke()` 和 `instance::call()`
  是当前高层动态调用入口；常见标量、字符串和对象指针会优先走 tagged value fast path。需要精确 C++
  拷贝或引用参数时使用 `property::get_any()` / `instance::get_any()` / `instance::call_any()`。
- `meta::registry::extension_view<T...>`：按扩展数据集合查询类型。
- `meta::run_registrations()`：执行已收集的延迟元数据注册函数。
- `meta::init_registry()`：执行延迟元数据注册函数并校验注册表；不会阻止后续注册。
- `meta::validate_registry()`：只校验注册表，不执行延迟注册函数。
- `CAORS_META_REGISTRATION(name)`：定义一个延迟注册函数，函数体需要返回 `caors::error`。
- `meta::get_registry()`：进程内全局元数据注册表。

相关测试位于 `tests/meta/` 和 `tests/core/`，测试组织规则见 [tests/README.md](../tests/README.md)。
