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
- 用注册表内部 meta 类型记录保存类型名、稳定 GUID 和父类型。
- 用内部 type descriptor 保存 C++ 类型布局、默认构造、复制、析构、赋值和运行期视图能力。
- 用内部 type descriptor 保存类型形态、通用类型关系和具体访问函数表。
- 用可选 factory capability 保存资源感知构造策略，供普通类多态反序列化等需要运行期创建值的路径使用。
- 用 `meta::type` 作为对外只读类型视图，避免直接暴露注册表内部数据。
- 用全局 `meta::registry` 集中处理 meta 类型注册、查找和继承关系判断。
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
caors::meta::class_<player>("game.player");
caors::type_id id = caors::meta::id_of<player>();
```

类型注册后，也可以通过名称查询当前进程内的运行期 id：

```cpp
caors::meta::class_<player>("game.player");
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
caors::meta::class_<player>("game.player")
    .guid(caors::type_guid{*guid});
```

需要注意：

- 未显式传入时，`meta::type::guid()` 为 nil。
- `type_guid` 不应由 `type_id`、`string_name` 的运行期 entry 或哈希值派生。
- 修改类型名不应自动修改 `type_guid`，否则旧数据无法稳定识别类型。

### meta::type

meta 类型记录由全局注册表内部保存，普通代码不直接构造或修改。对外查询统一返回
`meta::type`，它提供以下只读信息：

- `id`
- `name`
- `guid`
- `size`
- `alignment`
- `parent_id`

`meta::type` 以 C++ 类型 descriptor 为基础：`meta::resolve<T>()` 和 `meta::id_of<T>()`
可以为未显式注册的 C++ 类型返回 descriptor 视图和运行期 `type_id`。
`size()` / `alignment()`、默认构造、复制、析构和赋值函数都来自 descriptor。
`registered()` 表示该类型是否已经通过 `meta::class_<T>("name")` 写入稳定注册数据；只有已注册类型
才有稳定注册名、GUID、父类型和可挂载组件。
`meta::find<T>()` 只查询已经注册的 C++ 类型；未注册类型即使能解析 descriptor，也会返回空视图。
`meta::class_<T>()` 链式 builder 会原地更新全局注册表中已经保存的类型数据；已有 `meta::type`
视图只引用这份稳定数据，不会因为后续 `.guid()` 或 `.parent()` 调用而失效。

`meta::type` 是对外查询返回的只读视图：

```cpp
caors::meta::type type = caors::meta::resolve<player>();

if (type) {
  caors::type_id id = type.id();
  std::string_view name = type.name().view();
}
```

`meta::type` 不拥有数据，内部引用静态 descriptor；类型已注册时还会引用全局 append-only 注册表中的
meta 类型记录。需要查询类型信息时使用 `meta::type`。

类型查询按用途分为两条路径：

- `meta::resolve<T>()`：解析 C++ 类型视图，优先返回已注册 `meta::type`；没有注册时退回
  descriptor-only 视图。反射、序列化、value 和对象模块在只需要布局、类型形态、内置 ops、
  static capability 或 codec 时统一使用这个入口。
- `meta::find<T>()` / `meta::find(name/guid)`：查询已注册类型。需要稳定名称、GUID、父类型、
  factory 或模块组件索引时使用这个入口；未注册 C++ 类型返回空视图。

### traits::type_name<T>

`traits::type_name<T>` 是 C++ 类型到 CAORS 类型名的默认映射。内置基础类型已经提供默认名称，例如：

```cpp
caors::traits::type_name<int>::value;   // "int"
caors::traits::type_name<float>::value; // "float"
```

用户类型可以通过特化 `traits::type_name<T>` 提供 descriptor 默认显示名，也可以直接通过
`meta::class_<T>("name")` 显式注册。默认显示名只用于字段、`any` 和容器视图的调试/展示回退，
不等价于 meta 注册；模板注册入口会把 C++ 类型和注册后的 `meta::type` 绑定起来，后续可以用
C++ 类型查询：

```cpp
caors::meta::class_<actor>("game.actor");

auto type = caors::meta::find<actor>();
```

也可以使用 RTTR 风格入口：

```cpp
caors::meta::class_<actor>("game.actor")
    .guid(caors::type_guid{*guid});
