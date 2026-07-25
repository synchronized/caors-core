<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Type identity state 跨模块重构计划

## 文档状态

- 当前状态：待实施；
- 实施范围：C++ 类型运行期身份、跨 DLL 统一、registry 主索引与 RTTI fallback；
- 非实施范围：`type_guid` 格式、property/method wrapper、builtin capability 和序列化格式。

## 背景

当前 `type_id_of_cpp_type<T>()` 使用模板静态变量地址生成 `type_id`：

```cpp
template <typename T> struct cpp_type_id_storage {
  inline static const int key = 0;
};

template <typename T>
type_id type_id_of_cpp_type() noexcept {
  return type_id{reinterpret_cast<std::uintptr_t>(&cpp_type_id_storage<T>::key)};
}
```

该地址在一个链接模块内通常唯一，但 EXE 和不同 DLL 可能分别实例化 `key<T>`。同一个 C++ 类型
因此可能得到不同 `type_id`：

```text
EXE   T -> 0x1000
DLL A T -> 0x5000
DLL B T -> 0x9000
```

这会影响：

- `resolve<T>()`、`find<T>()` 和 `register_type_for<T>()` 的统一结果；
- property、method、parent 和 builtin ops 返回的子类型身份；
- static capability 与 registry capability 的汇合；
- EXE、插件和 runtime 之间共享同一个 `type_record`；
- 类型模型一致性检查。

现有 `std::type_index -> type_record*` 索引只在类型注册后提供 RTTI 动态类型查询，不能统一首次
生成的地址型 `type_id`，也不应作为跨 DLL 主身份。

## 设计目标

- 同一 CAORS runtime 中，同一个 C++ 类型从 EXE 或任意 DLL 获取相同 `type_id`；
- `resolve<T>()` 首次进入 runtime intern，后续在调用模块中直接命中静态缓存；
- `type_id` 继续只表示一次进程运行中的快速身份，不承担持久化语义；
- `type_guid` 继续承担跨进程、跨版本和序列化稳定身份；
- 类型 identity 与完整 `type_record`、静态模型和注册 metadata 分离；
- 不采用“先生成完整 type model，再由先注册模块获胜”的 RTTR 路线；
- identity 节点地址在 CAORS runtime 生命周期内稳定；
- 支持未显式注册、没有用户名称和 GUID 的任意完整 C++ 类型；
- registry 仍可按 `type_id` 高效查询，并保留多 context、多 registry 能力。

## 目标结构

```text
EXE / DLL 模板层
└─ static_type_identity<T>()
   └─ 模块本地 static const type_identity*
      └─ 首次调用 runtime get_or_create_type_identity(canonical_name)

CAORS runtime
└─ global type_identity_state
   ├─ canonical C++ name -> type_identity
   ├─ type_id -> type_identity
   └─ 单调分配下一 type_id

meta::registry（每个 context 可独立）
├─ type_id -> type_record
├─ registered name -> type_record
└─ type_guid -> type_record
```

`type_identity_state` 只回答“两个模板实例表示的是否为同一 C++ 类型”。`registry` 回答该类型在
当前 context 中注册了哪些反射数据。`type_guid` 回答持久化数据如何稳定识别类型。

## Type identity 数据

建议内部节点保持最小：

```cpp
namespace caors::meta::detail {

struct type_identity {
  type_id id{};
  string_name canonical_name{};
};

} // namespace caors::meta::detail
```

约束：

- 节点由 CAORS runtime 分配和持有；
- 节点一经发布，不修改 `id` 和 `canonical_name`；
- 节点在正常 runtime 生命周期内不删除；
- `type_id{0}` 保留为无效值；
- 有效 ID 由 state 单调顺序分配，不由地址或名称 hash 直接派生；
- `type_id` 继续保存固定宽度整数，避免把 runtime 指针暴露为公共 ABI。

顺序 ID 没有名称 hash 碰撞，但每次进程运行可能不同。这符合 `type_id` 的运行期定位职责。

## 三种名称必须分离

### Canonical C++ identity name

identity intern 使用的内部名称，必须能为任意 `T` 生成，并且 identity 创建后不可改名。

