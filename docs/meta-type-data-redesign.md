<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta 静态类型数据与注册数据重构计划

## 文档状态

- 状态：讨论草案
- 目标阶段：后续 meta 核心重构
- 当前结论：先稳定需求和数据模型，不在本轮直接修改公共 API

## 背景

当前 `meta::resolve<T>()` 会先查询当前 `meta::registry`。类型已经显式注册时返回 registry
中的 `type_data`；类型尚未注册时返回绑定到静态 `type_descriptor<T>` 的轻量视图。

当前关系如下：

```text
未注册类型
meta::type
├─ registry_   -> 当前 registry
├─ data_       -> nullptr
└─ descriptor_ -> 静态 type_descriptor<T>

已注册类型
meta::type
├─ registry_   -> 当前 registry
├─ data_       -> registry 中的 type_data
└─ descriptor  -> 通过 type_data 间接获得
```

`type_descriptor<T>` 同时收集布局、值生命周期、内置 ops 和静态 capability。其中一部分内容依赖
当前翻译单元可见的 traits 或模块特化。若把这种结果作为跨翻译单元唯一的 inline 实体，不同实例化点
可能产生不一致定义，形成 ODR 风险；若改为翻译单元本地缓存，又会让同一个 C++ 类型出现多份 descriptor。

本计划希望恢复最初参考 EnTT 的模型：每个 C++ 类型有一份静态类型节点；查询时优先返回当前 context
中的注册节点，不存在时回退到静态节点。

## 外部方案调研

### EnTT

EnTT 的 `internal::resolve<Type>(context)` 先创建每个 `Type` 唯一的函数内静态
`meta_type_node`，随后按该节点的 `type_info` 查询 context。context 中存在注册节点时返回注册节点，
否则返回静态节点。

其核心行为可概括为：

```cpp
template <typename Type>
const meta_type_node& resolve(const meta_context& context) noexcept {
    static const meta_type_node node = setup_node_for<Type>();
    const auto* registered = try_resolve(context, *node.info);
    return registered == nullptr ? node : *registered;
}
```

EnTT 的静态节点保存类型固有信息和编译期可推导行为；注册 context 保存名称、成员和其他显式反射信息。
容器、pointer-like 等模板能力要求相应 traits 在实例化位置可见。

参考：