```

如果类型需要表达 C++ 继承关系，推荐使用 typed parent 注册：

```cpp
caors::meta::class_<player>("game.player")
    .parent<actor>();
```

`.parent<actor>()` 不只保存 `parent_id`，还会保存 `player* -> actor*` 的安全 upcast 回调。普通类
多态序列化读入 `std::unique_ptr<actor>` / `std::shared_ptr<actor>` 时需要这份回调把新建的
`player` 对象交给声明基类 wrapper。`.parent(type_id)` 仍可用于只需要关系查询的场景，但不会提供
typed upcast 能力。

如果类型需要在运行期由序列化层创建，可以注册资源感知 factory：

```cpp
void* construct_player(caors::memory_resource* resource);
void destroy_player(caors::memory_resource* resource, void* value) noexcept;

caors::meta::class_<player>("game.player")
    .guid(player_guid)
    .parent<actor>()
    .factory(&construct_player, &destroy_player);
```

`.factory()` 无参数重载使用 `memory::make<T>()` / `memory::destroy<T>()` 注册默认构造策略，要求 `T`
可默认构造。带函数指针的重载用于不可默认构造类型，`destroy` 必须匹配 `construct` 的分配策略。
应用层需要创建运行期值时优先使用 `runtime::type::create()`，它会先通过
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

caors::meta::class_<actor>("game.actor")
    .constructor<int>();
```

`constructor<Args...>()` 使用 `std::span<const meta::any_ref>` 参数列表做精确匹配：参数数量必须一致，
参数类型必须和 `Args...` 去 cv/ref 后一致；非 const 左值引用参数要求传入可写 `any_ref`。
`runtime::type::create(arguments)` 会在 factory 之后查询 constructor，构造成功后同样由 `any`
保存匹配的内部销毁策略。也可以使用 `constructor_capability::construct(arguments, resource)` 直接获得
`meta::any`，或使用 `constructor_capability::construct_owned(arguments, resource)` 获得可延迟转移的
`meta::owned_value`。当前版本只支持一个 constructor 能力，右值引用参数和 move-only by-value 参数暂不支持；构造函数重载、
默认参数和隐式类型转换后续再扩展。

`caors::meta::resolve<T>()` 是 C++ 类型到 descriptor 视图的入口；类型未显式注册时仍可能返回有效
`meta::type`，但 `registered()` 为 false。`caors::meta::find<T>()` 是 C++ 类型到已注册类型的入口，
未注册时返回空视图。`caors::meta::find(id/name/guid)` 是运行期查询入口：按 `name` / `guid`
只能找到已注册类型，按 `id` 可以找到已缓存的 descriptor 视图或已注册类型。
普通业务类型仍应通过 `caors::meta::class_<T>("name")` 显式注册，再把得到的 `meta::type` 交给
对象、反射或序列化模块挂载组件。

### 普通入口：facade 和 view

普通业务代码优先使用公开 facade，而不是直接操作内部 descriptor 或 raw ops：

- 类型查询使用 `meta::resolve<T>()` 或 `meta::find<T>()`。
- 类型级能力使用 `meta::type::get_capability<Capability>()`，例如
  `get_capability<meta::sequence_capability>()`。
- 实例级能力同样优先传入 capability 类型，例如
  `any_ref.get_capability<meta::sequence_capability>()` 返回 `sequence_view`，
  `any_ref.get_capability<objects::ptr_capability>()` 返回 `objects::ptr_view`。
- 跨 meta / reflection / object 的高层访问优先使用 `runtime::type` 和 `runtime::instance`。
- 公开 capability facade 不直接暴露 `void*`。需要返回未知类型值时使用 `meta::any`，
  需要引用已有对象时使用 `meta::any_ref` 或绑定后的 `*_view`；`void*` 只用于底层
  `*_ops`、model 和 `meta::detail` helper。

只有实现新的 capability facade、调试工具、序列化桥接或性能敏感适配层时，才直接使用
`meta::type::get_raw_ops<Ops>()`。

### 分层边界：descriptor、registry、capability

当前 meta 层按四个边界分工：