首版从编译器函数签名提取：

```text
MSVC     -> __FUNCSIG__
Clang/GCC -> __PRETTY_FUNCTION__
```

实现应集中到单一 `canonical_cpp_type_name<T>()`，规范化顶层 cv/ref，并返回静态存储期
`std::string_view`。

它只用于相同工具链和 ABI 配置下的进程内跨模块统一，不作为公共显示名称或持久化协议。
同一进程不支持混用产生不同 C++ ABI 类型名称的编译器模块。

canonical name 不采用 RTTR 式名称美化或注册名称传播。实现只执行：

- 在进入名称提取前使用 `std::remove_cvref_t<T>` 归一顶层 cv/ref；
- 从编译器函数签名中精确截取完整类型文本；
- 保留 namespace、模板参数、指针层级、模板参数中的 cv、allocator、policy 和调用约定等
  可能影响类型身份的内容；
- 保存长度并按完整字符串判等；容器内部可以使用 hash 加速，但 hash 不能单独决定 identity。

不得执行：

- 用 `traits::type_identity<T>::name()` 或注册名称替换 canonical name；
- 为了显示友好而把 `std::basic_string<...>` 改写为 `std::string`；
- 删除可能影响 ABI 或类型差异的模板参数、空格片段或编译器标记；
- 在某个类型注册改名后，遍历并重写指针、数组或组合类型的 canonical name。

RTTR 需要美化和传播名称，是因为其类型名称同时承担编译器类型去重、`get_name()` 展示、
`get_by_name()` 查询和派生类型命名等职责。CAORS 将这些职责拆分后，canonical name 只承担
C++ identity intern，不需要可读性，也不能在 identity 发布后改变。

### Registered/display name

`traits::type_identity<T>::name()`、object `static_type_name` 和 registration builder 提供的名称
继续用于：

- `meta::type::name()`；
- `registry::find_type(name)`；
- 日志、编辑器和序列化类型标签。

注册名称可晚于首次 `resolve<T>()` 出现，因此不能作为 identity 的唯一 intern key。不同翻译单元
对 `traits::type_identity<T>` 的可见性不同，也不能导致同一个 C++ 类型产生不同 identity。

指针、容器、pair、tuple 和 variant 等组合类型仍可通过现有 `static_type_name_text<T>()` 生成
友好显示名称。该名称属于 registry/display 层，不参与 identity 判等，也不反向修改 canonical
name。

### Stable type GUID

`traits::type_identity<T>::guid()` 或注册接口提供的 `type_guid` 继续用于持久化和跨版本协议。
GUID 不由运行期 `type_id`、canonical name、`string_name` entry 或其 hash 派生。

## Create or get 流程

模板入口仿照 RTTR 的本地静态缓存，但只 intern identity：

```cpp
template <typename T>
[[nodiscard]] const type_identity* static_type_identity() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  static const type_identity* identity =
      get_or_create_type_identity(canonical_cpp_type_name<type>());
  return identity;
}

template <typename T>
[[nodiscard]] type_id static_type_id() noexcept {
  const type_identity* identity = static_type_identity<T>();
  return identity != nullptr ? identity->id : type_id::invalid();
}
```

首次调用：

```text
生成 canonical C++ name
        ↓
进入 CAORS runtime type_identity_state
        ↓
持锁查询 canonical name
   ├─ 已存在：返回已有 identity
   └─ 不存在：顺序分配 type_id，创建并发布 identity
        ↓
模块本地 static 缓存 identity*
```

后续调用只读取模块本地静态指针，不查询 map、不加 registry 锁。

与 RTTR 的差异：

- RTTR 候选对象包含完整 `type_data`，重复时由先注册模型获胜；
- CAORS 只共享 `type_id + canonical_name`；
- `value_storage_ops`、builtin ops、static capability 和 wrapper model 仍由现有静态模型生成；
- registry commit 继续执行模型签名一致性检查，不让 identity intern 隐藏 ODR/adapter 可见性差异。

## 并发与生命周期

`type_identity_state` 是写少读多的进程级 interner：

