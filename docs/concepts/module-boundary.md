<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 跨编译模块边界

本文说明 CAORS 在 EXE、动态库、插件和测试模块之间共享类型系统时的边界。这里的“编译模块”
包括宿主程序、普通动态库、插件 DLL/shared object 和静态库被链接进不同二进制后的实例。

## 唯一 CAORS Runtime

参与交换 CAORS 对象的模块必须共用一个 CAORS runtime。宿主、插件和参与共享的动态库必须链接到
同一个 `caors.dll` / `libcaors.so` / `libcaors.dylib` 实例。
单模块可以私有静态使用 CAORS，前提是内部对象、allocator 和 runtime 身份不跨模块边界传播。
多个完全隔离的 runtime 只能交换显式值协议，例如 GUID 字节、UTF-8 或序列化字节，不能直接互传 C++ DOM。
普通动态链接不会执行插件加载期的构建环境检查；宿主负责统一工具链、CRT、配置和版本。
当前 alpha 的动态库兼容标记按 major.minor 划分，包版本匹配只筛选版本，不证明 ABI 兼容。

不支持下面这种拓扑中的 CAORS 对象互传：

```text
host.exe ── static caors runtime A
plugin_a.dll ── static caors runtime B
plugin_b.dll ── static caors runtime C
```

多份 runtime 会各自拥有 registry、默认 context、memory resource、`string_name` 驻留表、
`type_id` 分配器和 capability key。即使 C++ 类型名相同，跨 runtime 的
`meta::type`、`type_id`、`any`、capability 指针和 allocator 指针也没有统一语义。

CAORS 的 C++ meta 注册与查询接口当前应视为 native C++ plugin API：它要求宿主、插件和共享库使用
同一 CAORS runtime、同一 CAORS 版本、同一编译器主版本、同一 C++ 语言模式、同一标准库/CRT ABI、同一异常和 RTTI 配置，以及一致的公共
类型定义、traits、adapter 和 CAORS 编译配置。它不是 stable C ABI，也不承诺跨编译器、跨 CRT、
跨进程或多 runtime 兼容。

## 可以跨模块传递的稳定数据

下面区分数据的逻辑身份与其传输表示。独立 C ABI、持久化文件和远程协议应使用显式布局结构、
GUID 字节、UTF-8 文本或序列化字节；不能因为逻辑上属于数据，就直接传递 C++ DOM 或 STL 对象：

- `type_guid`：用于存档、资源和跨版本协议；
- 注册类型名：用于诊断、配置和人工可读查询；
- `type_id`：只允许在同一进程、同一 runtime 的短期查询中使用；
- schema、JSON、variant/serialization DOM 的序列化表示；DOM C++ 对象本身仍受 native ABI 和生命周期约束；
- POD/显式 ABI 结构和版本化 C ABI descriptor。

这些数据可以在接收方重新通过 `meta::find()`、schema 查询或序列化上下文解析为当前 runtime 的视图。

## 只能进程内临时传递的视图

以下类型是轻量运行期视图，不拥有完整数据，适合函数调用期间使用，不适合作为稳定 ABI 或长期缓存：

- `meta::type`；
- `meta::property`、`meta::method`、`meta::method_set`、`property_accessor`；
- `meta::any_ref`；
- `sequence_view`、`set_view`、`associative_view`、`pair_view`、`tuple_view`、`variant_view`、
  `indirect_view`、`enum_view`；
- `objects::ptr_view`、`objects::handle_view`；
- `serialization::schema_view`、`serialization::codec_capability` 等 facade/view。

这些对象内部可能引用 registry layer、`type_record`、运行期对象地址或类型擦除 ops。插件 layer
detach 后，旧视图应失效；调用方不能绕过有效性检查继续访问内部指针。

## 不应跨 ABI 边界共享的对象

以下对象不应作为插件 ABI 字段、跨 DLL 持久缓存或跨 runtime 数据传递：

