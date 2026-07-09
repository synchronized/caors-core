<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Registration

本文档说明 CAORS 当前推荐的类型注册顺序和初始化策略。这里的注册只面向 C++ 类型；脚本或动态类型后续应使用独立注册系统，再通过上层 resolver 和 C++ 运行期数据交互。

## 推荐顺序

一个类型如果要参与对象、反射和序列化，推荐按以下顺序注册：

1. 注册需要稳定名称、GUID、父类型或 factory 的业务 meta 类型。
2. 如有对象能力，注册 object 组件。
3. 注册字段反射。
4. 注册序列化组件。
5. 可选：挂接用户或插件的 runtime capability。
6. 可选：在启动阶段最后调用 `meta::freeze_registry()`。

示例：

```cpp
void register_actor() {
  caors::meta::type actor_type =
      caors::meta::type_<actor>("game.actor")
          .guid(caors::type_guid{actor_guid})
          .get();

  caors::reflection::class_<actor>(actor_type)
      .field("id", &actor::id)
      .commit();

  caors::serialization::class_(actor_type)
      .field("id")
      .commit();
}
```

字段声明类型不要求全部完成 `meta::type_<T>("name")` 注册。基础标量、字符串、容器、指针、
wrapper、pair、tuple 和 variant 可以通过内部 descriptor 参与反射和序列化。只有需要稳定名称、
GUID、对象工厂、反射根类型、多态身份或按名称查询的业务类型，才应显式注册到 meta。
反射 builder 不会为字段类型隐式创建 meta 类型，序列化 codec 注册也不会隐式创建 meta 类型。

descriptor、registry 和 capability 的边界如下：

- descriptor 是编译期 C++ 类型描述和内置能力入口，未注册类型也可能拥有有效 descriptor。
- registry 只保存显式注册后的稳定元数据和模块组件，例如名称、GUID、父类型、factory、反射和序列化组件。
- descriptor static capability 用于类型固有能力，例如对象指针、对象句柄和 `codec_traits<T>` 生成的
  codec 函数表，不需要单独注册 codec。
- runtime capability 用于用户或插件扩展能力，通过 `meta::type_<T>().capability(ops)` 挂到运行期索引；
  它不是 registry 组件，也不替代业务 meta 类型注册。

## 显式初始化

最可控的方式是在应用启动阶段显式调用注册函数：

```cpp
int main() {
  register_core_types();
  register_game_types();
  caors::meta::freeze_registry();
  run_app();
}
```

这种方式适合游戏、工具和测试程序：注册顺序清楚，错误可以直接返回或记录，也不依赖静态初始化顺序。

## 延迟注册函数

`CAORS_META_REGISTRATION(name)` 可以把注册函数加入全局队列，`meta::freeze_registry()` 会先执行这些函数，再冻结注册表：

```cpp
CAORS_META_REGISTRATION(register_actor) {
  caors::meta::type_<actor>("game.actor");
  return caors::error{};
}
```

该方式适合模块局部自注册，但有两个限制：

- 静态库中未被最终程序引用的翻译单元可能不会参与链接，因此注册函数也不会进入队列。
- 多个翻译单元之间的注册函数执行顺序不应被依赖。

如果类型之间有明确依赖，优先使用显式 `register_all()`。

## 冻结注册表

`meta::freeze_registry()` 的语义是：

- 执行已收集的延迟注册函数。
- 校验 meta 层基础索引和父类型关系。
- 将全局注册表切换为只读。

冻结后，meta 类型注册和组件挂载都会返回 `errc::frozen`。查询接口仍可继续使用。是否冻结由应用决定；库内部不强制自动冻结。

## 注册失败处理

注册 API 返回 `caors::error`：

- `errc::duplicate_id`：同一个运行期类型 id 已经注册。测试和初始化代码可按需视为成功。
- `errc::duplicate_name` / `errc::duplicate_guid`：名称或稳定 GUID 冲突，应修正类型声明。
- `errc::missing_type`：给不存在的 `type_id` 挂载组件。
- `errc::missing_parent`：父类型尚未注册。
- `errc::invalid_field`：反射字段描述无效、字段名冲突，或字段类型无法生成有效 descriptor。
- `errc::frozen`：注册表已冻结。

公共测试和用户代码应使用 `meta::type_<T>()`、`objects::class_<T>()`、`reflection::class_<T>()`、`serialization::class_()`、`serialization::register_type()` 等公开入口，不直接调用各模块的 `detail` 命名空间函数。