- 内部 descriptor 是编译期 C++ 类型描述。它描述某个 `T` 的 `type_id`、默认显示名、布局、构造 /
  析构 / 复制 / 赋值、类型语义，以及 CAORS 内置运行期访问能力。它不表示类型已经完成业务注册。
- `meta::registry` 是注册后元数据和模块组件索引。只有显式 `meta::class_<T>("name")` 注册后的类型，
  才拥有稳定名称、GUID、父类型关系、对象组件、反射组件和序列化组件等全局可查询数据。
- descriptor static capability 用于类型固有能力。能力由模块在编译期按 `T` 生成，例如
  `objects::ptr<T>` 访问函数、对象 handle 函数表和 `codec_traits<T>` 生成的 codec 函数表。
  它们随 descriptor 一起查询，不要求类型先注册到 registry。
- runtime capability 用于用户或插件扩展能力。它挂在 `type_id + capability_key` 的运行期索引上，
  适合矩阵、图、脚本桥接、编辑器扩展等 CAORS 不内置的 concept table。公开写入入口是
  `meta::class_<T>().capability(ops)`，由 CAORS 持有 `ops` 生命周期。
  `meta::class_<T>().factory()` 也是该机制的高级封装，会把构造策略注册为内部
  `meta::factory_ops`，公开查询时返回 `meta::factory_capability`。

默认查询顺序是：`meta::type::get_capability<C>()` 先查 descriptor static capability，再回退到
runtime capability。普通调用方应使用这个统一入口，不直接关心能力来自 descriptor 还是运行期索引。
如果只剩 `type_id`，应先通过 `meta::find(id)` 找回 `meta::type` 视图；只有确实需要直接查运行期扩展时，
才使用 `meta::capabilities::find<C>(id)`。

设计约束：

- descriptor 只保存 C++ 类型事实和可由 `T` 推导的能力，不保存反射字段、序列化字段配置或对象注册数据。
- registry 保存注册后事实和模块组件，不复制 descriptor 的布局、子类型和内置 ops 数据。
- static capability 不应承担用户插件协议；它适合编译期可见、进程内静态生命周期的模块能力。
- runtime capability 不是 meta 组件，也不替代业务类型注册；需要稳定名称、GUID 或跨模块组件查询时仍要注册
  meta 类型。

### 内部结构：descriptor 和 ops

内部 descriptor 是 C++ 类型的内部能力描述。注册表按 `type_id`
保存指向静态 descriptor 的指针，不把 descriptor 当作普通 generic component 复制存储。descriptor
由布局、值操作、类型语义、内置 ops 和静态能力表几部分组成；未注册结构类型也可以通过 `any`、
`any_ref` 或反射字段携带的静态 descriptor 访问同一套能力。
descriptor 位于 `meta::detail`，不属于公开 API；普通用户应通过 `meta::type`、`meta::resolve<T>()`
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
`meta/type_registration.h` 保存公开注册输入描述 `type_registration_data`；
`meta/detail/type_descriptor_name.h` 负责 descriptor 默认显示名生成；
`meta/detail/type_descriptor_model.h` 负责 descriptor holder、语义填充和构建实现；
`meta/detail/builtin_ops_model.h` 作为内置 ops 聚合入口，只负责引入
`meta/detail/builtin/*.h` 并选择当前类型的内置 ops；`builtin/enum_ops.h`、
`builtin/indirect_ops.h`、`builtin/sequence_ops.h`、`builtin/set_ops.h`、
`builtin/associative_ops.h`、`builtin/pair_ops.h`、`builtin/tuple_ops.h` 和
`builtin/variant_ops.h` 分别负责对应能力的 `*_ops` 函数表、函数指针类型、具名 static 回调和
`*_ops_model<T>` 生成；`builtin/container_iterators.h` 提供通用 `container_iterator_ops`、
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
- `*_data` 保留给注册表组件、配置数据或纯元数据结构，不用于命名一组运行期分发函数。
- `*_ops` 不是 C++ ABI 层面的虚函数表，不依赖继承；它只是 CAORS 内部统一的函数表命名。
- `capability_info` 保存 capability 的进程内 key、可选稳定名称、可选稳定 GUID 和版本号。`capability_key`
  仍是当前进程内查询 key，稳定元信息只用于诊断、工具链和未来插件协议协商。

