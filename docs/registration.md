<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Registration

本文档说明 CAORS 当前推荐的类型注册顺序和初始化策略。这里的注册只面向 C++ 类型；脚本或动态类型后续应使用独立注册系统，再通过上层 resolver 和 C++ 运行期数据交互。

## 公共 API

一个类型如果要同时参与 meta、对象和序列化，推荐使用 `caors::registration::type_<T>(name)`
作为跨模块聚合入口。它把类型名、属性、方法、序列化 schema 和对象能力集中到同一条注册链路，
避免业务代码直接关心各模块 extension 的挂载顺序。

当前公共注册入口如下：

| 入口 | 用途 |
| --- | --- |
| `registration::type_<T>(name)` | 注册普通 C++ 类型，并在同一条 builder 链路中收集 meta、serialization、object 和 capability 数据。 |
| `registration::type_<T>()` | 注册 `meta::object` 派生类型时读取 `T::object_type_name`，并自动写入对象能力。 |
| `registration::enum_<T>(name)` | 注册枚举类型和枚举项。 |
| `registration::value(name, value)` | 枚举项批量注册输入，用在 `registration::enum_<T>(name)(...)`。 |
| `registration::batch(ctx)` / `registration::batch()` | 顺序收集多个 type/enum builder，并在 `commit()` 时批量提交。 |
| `CAORS_META_REGISTRATION(name)` | 可选的延迟注册函数收集入口，最终仍应调用统一 registration builder。 |

普通类型示例：

```cpp
void register_actor() {
  caors::registration::type_<actor>("game.actor")
      .meta()
      .property("id", &actor::id)
      .method("rename", &actor::rename)
      .serialization()
      .field("id")
      .commit();
}
```

枚举类型示例：

```cpp
enum class actor_state {
  idle,
  running,
  hidden,
};

void register_actor_state() {
  caors::registration::enum_<actor_state>("game.actor_state")
      (caors::registration::value("idle", actor_state::idle),
       caors::registration::value("running", actor_state::running))
      .value("hidden", actor_state::hidden)
      .commit();
}
```

批量注册示例：

```cpp
caors::error register_game_types(caors::meta::context& ctx) {
  caors::registration::batch_builder batch = caors::registration::batch(ctx);

  batch.type_<actor>("game.actor")
      .meta()
      .property("id", &actor::id)
      .serialization()
      .field("id");

  batch.enum_<actor_state>("game.actor_state")
      (caors::registration::value("idle", actor_state::idle),
       caors::registration::value("running", actor_state::running));

  return batch.commit();
}
```

## 内部实现边界

当前聚合入口使用 registry pending model：builder 在注册阶段收集 meta 属性/方法、序列化 schema、
对象 factory capability 和其他类型能力，`commit()` 时一次性发布完整 `type_record`。发布后的
`type_record` 不会再通过 `registration::type_<T>(name)` 或 `registration::enum_<T>(name)` 修改；重复提交
同一个类型会返回 `errc::duplicate_id`，并保留已有注册数据。

业务类型注册统一使用 `registration::type_<T>(name)`。object 和 serialization 不再提供独立公共
builder；当 `T` 继承自 `meta::object` 时，`commit()` 会自动写入对象能力。序列化 schema 通过
`.serialization()` 写入，同一次 `commit()` 发布完整 `type_record`。显式序列化字段会在提交阶段绑定到
已注册属性，未显式填写字段时会按当前属性列表生成默认 schema。`meta::init_registry()` 仍负责执行
延迟注册函数并校验注册表。

枚举注册统一使用 `registration::enum_<T>(name)`。枚举项、枚举项 metadata、类型 GUID、类型 metadata
和 capability 都在 builder pending 阶段收集；只有 `.commit()` 成功后，`meta::find<T>()` 才能查到该枚举类型。
如果枚举项名称重复或类型名为空，builder 会保存错误状态，`commit()` 不会发布半成品类型。

