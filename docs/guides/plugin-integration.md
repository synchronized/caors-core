<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 动态插件接入与生命周期

本文说明 CAORS 的动态库和插件边界。当前插件模型以“宿主和插件共享同一份 CAORS runtime”
为前提。更完整的跨编译模块 API 风险和传参边界见 [module-boundary.md](../concepts/module-boundary.md)。

## 推荐拓扑

宿主与插件共享唯一动态 CAORS runtime，native C++ ABI 的共同前置条件统一由
[模块边界](../concepts/module-boundary.md#唯一-caors-runtime)维护，本页说明插件接入和加载/卸载 API。

## 编译与 ABI 要求

跨模块公共类型使用共同头文件；匿名 namespace、lambda 和局部类型属于插件私有实现，不能充当协议类型。
C++ view/ops 与版本化 C ABI 的区别见[跨边界传参规则](../concepts/module-boundary.md#推荐使用规则)。
Windows 的具体 CRT 配置和加载期诊断如下。

### Windows CRT 与构建环境校验

启用动态插件时，Windows 官方支持的配置是：

- Debug：host、plugin、CAORS 和相关静态库统一使用 `/MDd`；
- Release、RelWithDebInfo、MinSizeRel：统一使用 `/MD`；
- 同一进程不支持 `/MD`、`/MDd`、`/MT`、`/MTd` 混用；
- 不使用动态插件的单体静态程序仍可选择 `/MT` 或 `/MTd`。

需要调试 Release host 中的插件时，应继续使用 `/MD`，通过 `/Zi` 和 PDB 提供调试信息，不应改用
`/MDd`。官方插件 CMake 目标通过 `MSVC_RUNTIME_LIBRARY` 固定上述规则。

host 通过 `caors.host.runtime` 发布自身 `caors_plugin_build_environment`。每个插件必须通过
`caors.plugin.build` 返回 `caors_plugin_build_extension`，建议直接使用：

```cpp
static const caors_plugin_build_extension build{
    sizeof(caors_plugin_build_extension),
    CAORS_PLUGIN_BUILD_EXTENSION_VERSION,
    CAORS_PLUGIN_BUILD_ENVIRONMENT_INITIALIZER,
};
```

`plugin_binary::load()` 在插件进入 `loaded` 和 `initialize` 前比较编译器、编译器版本、C++ ABI
工具集、CRT 模式、`_ITERATOR_DEBUG_LEVEL`、指针宽度和处理器架构。缺少 build extension 或任一字段
不一致都会返回 `plugins/inconsistent_type`。

调用方需要定位具体差异时，可以使用诊断重载：

```cpp
caors::plugins::plugin_build_compatibility_diagnostic diagnostic;
auto loaded = caors::plugins::plugin_binary::load(path, &host.abi(), &diagnostic);
if (!loaded &&
    caors::plugins::has_mismatch(
        diagnostic.mismatches,
        caors::plugins::plugin_build_mismatch::crt_mode)) {
  // diagnostic.host 和 diagnostic.plugin 保存双方环境快照。
}
```

构建环境一致只能证明已知 ABI 前置条件满足，不能允许 STL 拥有型对象、异常或隐式内存所有权穿过稳定
C ABI。跨边界数据仍应使用固定布局结构、借用视图、opaque handle 和明确的创建/销毁函数。

## 类型身份

- `type_id` 是同一 runtime 内的临时身份，由 canonical C++ 类型名称驻留后顺序分配；
- `type_id` 不跨进程、版本或多份 runtime 稳定，不能写入持久化格式；
- `type_guid` 用于存档、资源和跨版本协议，必须由公共声明显式提供且保持不变；
- 注册显示名称用于查询和诊断，不参与 C++ 类型身份判等；
- `std::type_index` 只用于普通 C++ 多态对象的动态类型 fallback，不是主身份来源。

`capability_key` 同样由共享 runtime 按 canonical C++ 类型名称驻留。它只适合当前进程内快速查询；
需要稳定插件协议时，还应为 capability 提供名称、GUID 和版本。插件 host extension 是另一套
C ABI 服务查询机制，使用字符串 ID 和版本，不使用 `capability_key`。

## 注册与卸载

插件必须在 `initialize()` 期间通过 Host meta registration extension 取得自身 draft registry，
创建显式 `meta::context`，再完成 `registration::type_<T>(context, ...).commit()`。插件不得使用
默认 context，也不得直接向 primary registry 注册。同一 `type_id` 的类型模型不一致会返回
`errc::inconsistent_type_model`，常见原因是不同模块看到的 traits、adapter 或静态能力特化不一致。

`plugin_manager::load(path)` 默认按 `detachable` 策略创建插件 registry layer。调用方也可以传入
`plugin_load_options{plugin_lifetime_policy::pinned_process}`，表示该插件钉在 manager 或进程生命周期内，
显式 `unload()` 会返回 `errc::unsupported` 并给出 `pinned_process` 诊断，不提前关闭动态库。
无论使用 `detachable` 还是 `pinned_process`，插件都只能写入 manager 为其创建的 draft layer，
不得绕过 layer 写入 primary 或其他外部 registry。`pinned_process` 只改变卸载和调用 guard 策略，
不放宽注册目标约束。

detachable 插件卸载前必须保证：

- registry、property、method、capability 和擦除值不再持有插件代码中的函数指针；
- 不再存在由插件模型创建并需要插件析构代码的擦除值；
- 其他线程不再执行插件注册或反射调用。

`plugin_manager::unload()` 会执行 stop/shutdown、scope cleanup、registry layer detach 和 lease
检查。仍有旧 view、owning value、in-flight call 或 scope blocker 时返回 `errc::pending`，插件状态为
`detached_pending_release`；调用方可以通过 `plugin::unload_diagnostic()` 查看 generation、layer
lease、in-flight 调用数、本地注册类型数量和阻止原因。

### 插件内存域

宿主为每个插件创建独立 registry layer，并决定该 layer 的长期资源。插件只向获得的显式 context 注册，
不得把记录改分配到插件局部 TLS、短生命周期 scratch 或卸载时先于 layer 销毁的资源。layer detach 后，
资源仍须覆盖旧 view、owning value 和 in-flight 调用所持 lease；最后一个 lease 释放后才能销毁 layer 并
关闭动态库。

EXE 与每个 DLL 可以拥有不同的 TLS 实例，因此 `library_temporary_resource()` 只代表当前模块、当前线程的
同步 scratch。插件不能把由该资源分配的指针、view 或容器返回给宿主，也不能把它捕获到异步任务。native
C++ 调用需要跨模块保存结果时，应由宿主显式提供生命周期足够长的结果资源；稳定 C ABI 则继续使用借用
字节视图、opaque handle 和成对创建/销毁函数。当前 runtime extension 不发布通用 allocator 服务，也不
允许直接跨 C ABI 传递 `memory_resource*`。

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

`plugin_binary` 只是协议和单插件生命周期基础设施；单独使用它时，不能把“生命周期停止”解释为
“可以安全卸载 DLL”。完整卸载流程由 `plugin_manager` 负责，它会处理依赖、registration scope、
registry layer detach、旧 lease 和 in-flight 调用；任一前置条件未满足时不会关闭动态库。

## 验证

`windows-clang-debug` preset 提供真实的 `host + plugin_a + plugin_b + caors.dll`
测试拓扑，覆盖跨 DLL 类型身份、并发首次解析、插件重载、模型冲突、capability key 和 plugin host
extension 查询一致性。共享构建建议使用 `--parallel 1`，避免大型模板测试并行编译导致内存不足。

## 宿主显式保活资源模块

通用资源句柄的寿命边界由[资源参考](../reference/resources.md#模块卸载与资源寿命)维护。
宿主在模块对象或 allocator 进入外部缓存前获取 `plugin_binary::library_lease()`；不要在收到卸载请求后才获取。
宿主卸载入口先停止新请求，对仍有资源、弱引用或工作中的模块返回 pending/拒绝，并保留原 pin。
在模块的 allocator/服务销毁前，完成以下顺序：

1. 关闭加载入口，取消等待并排空后台任务和完成队列；取消本身不能替代等待。
2. 清理 cache、请求/等待状态与外部强引用，再清理弱引用及其控制块。
3. 销毁仍引用模块函数和 allocator 的回调、任务、适配上下文，再销毁 allocator/服务。
4. 由宿主释放最后 pin 并完成插件卸载，不从插件内部最后一条执行路径自行卸载代码。

`plugin_manager` 无法自动枚举宿主的通用资源和弱引用，宿主必须在调用卸载流程前执行该门禁。
仅保存 library lease 不代表插件逻辑服务可以提前 shutdown；它只保证动态库代码仍映射。
创建/加载失败用 RAII 回收临时 pin，长期 pin 由宿主统一持有，避免反向所有权循环。
跨 CRT 对象仍使用兼容构建和创建方释放协议，普通 DLL 不自动获得插件构建环境检查。

任务同时保存 pin 与弱引用时，应使用明确成员析构顺序的拥有对象，保证依赖先销毁、pin 最后释放；不要依赖 lambda 捕获成员的隐含顺序。

## Linux 插件符号与卸载

插件目标应设置 `CXX_VISIBILITY_PRESET hidden`、`C_VISIBILITY_PRESET hidden` 和
`VISIBILITY_INLINES_HIDDEN YES`，仅显式导出协议入口，避免不同插件的模板静态模型被动态链接器合并。
使用 GCC 构建可卸载插件时，还应为该目标的 C++ 编译添加 `-fno-gnu-unique`；
GNU unique 符号可能导致 `dlclose` 不执行真实卸载，见 [GCC 编译选项说明](https://gcc.gnu.org/onlinedocs/gcc/Code-Gen-Options.html#index-fno-gnu-unique)。
这些配置属于插件自身的构建约束，不通过 CAORS 链接目标强制修改宿主的全部编译选项。
仓库中的 plugin identity 与 protocol 测试插件采用相同约束，并保留类型冲突与真实卸载验证。