`type_semantics` 描述以下语义：

- `kind`：值、枚举、裸指针、wrapper、顺序容器、集合容器、关联容器、二元组、tuple-like 或 variant-like 类型。
- `traits::wrapper_semantics`：wrapper 或裸指针的处理语义，例如内联值或不支持。
- `indirect`：该类型是否具备一层间接值语义，例如裸指针或 wrapper。
- `nullable`：运行期值是否可能为空。

具体访问函数表由 `builtin_ops` 或模块 ops 持有，用来描述枚举转换、容器、指针、wrapper、
对象引用、pair、tuple 和 variant 访问能力。当前 descriptor 内置 ops 由 CAORS 定义，并且同一类型只会持有一个内置 ops；查询入口仍是内部
`descriptor_ops<T>()`：

- `meta::indirect_ops`：裸指针或 wrapper 的内部值访问、持值检测、重置和确保持值函数。
- `meta::enum_ops`：枚举类型和整数之间的转换函数。
- `meta::sequence_ops` / `set_ops` / `associative_ops` / `pair_ops` / `tuple_ops` / `variant_ops`：容器、二元组、tuple-like 和 variant-like 的运行期访问函数。

模块静态能力通过 `meta::capabilities::module_static_capability_list<Module, T>` 声明。
`meta::capabilities::static_capability_modules<T>` 是模块 tag 聚合入口，默认包含 object、serialization
和 custom 三个模块 tag；外部模块可以特化 `extension_static_capability_modules<T>` 追加自己的 module tag，
再由 `static_capability_list<T>` 合并所有模块贡献的 `type_list<Capability...>`。descriptor 生成时会把这些能力整理成一张静态
`capability_entry` 表。公开查询优先使用 `meta::type::get_capability<T>()` 或
`meta::type::has_capability<T>()`；该入口会先查询 descriptor 静态能力，再回退到运行期
capability 索引。内部仍保留 `descriptor_static_capability<T>()` 给 descriptor 层实现使用。
对象指针、对象句柄和 serialization codec 都通过 `meta::type` 的统一入口查询公开 facade；
内部 `ptr_ops`、`handle_ops` 和 `codec_ops` 只作为实现细节保留。

动态扩展能力通过 `meta::class_<T>().capability(ops)`、`meta::class_<T>().capability<Capability>()`
或内部稳定指针入口挂接到独立运行期索引；`meta::capabilities` 命名空间只保留静态能力提供者、
运行期查询和诊断工具。`capability(ops)` 会把一份用户自定义 concept table 复制或移动到运行期索引中，
由 CAORS 持有其生命周期；查询仍通过 `meta::type::get_capability<Capability>()` 或
`meta::capabilities::find<Capability>(type_id)` 查询。默认用户 capability 返回只读指针；内置公开
facade 可以通过 `type_capability_traits` 返回轻量对象。拿得到 C++ 类型 `T` 时，也可以通过
`meta::capabilities::get<Capability, T>()` 直接查询静态能力。serialization codec 会作为静态能力挂到
descriptor 上，并通过 `serialization::codec_capability` facade 暴露；当运行期只剩 `type_id` 时，应先通过
meta registry 找回携带 descriptor 的 `meta::type`。

capability 和 registry 的边界：

- descriptor 静态能力适合“编译期已知 C++ 类型即可生成”的只读函数表，例如对象指针访问函数、
  句柄访问函数和 `codec_traits<T>` 生成的 codec 操作表。
- `meta::capabilities` 运行期索引适合“只剩 `type_id`，但仍需要查询某种用户外挂运行期能力”的场景；
  内置 object / serialization 能力优先通过 type descriptor 静态能力查询。
- 公开写入运行期索引时优先使用 `meta::class_<T>().capability(ops)`，让 CAORS 持有用户自定义
  concept table。内部模块如果已经拥有进程生命周期的静态函数表，可以使用指针注册入口。
  如果能力来自 `static_capability<Capability, T>`，可以使用 `meta::class_<T>().capability<Capability>()`。
- 普通调用方优先使用 `meta::type::get_capability<T>()`。该入口统一处理 descriptor 静态能力和运行期
  索引 fallback，避免业务代码分别理解两套存储位置。