当前 registry 使用 append-only 生命周期。普通注册不提供 replace、update 或覆盖语义；一旦
`commit()` 成功发布 `type_record`，后续重复类型、重复名称或重复 GUID 注册都会返回错误，已经发布的
属性、方法、nested 关系、序列化 schema、对象能力、metadata 和 capability 保持不变。已经获取的
`meta::type` view 也继续指向旧的稳定数据。

批量注册第一版不提供事务回滚。`registration::batch_builder::commit()` 会按追加顺序提交每个 builder，
遇到第一个错误立即停止并通过 `diagnostic()` 返回失败项的 index、kind、type id、name 和 error。已经成功提交的
前置项会保留在 registry 中，失败项之后的注册项不会提交。批量注册也不提供隐式 replace；重复类型、重复名称或
重复 GUID 仍按普通 builder 规则返回错误。

批量注册错误处理示例：

```cpp
caors::registration::batch_builder batch = caors::registration::batch();
batch.type_<actor>("game.actor");
batch.enum_<actor_state>("game.actor_state")
    .value("idle", actor_state::idle)
    .value("idle", actor_state::running);

caors::error status = batch.commit();
if (status) {
  caors::string message = batch.diagnostic_message();
  const caors::registration::batch_diagnostic* diagnostic = batch.diagnostic();
  // diagnostic->kind / index / id / name / status 可用于日志、断言或工具链展示。
}
```

`diagnostic_message()` 返回类似
`registration batch failed at item 1: enum game.actor_state duplicate name` 的可读文本。
需要结构化处理时优先读取 `diagnostic()`；只需要日志时使用 `diagnostic_message()`。

## Registry 策略方向

当前默认策略等价于 `append_only_registry`：启动期或模块加载期集中注册，发布后只读查询，view 可以保持
稳定指向已发布数据。这是普通游戏运行期、工具启动和测试环境的推荐模型。

未来如果需要插件热加载、脚本动态类型或编辑器热替换，应引入独立 registry 策略，而不是让当前默认
registry 隐式变成可变结构。可评估的策略包括：

- `mutable_registry`：允许显式替换或删除类型数据，但需要 generation、失效检测和更严格的锁策略；旧 view
  不能继续假定永久稳定。
- `snapshot_registry`：在后台构建新快照，提交时原子切换读取快照；旧 view 的生命周期绑定到旧快照，
  适合读多写少的工具或热重载场景。
- 插件 registry overlay：应用主 registry 保持 append-only，插件注册写入 overlay；查询时先查 overlay
  再查主 registry，插件卸载时整体丢弃 overlay，避免修改主 registry。

`dry_run` 暂不作为当前 API。真正有意义的 dry-run 需要 registry 提供“只校验不提交”的策略能力，
否则无法可靠判断名称、GUID、父类型、extension 和 capability 冲突是否会在提交时发生变化。

字段声明类型不要求全部完成 `registration::type_<T>("name")` 注册。基础标量、字符串、容器、指针、
wrapper、pair、tuple 和 variant 可以通过 `meta::type` 能力参与属性访问和序列化。只有需要稳定名称、
GUID、对象工厂、meta 根类型、多态身份或按名称查询的业务类型，才应显式注册到 meta。
字段注册不会为字段类型隐式创建 meta 类型，序列化 codec 注册也不会隐式创建 meta 类型。

nested 类型关系只记录 owner 内部使用的短名和目标 `type_id`。目标类型必须先通过
`registration::type_<Nested>(name)` 或 `registration::enum_<Nested>(name)` 独立注册；owner 再通过
`.meta().nested<Nested>("short_name")` 或 `.meta().nested("short_name", nested_type)` 记录关系。
注册后可以通过 `meta::type::find_nested()`、`nested_at()` 和 `nested_name_at()` 查询。

公共 API、registry 和 capability 的边界如下：

