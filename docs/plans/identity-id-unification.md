<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Identity ID 收敛计划

## 文档状态

- 当前状态：阶段 1-3 已落地，阶段 4 诊断查询保留后续；
- 目标：统一 type、capability、extension 等进程内 ID 的底层发号逻辑；
- 非目标：不把所有公开 API 改成同一个万能 ID，不把 identity state 变成 registry。

## 背景

当前 `type_id`、`capability_key` 和 `extension_key` 都由 CAORS runtime 按 domain 与 canonical
C++ 类型名分配进程内递增整数。该改造替代了早期 `const void*` 模板静态 key 方案，解决了插件和
跨 DLL 场景下模板静态地址可能不一致的问题。

仍需注意：

- 这些 ID 只在同一 runtime 进程内有效，不是持久化或跨版本协议 ID；
- capability 的名称、GUID、版本仍用于诊断和未来稳定插件协议；
- 插件 extension 的 value 构造、复制和析构代码仍可能来自插件 DLL，需要独立生命周期策略；
- 后续新增 event、schema、resource type 等 ID 时应复用同一发号器模式。

## 设计原则

### Identity State 只做发号器

内部提供一个纯粹的 identity 发号器：

```text
(domain, canonical_name) -> uint64_t
```

它只负责：

- 保存 canonical name 副本；
- 按 domain 查询或分配进程内整数 ID；
- 保证同一 runtime 内同一 domain/name 返回同一数字；
- 提供必要的诊断查询。

它不保存 type record、capability 数据、extension 数据或模块生命周期信息，不承担 registry 职责。

### 外部保持强类型 ID

公开和模块内部 API 不直接传万能 `identity_id`。外层继续使用不同强类型：

```cpp
class type_id;
class capability_id;
class extension_id;
```

未来新增 ID 也按同样模式扩展：

```cpp
class event_id;
class schema_id;
class resource_type_id;
```

这样可以复用底层发号逻辑，同时保持函数签名的语义约束：

```cpp
meta::type find(type_id id);
auto get_capability(capability_id id);
auto find_extension(extension_id id);
```

### Domain 明确参与 identity key

同一个 canonical name 在不同 domain 下不是同一个身份：

```text
(type, "caors.foo")       != (capability, "caors.foo")
(capability, "caors.foo") != (extension, "caors.foo")
```

实现可以选择单个 store，key 使用 `(domain, canonical_name)`；也可以在 store 内部按 domain 分表。
逻辑上必须保留 domain 边界，避免 type/capability/extension 同名合并。

### 数字空间

推荐每个 domain 独立递增：

```text
type:       "caors.foo" -> type_id{1}
capability: "caors.foo" -> capability_id{1}
extension:  "caors.foo" -> extension_id{1}
```

因为强类型 ID 已经表达 domain，跨 domain 的 raw value 重复不是问题。这样调试时也更容易理解某个 ID
在自身 domain 内的顺序。

如果实现上先使用全局递增空间也可以接受，但不应让调用方依赖 raw value 跨 domain 唯一。

## 目标 API 草案

内部 detail 层：

```cpp
enum class identity_domain {
  type,
  capability,
  extension,
};

[[nodiscard]] uint64_t get_or_create_identity_id(identity_domain domain,
                                                 const char* canonical_name,
                                                 std::size_t length) noexcept;
```

公开强类型：

```cpp
class capability_id {
public:
  [[nodiscard]] constexpr std::uint64_t value() const noexcept;
  [[nodiscard]] explicit constexpr operator bool() const noexcept;
};

class extension_id {
public:
  [[nodiscard]] constexpr std::uint64_t value() const noexcept;
  [[nodiscard]] explicit constexpr operator bool() const noexcept;
};
```

当前模板入口：

```cpp
template <typename T>
[[nodiscard]] capability_key capability_key_of() noexcept;

template <typename T>
[[nodiscard]] detail::extension_key detail::extension_key_of() noexcept;
```

`type_id` 保持现有公开语义，只把内部实现迁移到统一 identity state。

## 分阶段实施

### 阶段 1：抽出通用 identity state

- [x] 从当前 `type_identity_state` 抽出 domain-aware identity store；
- [x] 保持 `get_or_create_type_id()` 对外行为不变；
- [x] 保持 type id 分配和 canonical name 查询测试通过；
- [x] 不改变 registry 与 type record 的职责边界。

完成标准：`type_id` 测试保持通过，内部实现已经可复用。

### 阶段 2：引入 `capability_id`

- [x] 新增 `capability_id` 强类型；
- [x] 将 `capability_key` 从 `const void*` 迁移到 `capability_id`；
- [x] `capability_key_of<T>()` 改为基于 identity domain `capability`；
- [x] 更新 `type_record::capabilities`、动态 capability 注册、查询和测试；
- [x] 文档说明 capability id 是进程内 ID，跨版本稳定协议仍应使用 capability GUID / version。

完成标准：动态 capability 不再依赖模板静态地址作为 key。

### 阶段 3：引入 `extension_id`

- [x] 新增 `extension_id` 强类型；
- [x] 将 `extension_key` 从 `const void*` 迁移到 `extension_id`；
- [x] `extension_key_of<T>()` 改为基于 identity domain `extension`；
- [x] 更新 registry extension 存储、查询和测试；
- [x] 明确 C++ 模板 extension 仍不等于插件稳定 ABI，插件扩展需要后续稳定协议。

完成标准：extension key 不再依赖模板静态地址作为 key。

### 阶段 4：诊断与未来扩展

- [ ] 为 identity store 增加按 domain/name/id 的调试查询；
- [x] 为 capability 预留 name、GUID、version 元信息挂接点；
- [ ] 评估新增 `schema_id`、`event_id` 或 `resource_type_id` 是否复用同一发号器；
- [x] 更新跨 DLL 文档，说明进程内 ID 与稳定 GUID 的边界。

完成标准：identity 机制可以作为后续模块 ID 的统一底座。

## 风险

- 如果某处仍裸传 `uint64_t`，会削弱强类型 ID 的边界；
- ID 仍是进程内身份，不应替代持久化 GUID；
- 插件 extension 即使 key 稳定，value 的构造/析构代码仍可能来自插件 DLL，仍需独立生命周期策略。
