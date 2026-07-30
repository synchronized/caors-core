<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta 静态类型数据与注册数据重构计划

## 文档状态

- 状态：阶段 0～5 的实现、清理、性能基准和 Windows 验证已完成
- 当前阶段：仅剩 Linux Clang/GCC 的 Debug/Release 验证
- 当前结论：静态与注册路径已统一为 `type_record + type_details`，旧 `type_descriptor` 已删除
- 尚未完成：Linux Clang/GCC 完整测试矩阵

## 重构前背景

重构前，`meta::resolve<T>()` 会先查询当前 `meta::registry`。类型已经显式注册时返回 registry
中的 `type_record`；类型尚未注册时返回绑定到静态 `type_descriptor<T>` 的轻量视图。

当时的关系如下：

```text
未注册类型
meta::type
├─ registry_   -> 当前 registry
├─ data_       -> nullptr
└─ descriptor_ -> 静态 type_descriptor<T>

已注册类型
meta::type
├─ registry_   -> 当前 registry
├─ data_       -> registry 中的 type_record
└─ descriptor  -> 通过 type_record 间接获得
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
7. 同一个 C++ 类型可以在不同 registry 中拥有独立注册节点；未注册时共享同一份静态 fallback 节点。
8. `meta::type` 不得依赖 descriptor 地址判断类型身份。

### 身份需求

静态 fallback `type_record` 应保存：

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

1. 同一个 `T` 的所有受支持实例化点必须使用一致的 traits 和 static capability 可见集合。
2. 同一类型在不同翻译单元得到的静态数据必须满足 ODR。
3. serialization、object、资源等可选模块必须通过明确 adapter 头建立特化可见性边界，不能依赖偶然的
   `.cpp` 包含顺序。
4. 显式 specialization 必须在首次实例化相关模板前可见；测试也必须遵守这一规则。
5. registry 发布后继续保持 append-only，已有 `meta::type` 视图的指针稳定性不得下降。

### 性能需求

1. 静态节点完成首次惰性初始化后，未注册 `resolve<T>()` 不再进行堆分配。
2. 已注册 `resolve<T>()` 保持一次 registry 哈希查询。
3. 静态类型节点按 `T` 惰性初始化一次。
4. `meta::type` 保持轻量、可复制，目标大小不超过两个或三个指针。
5. 不因本次重构为每个静态类型构造属性、方法、extension 等动态容器。

## 推荐数据模型

### 统一 `type_record` 节点

参考 EnTT，不再拆分 `static_type_record` 和 `registered_type_record`。静态 fallback 与 registry 注册项
使用同一种 `type_record` 节点结构：

```cpp
class type_record {
public:
  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] bool registered() const noexcept;

private:
  type_id id_{};
  const std::type_info* cpp_type_ = nullptr;
  string_name name_{};
  type_guid guid_{};
  type_semantics semantics_{};
  value_storage_ops value_{};
  builtin_ops builtin_{};
  const capability_entry* static_capabilities_ = nullptr;
  std::size_t static_capability_count_ = 0;
  memory::unique_ptr<type_details> details_{};
  bool registered_ = false;
};
```

静态 fallback 节点保存类型固有信息，`details_ == nullptr`、`registered_ == false`。registry 中的节点
保存同样的固有字段，并在显式提交时设置 `registered_ == true` 和注册详情。

名称直接保存为 `string_name`。类型身份 traits 仍返回 constexpr 文本，构造静态节点时只进行一次 intern：

```cpp
template <typename T>
[[nodiscard]] type_record make_static_type_record() {
  return type_record{
      detail::type_id_of_cpp_type<T>(),
      &typeid(T),
      string_name{type_identity_traits<T>::name},
      type_identity_traits<T>::guid,
      // semantics、value、builtin...
  };
}
```

### 可选注册详情

属性、方法和模块数据不应让每个静态节点构造空动态容器，统一延迟放入 `type_details`：

```cpp
struct type_details {
  type_id parent_id{};
  vector<property_record> properties{};
  vector<method_set_record> methods{};
  vector<nested_type_data> nested_types{};
  vector<enum_value_record> enum_values{};
  metadata_map metadata{};
  vector<capability_data> capabilities{};
};
```

早期 type extension 字段已删除，类型附加数据统一使用 `capability_data` 或 metadata。

静态 fallback 不分配 `type_details`。registry 记录只在显式 `commit()` 后存在，并由 registry 拥有完整
`type_record` 和可选详情。

```text
static type_record<T>           registry type_record
├─ id/name/guid               ├─ id/name/guid
├─ semantics/value/builtin    ├─ semantics/value/builtin
├─ static capabilities        ├─ static capabilities
├─ registered = false         ├─ registered = true
└─ details = nullptr          └─ details
                                 ├─ parent
                                 ├─ properties/methods
                                 ├─ dynamic capabilities
                                 └─ extensions