- [EnTT resolve.hpp](https://github.com/skypjack/entt/blob/master/src/entt/meta/resolve.hpp)
- [EnTT node.hpp](https://github.com/skypjack/entt/blob/master/src/entt/meta/node.hpp)
- [EnTT meta 文档](https://skypjack.github.io/entt/md_docs_2md_2meta.html)

### RTTR

RTTR 对外提供唯一的轻量 `type` 对象，`type::get<T>()` 可获得 C++ 类型信息；名称、构造函数、属性和方法
等运行期反射信息通过显式 registration 补充。按注册名查询只有在注册执行后才有效。

RTTR 更偏向“类型身份始终可得、丰富反射信息显式注册”，适合作为 CAORS 划分静态固有数据和 registry
注册数据的参考，但其全局注册模型不能直接替代 CAORS 的多 registry/context 设计。

参考：

- [RTTR type](https://www.rttr.org/doc/master/classrttr_1_1type.html)
- [RTTR registration::class_](https://www.rttr.org/doc/master/classrttr_1_1registration_1_1class__.html)

### Boost.Describe

Boost.Describe 将枚举、基类和成员描述保留为编译期类型信息，由使用方在模板实例化位置查询，不提供
统一运行期 registry。它说明了 traits/描述信息必须在使用前可见，但不直接解决 CAORS 的动态注册、
插件和多 context 需求。

参考：

- [Boost.Describe 文档](https://www.boost.org/doc/libs/latest/libs/describe/doc/html/describe.html)

## 需求

### 功能需求

1. `meta::resolve<T>(ctx)` 必须先查询 `ctx` 所属 registry。
2. registry 已存在显式注册类型时，返回注册后的类型数据。
3. registry 不存在该类型时，返回每个 C++ 类型唯一的静态类型数据。
4. `meta::find<T>(ctx)` 仍只返回显式注册类型。
5. 未注册类型继续支持当前已经公开和测试覆盖的基础能力：
   - 运行期 `type_id`；
   - 默认类型名；
   - 稳定类型 GUID（类型声明提供时）；
   - 布局和值生命周期操作；
   - scalar、enum、container、wrapper、pair、tuple、variant 等内置 ops。
6. 显式注册后，后续 `resolve<T>(ctx)` 必须返回 registry 中的注册数据。
7. 同一个 C++ 类型可以在不同 registry 中拥有独立注册信息，但共享同一份静态固有数据。
8. `meta::type` 不得依赖 descriptor 地址判断类型身份。

### 身份需求

静态类型数据应保存：

- 进程内 `type_id`；
- 默认或声明的类型名；
- 声明的稳定 `type_guid`；
- C++ RTTI/type info 映射；
- 类型布局和基础语义。

名称和 GUID 的来源统一通过 traits 适配。object 子类的 `static_type_name`、`static_type_guid`
应由 object 模块适配到统一 traits，避免 meta 核心直接依赖 object。

显式注册值与静态声明值的合并规则：

| 静态声明 | 显式输入 | 处理 |
|---|---|---|
| 无 | 无 | 保持为空 |
| 有 | 无 | 使用静态声明 |
| 无 | 有 | 使用显式输入 |
| 有且相同 | 有 | 正常提交 |
| 有但不同 | 有 | validate 返回类型身份冲突 |

### 一致性需求

1. 静态类型数据的定义不得依赖可选模块头文件的包含顺序。
2. 同一类型在不同翻译单元得到的静态数据必须满足 ODR。
3. serialization、object、资源等可选模块 capability 不应悄悄改变 meta 核心静态节点的定义。
4. 显式 specialization 必须在首次实例化相关模板前可见；测试也必须遵守这一规则。
5. registry 发布后继续保持 append-only，已有 `meta::type` 视图的指针稳定性不得下降。

### 性能需求

1. 未注册 `resolve<T>()` 不进行堆分配。
2. 已注册 `resolve<T>()` 保持一次 registry 哈希查询。
3. 静态类型节点按 `T` 惰性初始化一次。
4. `meta::type` 保持轻量、可复制，目标大小不超过两个或三个指针。
5. 不因本次重构为每个静态类型构造属性、方法、extension 等动态容器。

## 推荐数据模型

### 静态固有数据

建议将当前 `type_descriptor` 演进为明确命名的静态核心节点：

```cpp
struct static_type_data {
  type_id id{};
  const std::type_info* cpp_type = nullptr;
  std::string_view declared_name{};
  type_guid declared_guid{};
  type_semantics semantics{};
  value_storage_ops value{};
  builtin_ops builtin{};
};
```

每个 C++ 类型只有一份：

```cpp
template <typename T>
[[nodiscard]] const static_type_data& static_type_data_of() noexcept {
  static const static_type_data value = make_static_type_data<T>();
  return value;
}
```

`make_static_type_data<T>()` 只能读取所有实例化点都必须一致的 traits。可选模块 capability 不进入该节点。

### Registry 注册数据

现有 `type_data` 继续表示 registry 拥有的完整注册记录，但引用静态核心：

```cpp
class type_data {
public:
  [[nodiscard]] const static_type_data& static_data() const noexcept;

private:
  const static_type_data* static_data_ = nullptr;
  string_name name_{};
  type_guid guid_{};
  type_id parent_id_{};
  // properties、methods、metadata、capabilities、extensions...
};
```

registry 记录只在显式 `commit()` 后存在，不需要引入“resolved 但未注册”的 registry 条目。

### 公共视图

`meta::type` 保存当前实际节点的二选一来源：

```cpp
class type {
  const registry* registry_ = nullptr;
  const static_type_data* static_data_ = nullptr;
  const type_data* registered_data_ = nullptr;
};
```

也可以进一步压缩为一个带来源标记的内部 node 指针，但第一阶段优先保持实现清晰。

查询规则：

- ID、默认名、声明 GUID、布局、类型形态和值操作：静态数据始终可用；
- 注册名称、注册 GUID、父类、属性、方法、metadata、动态 capability：仅注册数据可用；
- 注册数据存在时，名称和 GUID 已在 validate 阶段与静态声明完成一致性校验。

### Resolve 流程

```cpp
template <typename T>
[[nodiscard]] type resolve(context& ctx) {
  const static_type_data& fallback = static_type_data_of<T>();

  if (const type_data* registered =
          ctx.get_registry().find_type_data(fallback.id)) {
    return type_access::make(ctx.get_registry(), fallback, registered);
  }

  return type_access::make(ctx.get_registry(), fallback, nullptr);
}
```

```text
resolve<T>(ctx)
├─ 获取唯一 static_type_data<T>
├─ 查询 ctx.registry
├─ 已注册：返回 static + registered 视图
└─ 未注册：返回只有 static 的视图
```

## Capability 边界

### 进入静态核心的能力

第一阶段保留当前未注册类型测试所需的内置能力：

- value storage；
- scalar；
- enum；
- raw pointer；
- sequence、set、associative；
- pair、tuple、variant；
- 项目认可的核心 wrapper traits。

这些 traits 的特化必须与类型声明一起放在公共头文件中，并在任何 `resolve<T>()` 前可见。

### 不进入静态核心的能力

以下能力属于可选模块，不应影响 `static_type_data<T>` 的 ODR：

- serialization codec；
- object 模块 capability；
- resources/assets capability；
- 用户或插件动态 capability；
- registry extension。

编译期已知 `T` 的便利 API 可以直接使用模块 traits。例如 `codec_of<T>()` 可直接由
`codec_traits<T>` 构造 facade。需要通过擦除后的 `meta::type` 查询模块能力时，应要求该能力已经通过
对应注册流程提交到 registry。

这会调整当前“未注册类型可通过 `meta::type` 查询静态 codec/module capability”的行为，必须在实施前
单独确认兼容策略。

## 与现有行为的差异

当前测试明确覆盖以下 descriptor-only 行为：

- 未注册类型可默认构造和销毁；
- 未注册容器和 wrapper 可创建运行期 view；
- 未注册类型可通过 `meta::type` 查询静态 capability；
- 未注册自定义 codec 可通过擦除后的 `meta::type` 序列化。

推荐方案保留前两项。后两项需要二选一：

### 方案 A：模块能力必须注册

- 静态核心完全不依赖模块；
- 运行期擦除查询结果稳定；
- codec 等能力在 registration pending record 中收集并提交；
- 需要修改 descriptor static capability 和未注册 codec 测试。

这是推荐方案。

### 方案 B：模块提供独立静态侧表

- 每个模块维护自己的 `T -> ops` 静态入口；
- `codec_of<T>()` 等编译期入口无需注册；
- `meta::type` 无法仅凭 type-erased `type_id` 安全发现所有未注册模块能力；
- 若增加全局侧表，又会引入初始化、生命周期和多 registry 边界问题。

该方案只能作为兼容层，不建议成为最终事实源。

## Pending、Validate、Commit

显式注册继续采用三阶段模型：

```text
type_<T>() / enum_<T>()
        │
        ▼
pending_type_record
├─ static_data = &static_type_data_of<T>()
├─ name / guid / parent
├─ properties / methods
├─ module capabilities
└─ extensions
        │
        ▼
validate
├─ 静态身份与显式身份一致
├─ type id / name / guid 无冲突
├─ parent 已存在
├─ 成员和 capability 无重复
└─ 模块数据完整
        │
        ▼
commit
└─ 一次性发布 registry type_data
```

不建议让后续 `resolve<T>()` 增量修改已发布 `type_data`。若某个模块能力是注册类型的一部分，应在同一次
pending record 中收集完成，保持现有 append-only 和事务语义。

## 分阶段开发计划

### 阶段 0：需求确认

- 确认未注册 codec/static capability 是否继续作为 `meta::type` 契约；
- 确认静态名称和 GUID 与显式注册冲突时返回错误；
- 确认 `type_descriptor` 最终改名还是由 `static_type_data` 替代；
- 确认 object 静态身份的统一 traits 入口。

完成标准：本文件中的开放问题有明确结论。

### 阶段 1：建立静态类型核心

- 新增 `static_type_data` 和 `static_type_data_of<T>()`；
- 迁移 type id、RTTI、默认名称、声明 GUID、布局和值生命周期；
- 迁移稳定的 builtin ops；
- 增加跨翻译单元地址唯一和字段一致性测试；
- 保持现有 `meta::type` 公共行为不变。

完成标准：同一个 `T` 在多个翻译单元获得同一静态节点地址，且全量测试通过。

### 阶段 2：收敛 `meta::type`

- 让 `resolve<T>()` 查询 registry 后回退到 `static_type_data<T>`；
- 让已注册 `type_data` 引用静态核心；
- 移除 `meta::type` 中重复的 descriptor 数据来源；
- 统一 `type_access` 和 builtin ops 查询；
- 保持 `find<T>()` 只返回显式注册类型。

完成标准：未注册与已注册查询只有一条静态核心路径，注册后下一次 `resolve<T>()` 返回注册视图。

### 阶段 3：身份合并与 object 适配

- 增加统一 `type_identity_traits<T>`；
- 接入 `traits::type_name<T>`；
- 由 object 模块适配 `static_type_name` 和 `static_type_guid`；
- 在 pending validate 中实现名称/GUID 一致性检查；
- 增加多 registry 和冲突测试。

完成标准：注册前后类型身份一致，不同 registry 共享静态核心但注册记录独立。

### 阶段 4：模块 capability 解耦

- 从静态核心移除 serialization/object 等可选模块 capability；
- registration builder 在 pending 阶段收集模块能力；
- 明确 `codec_of<T>()` 与 `find_codec(meta::type)` 的不同前置条件；
- 修改或增加未注册 codec、静态 capability 和跨模块测试；
- 删除临时的翻译单元本地 descriptor workaround。

完成标准：不同模块头文件包含顺序不再改变静态类型节点定义。

### 阶段 5：清理和性能验证

- 删除旧 `type_descriptor`、双指针回退和不再使用的 helper；
- 更新 meta、registration、serialization、object 和 traits 文档；
- 对 `resolve<T>()`、`find<T>()`、属性查询和序列化调度运行 benchmark；
- 在 MSVC、Clang、GCC 的 Debug/Release 下执行完整测试。

完成标准：没有兼容残留入口，性能不低于当前实现，所有平台测试通过。

## 测试计划

必须新增或保留以下覆盖：

1. 两个翻译单元解析同一个基础类型，静态节点地址相同；
2. 两个翻译单元解析同一个容器/wrapper 类型，语义和 ops 相同；
3. 未注册 `resolve<T>()` 有效且 `registered()` 为 false；
4. 显式提交后下一次 `resolve<T>()` 返回注册数据；
5. `find<T>()` 在提交前为空、提交后有效；
6. 同一个静态类型在两个 registry 中注册信息独立；
7. 静态 name/GUID 与显式值冲突时提交失败且 registry 不发生部分写入；
8. 可选模块头文件包含顺序不改变静态节点；
9. codec/module capability 按最终确认的契约测试注册前后行为；
10. 已取得的 `meta::type` 视图生命周期和 registry append-only 约束明确覆盖。

## 风险

- `type_data` 拆分会影响 meta、object、serialization、resources 多个模块，必须分阶段提交；
- 修改未注册 codec 行为可能影响用户代码，需要在 beta 前明确兼容策略；
- 静态名称若保存 `std::string_view`，traits 必须保证底层字符数据具有静态生命周期；
- 静态 GUID 与显式 GUID 的冲突策略会把过去允许的覆盖变为错误；
- 跨 DLL 时模板静态节点是否唯一取决于导出边界，需在插件稳定协议专项中继续处理；
- 不能用静态节点地址替代 `type_id` 作为跨模块或持久化身份。

## 开放问题

1. 未注册自定义 codec 是否仍必须支持通过 `find_codec(meta::resolve<T>())` 查询？
2. 用户自定义 static capability 是否必须支持未注册 type-erased 查询？
3. `type_descriptor` 是直接改名为 `static_type_data`，还是先保留兼容内部别名？
4. 静态 declared name 是否直接通过 `meta::type::name()` 暴露，还是新增 `declared_name()`？
5. object 的静态名称/GUID 缺失时，是否允许 registration builder 补充？
6. 跨 DLL/插件场景是否要求静态节点地址统一，还是仅要求 `type_id/type_guid` 统一？

在这些问题确认前，不进入大规模代码迁移。
