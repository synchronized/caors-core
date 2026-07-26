<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# DLL 与插件使用规范

本文说明 CAORS 的动态库和插件边界。当前插件模型以“宿主和插件共享同一份 CAORS runtime”
为前提。更完整的跨编译模块 API 风险和传参边界见 [module-boundary.md](module-boundary.md)。

## 推荐拓扑

```text
host.exe
├─ caors.dll
├─ plugin_a.dll ── caors.dll
└─ plugin_b.dll ── caors.dll
```

宿主和所有插件必须动态链接同一个 `caors.dll` 实例。不要让多个插件分别静态链接 CAORS，
也不要让宿主静态链接 CAORS 后再加载动态链接 CAORS 的插件。这种拓扑会产生多个 runtime state，
CAORS 不保证不同 state 之间的 registry、默认 context、memory resource、`type_id`、
capability key 或 extension key 相同。

## 编译与 ABI 要求

参与同一进程的宿主和插件必须统一：

- 编译器及其主版本；
- C++ 标准库和运行库 ABI；
- C++ 语言模式、异常和 RTTI 配置；
- 影响公共类型布局的编译选项和 CAORS 配置；
- 公共 C++ 类型、traits、adapter、名称和 GUID 声明。

跨模块共享的类型应定义在共同头文件中。匿名 namespace 类型、lambda 和局部类型只视为模块
私有类型，不应作为插件协议类型。

## 类型身份

- `type_id` 是同一 runtime 内的临时身份，由 canonical C++ 类型名称驻留后顺序分配；
- `type_id` 不跨进程、版本或多份 runtime 稳定，不能写入持久化格式；
- `type_guid` 用于存档、资源和跨版本协议，必须由公共声明显式提供且保持不变；
- 注册显示名称用于查询和诊断，不参与 C++ 类型身份判等；
- `std::type_index` 只用于普通 C++ 多态对象的动态类型 fallback，不是主身份来源。

`capability_key` 和 meta extension key 同样由共享 runtime 按 canonical C++ 类型名称驻留。
它们只适合当前进程内快速查询；需要稳定插件协议时，还应为 capability 提供名称、GUID 和版本。

## 注册与卸载

插件应提供显式注册入口，在主循环开始前完成 `registration::type_<T>(...).commit()`。同一
`type_id` 的类型模型不一致会返回 `errc::inconsistent_type_model`，常见原因是不同模块看到的
traits、adapter 或静态能力特化不一致。

当前默认 registry 是 append-only。插件卸载前必须保证：

- registry、property、method、capability 和 extension 不再持有插件代码中的函数指针；
- 不再存在由插件模型创建并需要插件析构代码的擦除值；
- 其他线程不再执行插件注册或反射调用。

因此当前推荐插件在进程生命周期内保持加载。需要可卸载插件时，应使用未来的独立 registry
overlay 和生命周期管理方案，不能只卸载 DLL。

## Meta 视图和擦除值边界

`meta::type`、`meta::property`、`meta::method`、`meta::any_ref`、容器 view、对象 view、
schema view 和 capability facade 都是当前 runtime 内的轻量视图。它们可以在同一调用链中跨模块传递，
但不能作为稳定 ABI、跨进程数据或插件卸载后的长期缓存。

`meta::any` 如果拥有插件类型对象，会在复制、移动或析构时调用对应类型的擦除 ops。插件卸载前必须确保
这类值已经释放，或由 registry layer/plugin library lease 保活到析构完成。

插件 ABI 不应暴露 `type_record*`、raw ops 指针、用户 capability raw pointer、`memory_resource*`
或 STL 拥有型对象。需要表达类型时优先传 `type_guid`、注册名、schema 或同 runtime 内短期有效的
`type_id`，接收方再通过 registry 查询当前视图。

## 当前 Library 与 Plugin API

`platform::shared_library` 提供 move-only 的跨平台动态库所有权、符号查询和幂等关闭：

```cpp
auto result = caors::platform::shared_library::open(plugin_path);
if (!result) {
  return result.error();
}

auto library = std::move(*result);
auto entry = library.find_function<entry_fn>("plugin_entry");
```

CAORS 插件使用 `caors/plugins/abi.h` 中的纯 C ABI，并只导出
`caors_query_plugin`。`plugins::plugin_binary::load()` 会复制插件 descriptor，校验
`struct_size` 和 ABI 版本，然后提供 `initialize()`、`start()`、`stop()`、`shutdown()` 以及
版本化 extension 查询。

当前 `plugin_binary` 是协议和单插件生命周期基础设施，不是完整 plugin manager。它还没有依赖图、
registration scope 或 registry overlay，因此不能把“生命周期停止”解释为“可以安全卸载 DLL”。

## 验证

`windows-clang-shared-debug` preset 提供真实的 `host + plugin_a + plugin_b + caors.dll`
测试拓扑，覆盖跨 DLL 类型身份、并发首次解析、插件重载、模型冲突、capability key 和 extension
key 一致性。共享构建建议使用 `--parallel 1`，避免大型模板测试并行编译导致内存不足。