```

### 公共视图

`meta::type` 只引用一个实际节点，不再同时保存 `type_record*` 和 `type_descriptor*`：

```cpp
class type {
  const registry* registry_ = nullptr;
  const detail::type_record* data_ = nullptr;
};
```

查询规则：

- ID、名称、GUID、布局、类型形态和值操作：直接查询当前 `type_record`；
- static capability：查询当前节点的静态 capability 表；
- 父类、属性、方法、metadata、动态 capability：查询可选 `type_details`；
- 静态节点的 `registered()` 为 false，registry 节点的 `registered()` 为 true；
- registry 节点的名称和 GUID 已在 validate 阶段与静态节点完成一致性校验。

### Resolve 流程

```cpp
template <typename T>
[[nodiscard]] type resolve(context& ctx) {
  const type_record& fallback = static_type_record_of<T>();

  if (const type_record* registered =
          ctx.get_registry().find_type_record(fallback.id())) {
    return type_access::make(&ctx.get_registry(), registered);
  }

  return type_access::make(&ctx.get_registry(), &fallback);
}
```

```text
resolve<T>(ctx)
├─ 获取唯一静态 type_record<T>
├─ 查询 ctx.registry
├─ 已注册：返回 registry type_record
└─ 未注册：返回静态 fallback type_record
```

## Capability 边界

### 保留未注册静态能力

本次重构保留当前 static capability 的便利性。以下行为继续作为公共契约：

```cpp
meta::resolve<T>().get_capability<Capability>();
serialization::find_codec(meta::resolve<T>());
serialization::codec_of<T>();
```

只要对应 traits 或 static capability 特化在首次 `resolve<T>()` 前可见，这些入口不要求类型已经注册。

静态 `type_record<T>` 继续保存：

- value storage 和 builtin ops；
- serialization `codec_ops`；
- object `ptr_ops`、`handle_ops`、`object_factory_ops`；
- 用户自定义 static capability；
- 外部模块通过 module tag 贡献的 static capability。

其中 builtin ops 包括：

- scalar；
- enum；
- raw pointer；
- sequence、set、associative；
- pair、tuple、variant；
- 项目认可的核心 wrapper traits。

### 特化可见性规则

保留 static capability 的前提是同一个 `T` 的静态节点在所有实例化点保持一致：

1. traits、codec 和 static capability 特化必须在首次 `resolve<T>()`、`id_of<T>()`、`any::from<T>()`
   或其他触发静态节点实例化的入口之前可见；
2. 同一特化不得只定义在某个 `.cpp` 的后半段，也不得在已经使用 `T` 后才声明；
3. 用户类型及其 traits/static capability 特化应放在同一个公共头或配套 adapter 头中；
4. CAORS 内部跨模块适配使用明确的 adapter 头，例如：
   - `caors/resources/serialization.h`；
   - `caors/object/serialization.h`；
   - `caors/math/serialization.h`；
5. 使用模块能力的翻译单元必须先包含对应 adapter，再首次解析相关类型；
6. 跨翻译单元测试必须验证相同头文件边界下节点地址、builtin ops 和 static capability 一致。

测试中这种顺序不允许：

```cpp
void use_type() {
  meta::resolve<my_type>();
}

template <>
struct codec_traits<my_type> {
  // 特化声明得太晚
};
```

### 动态能力边界

以下内容仍只进入 registry 的 `type_details`：

- 运行期通过 builder 或插件注册的 dynamic capability；
- metadata；
- 属性、方法和其他显式注册数据。

插件运行期 capability provider 或独立 codec registry 可作为未来跨 DLL 扩展方案，但不作为普通静态
codec 的默认路径。

## 与现有行为的差异

重构前测试明确覆盖以下 descriptor-only 行为：

- 未注册类型可默认构造和销毁；
- 未注册容器和 wrapper 可创建运行期 view；
- 未注册类型可通过 `meta::type` 查询静态 capability；
- 未注册自定义 codec 可通过擦除后的 `meta::type` 序列化。

推荐方案保留以上四项，不要求 CAORS 内置 codec 或普通用户 static capability 额外注册。

行为变化主要在内部数据结构和特化规则：

- `meta::type` 从 `type_record* + type_descriptor*` 双来源收敛为单一 `type_record*`；
- 静态 fallback 与 registry 节点使用同一种结构；
- static capability 继续由静态节点保存；
- 可选模块适配必须在首次类型解析前可见；
- 显式注册只增加详情和动态能力，不是使用静态 codec 的前置条件。

## Pending、Validate、Commit

显式注册继续采用三阶段模型：

```text
type_<T>() / enum_<T>()
        │
        ▼