- `type_record*`、registry/detail 内部节点和 raw record 指针；
- `meta::type::get_raw_ops<Ops>()` 返回的 ops 指针；
- 用户 capability 默认返回的 `const Capability*`，除非协议明确它只在当前调用期间有效；
- 指向插件静态数据或插件对象的 capability raw pointer；
- `memory_resource*`、allocator、容器内部 buffer 或由另一份 runtime 分配的内存；
- `meta::any` 中拥有的插件类型对象，如果其析构、复制或移动需要插件代码；
- `std::string`、STL 容器等 C++ ABI 不稳定对象，除非双方明确使用完全相同 ABI 且只在内部调用链中使用。

公共插件 ABI 应使用纯 C ABI、固定大小结构、显式 `struct_size`、版本号、GUID 和只读字符串视图，必要时由
host 复制字符串和 descriptor 数据。

## Meta 相关风险

`meta::type` 内部是 registry 与 `type_record` 的只读视图。`type_record` 不是纯元数据，它包含：

- `value_storage_ops`：构造、复制、析构、赋值等函数指针；
- `builtin_ops`：标量、枚举、指针/wrapper、容器、pair、tuple、variant 等 ops；
- 静态 capability 表指针；
- 属性、方法、枚举、nested type、metadata 和动态 capability；
- 可能由插件注册的 factory、constructor、codec 或用户自定义 concept table。

因此 `meta::type` 可以在同一 runtime、同一进程、插件 layer 活着时短期传递；不能当作跨 DLL 稳定
ABI 数据包。需要跨边界持久表达类型时，传 `type_guid`、注册名或短期 `type_id`，使用时重新查询。
插件注册必须写入 `plugin_manager` 为当前插件创建的 draft layer，不允许使用默认 context 向
primary 或其他外部 registry 注册；`pinned_process` 也不例外。

`meta::any` 和 `meta::any_ref` 的风险更高：`any_ref` 引用外部对象，`any` 可能拥有对象并在析构时调用
类型擦除 destroy ops。如果对象类型来自插件，插件卸载必须等待这些值释放，或由 registry layer lease
保活到析构完成。

## Capability 和 Host Extension

静态 capability 适合编译期可见、进程内静态生命周期的类型固有能力。它依赖模板特化可见性和当前
runtime 的 capability key，不是插件协议身份。

动态 capability 由注册 builder 写入已注册类型的 `type_record`。CAORS 可以拥有 ops 对象生命周期，
但 ops 内部如果保存插件函数指针或插件对象指针，仍受插件 layer 生命周期约束。

`meta` 不再提供注册类型上的独立 extension 数据槽。需要挂在类型上的行为或动态数据应使用
capability / capability data；只读标签和工具提示使用 metadata；需要批量扫描或替换策略时由模块维护自己的索引。

插件 host extension 是 C ABI 中的宿主服务查询机制，仍通过字符串 ID 和版本查询。它不等同于
`meta::type` 上的数据挂载。

## 插件卸载约束

插件 unload 前必须满足：

- 插件线程已停止；
- registry layer 已 detach，新的查询不会再看到该插件类型；
- 没有新的 property/method/capability/view 调用能进入插件代码；
- 已进入的调用完成，或由 layer lease 保活；
- 插件创建且需要插件析构代码的 `meta::any` / owned value 已释放；
- host 不再持有需要插件对象生命周期的 `any_ref`、container view、object view 或 raw ops 指针。

如果不能证明这些条件，插件应保持加载到进程结束。只调用 `FreeLibrary` / `dlclose` 而不清理 registry
和擦除值是不安全的。

## 推荐使用规则

- 普通业务查询使用 `meta::resolve<T>()`、`meta::find<T>()` 和 `meta::type::get_capability<C>()`。
- 插件 ABI 使用 GUID、name、schema 和 C ABI descriptor，不直接暴露 `meta::type`、`any` 或 raw ops。
- view/facade 只在当前调用链内使用，跨帧或跨线程缓存时优先缓存 `type_id` / `type_guid` 后重新查询。
- 需要跨模块共享 C++ 类型时，把类型、traits、adapter、GUID 和注册代码放在公共头或明确的注册模块中。
- 同一进程中不要混用静态链接 CAORS 和动态链接 CAORS 的模块。