- 首次创建 identity 时持有互斥锁；
- canonical name 查询和节点创建必须在同一个临界区完成；
- 已返回的节点不可移动和删除；
- state 容器扩容不得改变节点地址，使用独立分配节点或地址稳定容器；
- 模块本地函数静态初始化由 C++ 保证单次执行；
- 多个类型首次并发 intern 时由 state 锁保证 ID 不重复；
- 发布后读取 `type_identity` 不需要加锁。

不支持在 CAORS runtime 已卸载后继续调用模块静态缓存。插件卸载不会删除 runtime 持有的 identity，
因此其他模块缓存的 identity 指针不会因插件卸载而失效。

## 与 string_name state 的关系

复用 interner 架构思想，但不把 `string_name` 直接作为 `type_id`：

```text
string_name state
└─ 任意运行期字符串驻留

type_identity_state
└─ C++ canonical identity 去重与顺序 ID 分配
```

`type_identity::canonical_name` 可以保存 `string_name`，避免重复拥有名称文本。实现时必须确认
两个全局 state 的初始化与销毁顺序：

- identity state 创建时可以安全调用 string interner；
- runtime 关闭时 identity state 必须先于 string state 销毁，或两者都保持到进程退出；
- public API 不返回可在 runtime 卸载后继续解引用的内部名称地址。

## Registry 迁移

registry 继续以 `type_id` 为主索引，但所有模板入口必须先通过 identity state 获取统一 ID：

- `static_type_record_ptr_of<T>()`；
- `register_type_for<T>()`；
- `registry::find_type<T>()`；
- `set_type_parent_for<Child, Parent>()`；
- property/method 的 owner、value、return 和 parameter type；
- scalar、indirect、sequence、set、associative、pair、tuple 和 variant 子类型；
- static capability 注册与查询；
- object `static_type_id()`；
- `any` / `any_ref` 类型视图。

注册名称和 GUID 仍是 registry 内的可选二级索引。不同 registry 可以为同一个全局 `type_id`
保存独立 `type_record`，identity state 不直接引用任何 registry record。

## RTTI map 处理

当前 `std::type_index -> type_record*` 只保留为普通多态 C++ 对象的动态类型 fallback：

```text
typeid(*base_ptr)
        ↓
registry cpp_types
        ↓
dynamic type_record
```

它不参与：

- `resolve<T>()` identity 创建；
- `register_type_for<T>()` 主去重；
- registry 的 `type_id` 生成；
- 跨 DLL identity 统一。

分阶段处理：

1. identity state 落地时保留 `cpp_types`，避免同时改变普通多态 pointer 行为；
2. object 子类优先通过对象自己的虚拟 `type_id()` 返回动态身份；
3. 盘点剩余 RTTI fallback 调用；
4. 若只剩可选普通 C++ 多态支持，将 RTTI map 移入独立 adapter；否则保留并明确其工具链限制。

## 错误与诊断

新增或复用明确错误语义：

- canonical name 为空：`errc::invalid_type`；
- ID 空间耗尽：`errc::out_of_memory` 或新增专用错误；
- 相同 identity 注册模型签名不同：继续返回 `errc::inconsistent_type_model`；
- 相同注册名称指向不同 identity：`errc::duplicate_name`；
- 相同 GUID 指向不同 identity：`errc::duplicate_guid`；
- runtime 状态不可用：返回无效 identity，不在 `noexcept` 边界抛出异常。

Debug 模式可在 identity 节点保存额外诊断信息，但首版不把 adapter/capability 签名放入 identity；
模型一致性仍由 registry commit 负责。

## DLL 与 ABI 约束

- 所有模块必须调用同一个 CAORS runtime 导出的 `get_or_create_type_identity()`；
- 不能在 header 中为每个 DLL 创建独立 identity state；
- `type_identity` 保持 detail 类型，公共 API 继续传递固定宽度 `type_id`；
- canonical name 仅承诺相同工具链、ABI、编译选项和 CAORS 配置下的一致性；
- 匿名 namespace 类型、lambda 和局部类型可以获得模块内 identity，但不承诺跨模块合并；
- 插件不能把与宿主同名但 ABI 不同的 C++ 类型视为同一类型；
- identity 统一不等于模型一致，跨模块 adapter 可见性仍由模型签名诊断。