- capability 不是 meta 组件。它不拥有类型注册数据，也不表示该 C++ 类型已经注册到
  `meta::registry`。需要稳定名称、GUID、父类型、反射字段、序列化字段配置或对象工厂时，仍应显式注册
  meta 类型和对应模块组件。
- 当前 `capability_key` 是进程内模板静态地址，只适合当前进程内查询，不承诺跨动态库 ABI、跨进程或
  存档稳定。需要插件协议、脚本模块或工具链发现能力时，应特化 `capability_traits<T>` 提供稳定
  name/guid/version，并在协议层处理冲突和版本兼容。
- 多模块静态能力不应都挤到 `custom_static_capability_module`。外部模块应定义自己的 module tag，特化
  `module_static_capability_list<Module, T>` 贡献能力，再通过 `extension_static_capability_modules<T>`
  把 module tag 加入指定 C++ 类型的静态能力聚合列表。

统一 `value_type` / `key_type` 不再作为 descriptor 顶层字段保存。普通值和枚举直接使用
descriptor 自身的 `type_id`；裸指针、wrapper、容器、pair、tuple 和 variant 的子类型信息由对应
访问函数表保存，并通过 `field`、`sequence_view`、`pair_view` 等公开视图查询。

类型形态和通用类型关系只从 descriptor 查询，不再把语义数据、枚举 ops、容器 ops 或间接访问 ops 作为
独立 meta 组件挂载到注册表。反射字段会保存声明类型的 `type_id` 和 descriptor，因此
`std::vector<T>`、`std::optional<T>`、`std::tuple<...>` 这类结构类型不要求
自身注册为 meta 类型。未注册类型不会拥有 GUID、反射字段、对象工厂或序列化组件；如果 descriptor
能通过 traits 推导默认显示名，`type_name()` 仍可返回该名称。无默认显示名的未注册业务类型名称为空，
但仍可通过字段、`any` / `any_ref` 或嵌套容器元素携带的 descriptor 使用布局、值操作和运行期视图能力。

meta 不会因为存在 `traits::type_name<T>` 就在运行期隐式创建业务类型。需要稳定名称、GUID、对象工厂、
反射字段列表或多态序列化身份的 C++ 类型仍应通过 `meta::class_<T>("name")` 显式注册。

### meta::registry

`meta::registry` 是进程级全局类型元数据注册表。它直接管理 meta 类型记录，并允许在同一个 `type_id` 上挂载对象、反射、序列化等元数据组件。普通代码不需要创建 registry 对象：

```cpp
caors::error result = caors::meta::class_<actor>("game.actor").result();
```

注册表只面向 C++ 类型。脚本或动态类型不应直接写入当前注册表，后续应放到独立系统，再由上层 resolver
统一查询。注册表是 append-only：meta 类型注册后不可清理或撤销，重复注册相同类型会返回重复结果。
当前组件同样按 append-only 方式挂载：同一类型的同类组件注册后不可替换。反射和序列化这类组件
数据按“注册后不替换”处理，外部视图只在该约束下承诺进程生命周期内稳定。

registry 使用边界：

- 用 registry：需要全局稳定查询、按名称或 GUID 查找、跨模块组件查询、父类型关系、对象工厂、
  反射字段列表、序列化字段配置或多态构造。
- 不用 registry：只需要当前 C++ 类型的布局、构造/析构、容器视图、wrapper 视图、枚举转换、
  descriptor 静态 capability 或 `any` / `any_ref` 的局部值访问。
- 模块注册入口应接收已经存在的 `meta::type`，或者由调用方先显式 `meta::class_<T>("name")` 注册业务
  类型。反射和序列化模块不应为了挂载组件隐式创建业务类型注册记录。
- `meta::resolve<T>()` 返回 descriptor-backed 类型视图，未注册类型也可能有效；`registered()` 才表示
  registry 中已经存在稳定注册数据。
- `meta::find<T>()` 只查询已注册 C++ 类型；未注册类型返回空视图。
- `meta::find(name)` 和 `meta::find(type_guid)` 只查询已注册类型；未注册 descriptor 没有稳定 name/guid
  索引。
