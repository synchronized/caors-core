<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta value 边界收敛计划

## 文档状态

- 当前状态：待实施；
- 实施范围：`any_ref` API、object factory 注册、adapter 可见性规范和模型一致性诊断。

## 目标结构

```text
CAORS any / any_ref
└─ meta::type
   └─ 直接引用 type_record
      ├─ value_storage_ops
      ├─ builtin_ops
      ├─ static capability
      └─ registered capability
```

`type_record` 继续保存或公开：

- 类型布局与值生命周期操作；
- scalar、enum、indirect、sequence、set、associative、pair、tuple 和 variant 等
  `builtin_ops`；
- object、serialization、custom 和外部模块贡献的 static capability；
- property、method、constructor、动态 capability 和 extension 等注册数据。

`any`、`any_ref` 和 `owned_value` 保存 `meta::type`，所有值操作和 capability 查询以
`type_record` 为单一事实源。

## 实施范围

### any_ref 类型安全入口

现有：

```cpp
any_ref::from_raw(meta::type, const void*, bool writable);
```

允许调用方错误组合类型、地址和可写性。公共入口应收敛为模板接口：

```cpp
template <typename T>
[[nodiscard]] static any_ref from(meta::type type, T* data) noexcept;
```

该入口负责：

- 去除 `T` 的 cv 修饰；
- 校验显式 `meta::type` 与 `resolve<T>()` 的 ID、大小和对齐；
- 根据 `T` 的 const 性自动推导 writable；
- 空地址或类型不匹配时返回无效引用。

只有 `meta::type + void*` 的内部流程保留私有擦除入口：

```cpp
[[nodiscard]] static any_ref from_erased(
    meta::type type,
    const void* data,
    bool writable) noexcept;
```

内部擦除入口服务：

- `any::ref()` 和 `owned_value::ref()`；
- property record 的运行期回退；
- sequence、set、associative、pair、tuple、variant 和 indirect view；
- serialization schema 与动态值分派。

`property_model` 等仍然知道具体 `T` 的位置应迁移到模板入口。

### object factory 自动注册

`object_factory_ops` 是 object 类型级创建能力，不是已有值的实例能力。提交
`registration::type_<T>()` 时，如果 `T` 继承 `meta::object`，应自动生成并添加对应
`object_factory_ops`。

相关能力归属保持明确：

- `ptr_ops` 属于 `objects::ptr<T>`；
- `handle_ops` 属于 `objects::handle` 和 `objects::typed_handle<T>`；
- `codec_ops` 属于实际具有 `codec_traits<T>` 的值类型。

`ptr_ops`、`handle_ops` 和 `codec_ops` 继续使用模块 static capability 机制，不让 meta/value
直接依赖 object 或 serialization。

## Adapter 可见性与 ODR

如果不同翻译单元在首次实例化 `static_type_record_of<T>()` 时看到不同 traits 或 static
capability，链接器可能静默保留其中任意一份同名 COMDAT。

根本约束是为接入 meta 系统的类型提供统一 adapter 头：

```cpp
// player_meta.h
#include "player.h"
#include "caors/meta.h"
#include "caors/object.h"
#include "caors/serialization.h"

// type_name<Player>
// codec_traits<Player>
// builtin traits
// static capability
```

所有调用以下接口的翻译单元必须在首次模板实例化前包含同一份完整 adapter：

```cpp
resolve<T>();
any::from<T>();
any_ref::from<T>();
registration::type_<T>();
```

禁止在某个翻译单元已经实例化 meta 模型后，才在另一个翻译单元补充相关特化。

## 模型一致性诊断

统一 adapter 头是正确性要求；模型签名只是辅助诊断。

Debug 构建可为注册输入保存：

```cpp
struct type_model_signature {
  type_id id{};
  type_layout layout{};
  type_kind kind = type_kind::value;
  std::uint64_t builtin_mask = 0;
  std::uint64_t static_capability_hash = 0;
};
```

签名规则：

- builtin 使用固定 bit；
- static capability 使用排序后的稳定描述计算 hash；
- 优先使用 `capability_traits<Capability>` 的 stable GUID、name 和 version；
- 不使用 vtable、ops 或 `capability_key` 指针地址计算稳定签名；
- 没有稳定描述的 custom capability 只参与当前进程内的详细集合比较。

`pending_type_record::validate()` 和 registry commit 比较同一 `type_id` 的已发布签名。发现不一致
时返回 `errc::inconsistent_type_model`，Debug 诊断列出 builtin 和 capability 差异。

普通 inline `resolve<T>()` 内的签名检查不可靠，因为函数本身也可能被 COMDAT 合并。专项测试可
使用 TU-local probe：

```cpp
CAORS_VALIDATE_META_TYPE(T);
```

该宏在匿名 namespace 生成本地对象，直接提交当前翻译单元计算的签名。它只用于 Debug 和
跨翻译单元反例测试，不要求普通业务 cpp 使用。

## 分阶段实施

### 阶段 1：收敛 any_ref API

- 增加显式 `meta::type + T*` 的类型安全模板入口；
- const 性由 `T` 自动推导；
- 将公共 `from_raw` 改为私有 `from_erased`；
- 迁移 `property_model` 等已知类型调用点；
- 保留 view、serialization 和 record 的必要擦除路径；
- 补充空地址、类型不匹配、只读和可写测试。

完成标准：普通调用者不能自行组合错误的类型、地址和 writable 标志。

### 阶段 2：object factory 自动注册

- object 子类提交时自动生成 `object_factory_ops`；
- 保持 factory、constructor 和 `allow_type_default_construction` 的既定优先级；
- 验证不可构造 object、重复 capability 和 commit 失败回滚；
- 确认 `ptr_ops`、`handle_ops` 和 `codec_ops` 仍属于各自实际值类型。

完成标准：object 基础创建能力不需要用户重复声明，且不污染包装类型 capability 边界。

### 阶段 3：补充 ODR 规范与诊断

- 在公开文档中增加统一 adapter 头规则；
- 增加 `type_model_signature`；
- pending/commit 比较同一 `type_id` 的模型签名；
- 增加 `inconsistent_type_model` 错误与差异诊断；
- 增加仅供 Debug/测试使用的 TU-local probe；
- 编写两个翻译单元特化可见性相同与不同的正反测试。

完成标准：注册或专项测试遇到不同模型时明确失败，而不是由链接器静默任选一份。

### 阶段 4：清理与验证

- 确认 `type_record` 仍是值操作和 capability 的单一事实源；
- 运行 meta、value、object、property、view 和 serialization 测试；
- Windows MSVC Debug 与 Windows Clang Debug 均使用单线程构建。

完成标准：边界改造通过相关构建和测试。

## 验证计划

- `any_ref`：模板入口、擦除入口、const 推导、错误类型和空地址；
- property：成员字段、访问器、全局变量和只读属性；
- view：sequence、set、associative、pair、tuple、variant 和 indirect；
- object：自动 factory、不可构造类型、重复注册和失败回滚；
- static capability：`ptr_ops`、`handle_ops`、`codec_ops` 和 custom capability；
- ODR 正例：两个 cpp 包含同一 adapter 头，模型签名一致；
- ODR 反例：两个 cpp 的 codec 或 builtin traits 可见性不同，TU-local probe 报告
  `inconsistent_type_model`；
- serialization：codec、字段读写、多态对象和默认构造开关。

所有构建使用 `--parallel 1`，降低当前环境发生 OOM 的风险。
