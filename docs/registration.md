<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Registration

本文档说明 CAORS 当前推荐的类型注册顺序和初始化策略。这里的注册只面向 C++ 类型；脚本或动态类型后续应使用独立注册系统，再通过上层 resolver 和 C++ 运行期数据交互。

## 推荐入口

一个类型如果要同时参与 meta、对象和序列化，推荐使用 `caors::registration::type_<T>(name)`
作为跨模块聚合入口。它把类型名、属性、方法、序列化 schema 和对象能力集中到同一条注册链路，
避免业务代码直接关心各模块 extension 的挂载顺序。

示例：

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

当前聚合入口使用 registry pending model：builder 在注册阶段收集 meta 属性/方法、序列化 schema、
对象 factory capability 和其他类型能力，`commit()` 时一次性发布完整 `type_data`。发布后的
`type_data` 不会再通过 `registration::type_<T>(name)` 修改；重复提交同一个类型会返回
`errc::duplicate_id`，并保留已有注册数据。

业务类型注册统一使用 `registration::type_<T>(name)`。object 和 serialization 不再提供独立公共
builder；当 `T` 继承自 `objects::object` 时，`commit()` 会自动写入对象能力。序列化 schema 通过 `.serialization()` 写入，同一次
`commit()` 发布完整 `type_data`。`meta::init_registry()` 仍负责执行延迟注册函数并校验注册表。

字段声明类型不要求全部完成 `registration::type_<T>("name")` 注册。基础标量、字符串、容器、指针、
wrapper、pair、tuple 和 variant 可以通过内部 descriptor 参与反射和序列化。只有需要稳定名称、
GUID、对象工厂、反射根类型、多态身份或按名称查询的业务类型，才应显式注册到 meta。
字段注册不会为字段类型隐式创建 meta 类型，序列化 codec 注册也不会隐式创建 meta 类型。

公共 API、registry 和 capability 的边界如下：

- `meta::type` 是统一类型视图。`meta::resolve<T>()` 可返回未注册 C++ 类型视图，`meta::find<T>()`
  只返回已注册类型。
- registry 只保存显式注册后的稳定元数据和模块 extension，例如名称、GUID、父类型、factory、字段、方法和序列化 extension。
- descriptor static capability 用于类型固有能力，例如对象指针、对象句柄和 `codec_traits<T>` 生成的
  codec 函数表，通过 `meta::type::get_capability<C>()` 查询，不需要单独注册 codec。
- dynamic capability 用于用户或插件扩展能力，通过 `registration::type_<T>("name").capability(ops).commit()`
  挂到注册类型数据；
  它不是 registry extension，也不替代业务 meta 类型注册。

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

该方式适合模块局部自注册，但有两个限制：

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
- `errc::invalid_field`：反射字段描述无效、字段名冲突，或字段类型无法生成有效 descriptor。

公共业务代码使用 `registration::type_<T>(name)` 完成跨模块注册。用户代码不应直接调用各模块的
`detail` 命名空间函数。字段和方法直接通过 meta 注册能力写入，不再保留独立 reflection 注册入口；
serialization 和 object 的注册能力也收敛到统一 registration builder。