- registry 写入推荐集中在启动期、模块加载期或测试初始化阶段，之后调用 `meta::init_registry()` 做延迟注册和一致性校验；
  进入业务主循环和工作线程后只做只读查询。

推荐生命周期：

```cpp
void register_game_types() {
  caors::meta::class_<actor>("game.actor");
  caors::reflection::class_<actor>()
      .field("id", &actor::id)
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
  caors::meta::class_<actor>("game.actor");

  return caors::reflection::class_<actor>()
      .field("id", &actor::id)
      .commit();
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

使用方式是先注册业务 meta 类型，再给该类型挂载组件：

```cpp
struct object_factory_meta {
  bool constructible = false;
};

caors::meta::class_<actor>("game.actor");

caors::type_id actor_id = caors::meta::find<actor>().id();
caors::meta::get_registry().emplace_component<object_factory_meta>(actor_id, true);
```

## 高级扩展

### 自定义 capability

运行期 capability 是高级扩展接口，不是普通 meta 组件。公开写入入口挂在类型 builder 上：

```cpp
struct my_ops {
  int value = 0;
};

caors::meta::class_<actor>("game.actor")
    .capability(my_ops{42});
```

更接近 poly / concept table 的用法是把一组类型擦除函数注册为能力：

```cpp
struct matrix_ops {
  std::size_t (*rows)(const void*) = nullptr;
  std::size_t (*columns)(const void*) = nullptr;
  meta::any_ref (*element)(void*, std::size_t, std::size_t) = nullptr;
};

caors::meta::class_<matrix>("math.matrix")
    .capability(matrix_ops{
        &matrix_rows,
        &matrix_columns,
        &matrix_element,
    });

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
caors::meta::class_<matrix>()
    .capability(matrix_ops{&matrix_rows, &matrix_columns});

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

caors::meta::class_<matrix>()
    .capability<matrix_capability>();

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
C++ 类型存在同名成员函数，`meta::class_<T>().capability<Capability>()` 会自动生成并注册
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
caors::meta::class_<actor>()
    .capability<my_ops>();