pending_type_record
├─ fallback = &static_type_record_of<T>()
├─ name / guid / parent
├─ properties / methods
├─ dynamic capabilities
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
└─ 以 fallback 固有字段为基础，一次性发布 registered type_record
```

不建议让后续 `resolve<T>()` 增量修改已发布 `type_record`。若某个模块能力是注册类型的一部分，应在同一次
pending record 中收集完成，保持现有 append-only 和事务语义。

## 分阶段开发计划

### 阶段 0：需求确认（已完成）

- 已确认未注册 codec/static capability 继续作为 `meta::type` 契约；
- 确认静态名称和 GUID 与显式注册冲突时返回错误；
- 确认统一 `type_record + type_details` 节点边界；
- 确认 object 静态身份的统一 traits 入口。

完成标准：本文件中的开放问题有明确结论。

### 阶段 1：统一 `type_record` 节点（已完成）

- 扩展 `type_record`，使其同时能表示静态 fallback 和 registry 注册节点；
- 新增可选 `type_details`，迁移 parent、属性、方法、metadata、capability 和 extension 容器；
- 新增 `static_type_record_of<T>()`，返回函数内唯一静态 `type_record`；
- 将 type id、RTTI、`string_name`、声明 GUID、布局、值生命周期和稳定 builtin ops 放入统一节点；
- 将 static capability 表迁移到统一节点；
- 增加跨翻译单元地址唯一和字段一致性测试；
- 保持现有 `meta::type` 公共行为不变。

完成标准：静态和注册路径使用同一节点类型；同一个 `T` 在多个翻译单元获得同一静态节点地址。

### 阶段 2：收敛 `meta::type`（已完成）

- 让 `resolve<T>()` 查询 registry 后回退到静态 `type_record<T>`；
- 让 registration pending record 以静态 fallback 的固有字段为基础构造注册节点；
- 将 `meta::type` 收敛为 `registry* + type_record*`；
- 移除 `meta::type` 中独立的 descriptor 指针和回退逻辑；
- 统一 `type_access` 和 builtin ops 查询；
- 保持 `find<T>()` 只返回显式注册类型。

完成标准：未注册和已注册视图都只查询一个 `type_record` 节点，注册后下一次 `resolve<T>()` 返回 registry 节点。

### 阶段 3：身份合并与 object 适配（已完成）

- 增加统一 `type_identity_traits<T>`；
- 接入 `traits::type_name<T>`；
- 由 object 模块适配 `static_type_name` 和 `static_type_guid`；
- 在 pending validate 中实现名称/GUID 一致性检查；
- 增加多 registry 和冲突测试。

完成标准：注册前后类型身份一致，不同 registry 的注册节点彼此独立。

### 阶段 4：Static capability 可见性收敛（已完成）

- 保留 serialization、object 和用户扩展 static capability；
- 统一 CAORS 内部类型及其 adapter 头文件边界；
- 修正所有“首次使用后才声明特化”的测试和内部实现；
- 增加未注册 codec、object capability、用户 static capability 和跨模块测试；
- 增加不同包含顺序下的编译期约束或诊断说明；
- 删除临时的翻译单元本地 descriptor workaround。

完成标准：受支持的头文件组合不会让同一个 `T` 生成不同静态节点定义，未注册 static capability
继续可用。

当前约束已经明确记录在 `docs/meta.md`：类型声明与对应 adapter 特化必须在首次静态节点实例化前
共同可见，不同翻译单元使用一致的 adapter 边界。跨翻译单元测试同时校验静态节点、sequence ops
和 serialization codec ops 的地址一致性。

“特化晚于首次实例化”的通用编译期诊断在 C++20 下无法完整可靠实现，不作为本计划的完成门禁。
C++ 模板显式特化本身会覆盖一部分错误顺序，其余场景通过公共 adapter 头、文档约束、静态模型
一致性检查和跨翻译单元测试保证。

### 阶段 5：清理和性能验证（Windows 已完成）

- 删除旧 `type_descriptor`、`descriptor_` 回退和不再使用的 helper；
- 更新 meta、registration、serialization、object 和 traits 文档；
- 对 `resolve<T>()`、`find<T>()`、属性查询和序列化调度运行 benchmark；
- 在 MSVC、Clang、GCC 的 Debug/Release 下执行完整测试。

完成标准：没有兼容残留入口，性能不低于当前实现，所有平台测试通过。

2026-07-23 已使用 `windows-clang-benchmark`、Release、`100000` 次迭代连续运行 3 次并记录
`resolve/find`、属性查询和序列化调度基线。首次结果发现完整序列化路径存在明显回退，随后定位为
字段 schema 与属性分派在每次序列化时重复编译。修复后改为在类型注册时编译一次，并保存在
`type_schema` 的私有运行时缓存中；复测的完整序列化路径已恢复到旧基线附近。修复前后结果分别见
`benchmarks/results/2026-07-23-windows-clang-benchmark-a83c174.md` 和
`benchmarks/results/2026-07-23-windows-clang-benchmark-f4f2bba.md`。
同日 `windows-clang-debug` 与 `windows-vs2022-debug` 均已完成单线程全量构建，分别有 75 项测试
全部通过。后续当前版本又完成 Windows Clang Shared Debug/Release 全量构建与各 83 项测试，以及
Windows MSVC 静态 Debug/Release 干净构建与各 81 项测试。阶段 5 仅剩 Linux Clang/GCC
Debug/Release 测试矩阵。

## 测试计划

必须新增或保留以下覆盖：

1. 两个翻译单元解析同一个基础类型，静态节点地址相同；
2. 两个翻译单元解析同一个容器/wrapper 类型，语义和 ops 相同；
3. 未注册 `resolve<T>()` 有效且 `registered()` 为 false；
4. 显式提交后下一次 `resolve<T>()` 返回注册数据；
5. `find<T>()` 在提交前为空、提交后有效；
6. 同一个静态类型在两个 registry 中注册信息独立；
7. 静态 name/GUID 与显式值冲突时提交失败且 registry 不发生部分写入；
8. adapter 在首次 `resolve<T>()` 前可见时，不同翻译单元获得一致静态节点和 capability；
9. 特化晚于首次实例化的测试代码被禁止或调整；
10. 未注册 codec、object capability 和用户 static capability 继续可用；
11. 已取得的 `meta::type` 视图生命周期和 registry append-only 约束明确覆盖。

## 风险

- `type_record` 布局调整及 `type_details` 提取会影响 meta、object、serialization、resources
  多个模块，必须分阶段提交；
- 保留 static capability 意味着必须严格管理特化声明顺序和 adapter 包含边界；
- 无法要求一个完全不知道 serialization 的翻译单元与包含 serialization adapter 的翻译单元
  为同一个 `T` 生成不同静态节点内容；内部模块必须建立统一使用约定；
- 静态节点构造 `string_name` 时会访问进程级字符串表并可能分配一次，需避免静态初始化顺序依赖；
- 静态 GUID 与显式 GUID 的冲突策略会把过去允许的覆盖变为错误；
- 跨 DLL 时模板静态节点是否唯一取决于导出边界；当前策略是要求同一 CAORS runtime、同一 CAORS
  版本、同一编译器 / 标准库 / CRT ABI 和一致的公共类型定义；
- 不能用静态节点地址替代 `type_id` 作为跨模块或持久化身份。

## 后续问题与结论

1. object 的静态名称/GUID 缺失时，允许 registration builder 补充；静态声明存在时显式输入必须一致。
2. serialization、object、resources、math 等模块类型通过各自聚合头或 `*/serialization.h`
   adapter 头保证 static capability 可见。
3. “特化晚于首次实例化”的编译期诊断 helper 尚未实现，列入后续增强，不阻塞当前静态节点模型。
4. 跨 DLL/插件场景暂不承诺静态节点地址统一；native C++ capability 和 `meta::type` 只在同一 runtime、
   同一 CAORS 版本和同一 C++ ABI 组合内使用。

核心迁移已经完成。后续工作聚焦性能基准、多编译器矩阵和 native C++ 插件边界复验，不再改变本文件确定的
`type_record + type_details` 主结构。