- `meta::type` 是统一类型视图。`meta::resolve<T>()` 可返回未注册 C++ 类型视图，`meta::find<T>()`
  只返回已注册类型。
- registry 只保存显式注册后的稳定元数据和模块 extension，例如名称、GUID、父类型、nested 关系、
  factory、字段、方法和序列化 extension。
- static capability 用于类型固有能力，例如对象指针、对象句柄和 `codec_traits<T>` 生成的
  codec 函数表，通过 `meta::type::get_capability<C>()` 查询，不需要单独注册 codec。
- dynamic capability 用于用户或插件扩展能力，通过 `registration::type_<T>("name").meta().capability(ops).commit()`
  挂到注册类型数据；
  它不是 registry extension，也不替代业务 meta 类型注册。
- 公开业务注册不应直接调用 `meta::registry::register_type()`、`meta::detail::register_capability*()`、
  `meta::detail::*` 或各模块内部 extension 写入函数。这些入口只服务 builder、内部兼容桥接或低层测试。
- `assets::asset_manager::register_*()`、`resources::database::register_*()` 等运行期资源工厂注册不是
  meta 类型注册 builder，不受本节统一类型注册入口约束。

## 显式初始化

最可控的方式是在应用启动阶段显式调用注册函数：

```cpp
int main() {
  register_core_types();
  register_game_types();
  caors::meta::init_registry();
  run_app();
}
```

这种方式适合游戏、工具和测试程序：注册顺序清楚，错误可以直接返回或记录，也不依赖静态初始化顺序。

## 延迟注册函数

`CAORS_META_REGISTRATION(name)` 可以把注册函数加入全局队列，`meta::init_registry()` 会执行这些函数并校验注册表：

```cpp
CAORS_META_REGISTRATION(register_actor) {
  return caors::registration::type_<actor>("game.actor").commit();
}
```

该方式适合模块局部自注册，但有这些限制：

- 静态库中未被最终程序引用的翻译单元可能不会参与链接，因此注册函数也不会进入队列。
- 多个翻译单元之间的注册函数执行顺序不应被依赖。
- 如果 `init_registry()` 后又加载了带注册函数的模块，新注册函数会立即执行；调用方仍需保证此时没有其他线程并发读取依赖中的半成品类型。

如果类型之间有明确依赖，优先使用显式 `register_all()`。

## 初始化和校验

`meta::init_registry()` 的语义是：

- 执行已收集的延迟注册函数。
- 校验 meta 层基础索引和父类型关系。
- 不冻结注册表，也不阻止后续注册。

`meta::validate_registry()` 只做一致性校验，不执行延迟注册函数。CAORS 不再提供冻结注册表的公开入口。

注册表内部会对单次注册和查询加锁，但 CAORS 不保证“运行期一边读一边改注册表”的业务一致性。推荐由应用在启动期、
模块加载期或测试初始化阶段完成注册；进入主循环后只做查询。如果需要脚本类型、插件热卸载或热重载注册，后续应使用独立的动态注册系统。

## 注册失败处理

注册 API 返回 `caors::error`：

- `errc::duplicate_id`：同一个运行期类型 id 已经注册。测试和初始化代码可按需视为成功。
- `errc::duplicate_name` / `errc::duplicate_guid`：名称或稳定 GUID 冲突，应修正类型声明。
- `errc::missing_type`：给不存在的 `type_id` 挂载 extension。
- `errc::missing_parent`：父类型尚未注册。
- `errc::invalid_field`：meta 属性描述无效、属性名冲突，或属性类型无法生成有效 `meta::type` 能力。

公共业务代码使用 `registration::type_<T>(name)` 完成跨模块注册。用户代码不应直接调用各模块的
`detail` 命名空间函数。属性和方法直接通过 meta 注册能力写入，不再保留独立 reflection 注册入口；
serialization 和 object 的注册能力也收敛到统一 registration builder。