```

### registry 组件 API

- `emplace_component<T>(type_id, args...)`：给已注册类型挂载组件。
- `has_component<T>(type_id)`：判断类型是否拥有组件。
- `find_component<T>(type_id)`：获取组件副本。
- `find_component_ptr<T>(type_id)`：获取组件只读指针。组件由注册表以 `unique_ptr` 保存，地址在 append-only
  注册表生命周期内保持稳定。
- `ensure_component<T>(type_id, args...)`：按需组件创建 helper，组件已存在时返回现有指针，不存在时创建并返回
  新组件指针。
- `component_count<T>()`：统计拥有指定组件的类型数量。
- `component_view<T...>()`：查询同时拥有指定组件集合的类型。

容器 descriptor 会在具体容器支持 `begin()` / `end()` 时额外保存类型擦除 iterator 函数，用于
`set_view` 和 `associative_view` 的 range-for 遍历。运行期遍历统一通过 iterator 完成；自定义容器如果
需要暴露集合或关联容器遍历能力，应在对应 traits 中提供可适配的 iterator 操作。容器 view 只依赖
descriptor，不再为了创建 view 把每一种容器组合额外挂载为 meta 组件。

组件挂载同样返回 `caors::error`，`domain()` 为 `error_domain::meta`：

- `errc::invalid_type`：传入无效 `type_id`。
- `errc::missing_type`：目标类型尚未注册。
- `errc::duplicate_component`：目标类型已经挂载同类组件。

当前注册表仍是 append-only 语义，不支持组件替换；如果后续支持热重载，应在替换 API 上显式处理旧组件生命周期和读者同步。

## 线程安全边界

`meta::registry` 的注册、查询和组件挂载入口内部仍会加锁，避免注册表容器自身出现 data race。推荐在启动期
或模块加载期单线程完成注册，并调用 `meta::init_registry()` 做延迟注册和一致性校验。进入运行期后，业务主循环和工作线程应只做只读查询；
返回的 `meta::type` 和各模块视图是指向注册表内部数据的轻量只读视图，适合长期保存和并发读取。

当前不会保证运行期并发写注册表的业务一致性。脚本类型、异步加载、卸载和热重载应放到独立注册系统中处理。

## 与对象和反射的关系

全局 `meta::registry` 是共享元数据层：

- `objects::registry` 复用全局 meta 类型记录，再给同一个 `type_id` 挂载
  `objects::object_factory_capability`，并对外返回 `objects::type` 视图。
- 全局反射注册表复用 meta 类型记录，再挂载内部反射组件，并通过 `reflection::type` / `reflection::field` 对外查询。
- 后续 `serialization::registry` 可以挂载序列化相关组件。

这样对象、反射和序列化的全局注册入口不需要重复处理重名、重复 GUID、不一致类型描述和父类型缺失。
跨模块查询可以通过 `meta::registry::component_view<T...>()` 完成。反射这类内部组件应通过对应模块 API
返回稳定只读视图；序列化等公开组件仍可通过
`meta::get_registry().component_view<serialization::type_schema>()` 查询。

## 当前已实现能力

- `type_id`：运行期类型标识。
- `type_guid`：稳定类型标识。
- `string_name`：运行期 interned 名称，定义在 `caors/core/string_name.h`。
- `meta::resolve<T>()`：查询 C++ 类型 descriptor 视图；未注册类型也可返回有效视图。
- `meta::find<T>()`：查询已注册 C++ 类型；未注册类型返回空视图。
- `meta::type::registered()`：判断 descriptor 视图是否带有稳定注册数据。
- `meta::id_of<T>()`：查询 C++ 类型的运行期类型 id；类型不要求已经显式注册。
- `meta::id()`：根据已注册类型名查询运行期类型标识。
- `traits::type_name<T>`：基础 C++ 类型到 CAORS 类型名的默认映射。
- 内部 descriptor：布局、值操作、类型语义、内置 ops 和静态能力表，不属于公开 API。
- `traits::wrapper_traits<T>` / `traits::wrapper_access<T>`：wrapper 类型语义和内部值访问扩展点。
- `traits::sequence_traits<T>` / `traits::set_traits<T>` / `traits::associative_traits<T>` / `traits::pair_traits<T>` / `traits::tuple_traits<T>` / `traits::variant_traits<T>`：容器、二元组、tuple-like 和 variant-like 类型语义扩展点。
- `meta::type`：descriptor-backed 类型只读视图，注册数据可选。
- `meta::class_<T>()`：RTTR 风格的链式元类型注册入口；默认不注册类型，需要继续调用 `.type("name")`。
- `meta::class_<T>("name")`：推荐的显式命名注册入口。
- `meta::enum_<T>()`：RTTR 风格的枚举类型注册入口；默认不注册类型，需要继续调用 `.type("name")`。
- `meta::enum_<T>("name")`：显式注册枚举类型，注册后 `meta::type::kind()` 为
  `meta::type_kind::enumeration`。
- `meta::class_<T>().parent<Base>()`：保存 C++ 父类型关系和安全 upcast 回调。
- `meta::class_<T>().factory()` / `.factory(construct, destroy)`：注册运行期 factory，保存运行期值构造和销毁策略；查询时使用 `meta::type::get_capability<meta::factory_capability>()`，推荐通过 `factory_capability::construct()` 获取拥有生命周期的 `any`。
- `meta::class_<T>().constructor<Args...>()`：注册运行期参数构造策略；查询时使用
  `meta::type::get_capability<meta::constructor_capability>()`，普通调用优先走
  `runtime::type::create(arguments)` 或 `constructor_capability::construct(arguments)`。
- `meta::registry`：全局 meta 类型注册、按 id/name/guid 或 C++ 类型查询、继承关系判断和组件式元数据挂载。
- `meta::registry::component_view<T...>`：按组件集合查询类型。
- `meta::run_registrations()`：执行已收集的延迟元数据注册函数。
- `meta::init_registry()`：执行延迟元数据注册函数并校验注册表；不会阻止后续注册。
- `meta::validate_registry()`：只校验注册表，不执行延迟注册函数。
- `CAORS_META_REGISTRATION(name)`：定义一个延迟注册函数，函数体需要返回 `caors::error`。
- `meta::get_registry()`：进程内全局元数据注册表。

相关测试位于 `tests/meta/` 和 `tests/core/`，测试组织规则见 [tests/README.md](../tests/README.md)。