## 实施阶段

### 阶段 0：基线与真实 DLL 测试

- 记录当前 `sizeof(type_id)` 和公共 ABI；
- 增加真正的 shared-library 测试目标，不以多个 `.cpp` 链入同一 EXE 代替 DLL；
- 证明改造前同一 `T` 的地址型 ID 在宿主与 DLL 可能不同；
- 覆盖 EXE 先 resolve、DLL 先 resolve 和并发首次 resolve。

### 阶段 1：Canonical name

- 新增 `canonical_cpp_type_name<T>()`；
- 覆盖 MSVC、clang-cl 和 Windows Clang 当前支持 preset；
- 验证 cv/ref 归一化、指针、容器、模板、匿名类型和局部类型；
- 明确它与 `traits::type_identity<T>::name()` 的职责差异。

### 阶段 2：Global identity state

- 新增 runtime-owned `type_identity_state`；
- 实现 `get_or_create_type_identity(canonical_name)`；
- 顺序分配非零 `type_id`；
- 保证节点地址稳定和并发首次创建安全；
- 增加同名复用、异名分配、并发、生命周期和空名称测试。

### 阶段 3：模板入口迁移

- 将 `type_id_of_cpp_type<T>()` 和 `static_type_id<T>()` 切换到 identity state；
- 保持 `type_id` 公共布局不变；
- 迁移 static type record、registration、parent、capability 和 builtin 子类型调用点；
- 删除 `cpp_type_id_storage<T>::key` 地址型实现；
- 保持 `resolve<T>()` 未注册 fallback 行为。

### 阶段 4：Registry 与 RTTI 复审

- 确认 registry 只把统一后的 `type_id` 作为主索引；
- `std::type_index` 降级为动态 RTTI fallback；
- object 动态类型优先使用虚拟 `type_id()`；
- 评估是否将普通 C++ RTTI 支持拆为 adapter；
- 更新 registry dump、模型签名和冲突诊断。

### 阶段 5：跨模块验证与文档收口

- 验证宿主和多个 DLL 对同一个 `T` 获得相同 `type_id`；
- 验证不同类型获得不同 ID；
- 验证插件卸载后 identity 仍由 runtime 持有；
- 验证多个 registry 共享 identity、但各自拥有独立 record；
- 完整回归 property、method、value、serialization、object 和 capability；
- 更新 `docs/meta.md` 中地址型 key、RTTI map 和 DLL 限制说明。

## 验收标准

- `type_id` 仍为固定宽度、可复制、无效值为零的运行期标识；
- 同一 runtime 中 EXE、DLL A 和 DLL B 的 `id_of<T>()` 相同；
- `remove_cvref_t<T>` 共享同一 identity；
- 不同 canonical C++ 类型获得不同顺序 ID；
- 首次并发 resolve 不产生重复节点或数据竞争；
- 后续 `static_type_id<T>()` 不查询 registry map；
- identity state 不保存 property、method、builtin ops、capability 或 metadata；
- 未注册类型继续拥有有效 `meta::type` fallback；
- 注册后的 `meta::type` 继续覆盖 fallback record；
- `type_guid` 和序列化格式不变；
- 模型不一致仍能被诊断；
- Windows Clang Debug 使用 `--parallel 1` 完成相关构建和测试；
- 每个实施阶段独立提交并推送。

## 非目标

- 不让 `type_id` 跨进程或跨版本稳定；
- 不用 canonical name hash 代替顺序 ID；
- 不把注册显示名称作为不可变 C++ identity；
- 不把完整 `type_record` 移入全局 identity state；
- 不复制 RTTR 的完整候选 `type_data` 先注册者获胜模型；
- 不在 identity state 中合并 static capability 或 adapter 模型；
- 不在首阶段删除所有 RTTI 支持；
- 不支持混用 ABI 不兼容的编译器模块；
- 不承诺 runtime 卸载后内部 identity 指针仍可使用。
