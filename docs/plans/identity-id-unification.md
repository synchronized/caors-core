<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Identity ID 收敛计划

## 文档状态

- 当前状态：type 与 capability 已落地，早期 type extension ID 已删除，identity 调试查询已提供；
- 目标：统一 type、capability 等进程内 ID 的底层发号逻辑；
- 非目标：不把所有公开 API 改成同一个万能 ID，不把 identity state 变成 registry。

## 背景

当前 `type_id` 和 `capability_key` 由 CAORS runtime 按 domain 与 canonical C++ 类型名分配
进程内递增整数。该改造替代了早期 `const void*` 模板静态 key 方案，解决了插件和跨 DLL
场景下模板静态地址可能不一致的问题。早期 type extension / `extension_key` 机制已删除，类型附加数据统一走
capability data 或 metadata。

仍需注意：

- 这些 ID 只在同一 runtime 进程内有效，不是持久化或跨版本协议 ID；
- capability 的名称、GUID、版本仍用于诊断和未来稳定插件协议；
- 插件 host extension 是独立的 C ABI 服务查询机制，使用字符串 ID 和版本，不使用当前 identity state；
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

它不保存 type record、capability 数据或模块生命周期信息，不承担 registry 职责。

### 外部保持强类型 ID

公开和模块内部 API 不直接传万能 `identity_id`。外层继续使用不同强类型：

```cpp
class type_id;
class capability_id;
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
```

### Domain 明确参与 identity key

同一个 canonical name 在不同 domain 下不是同一个身份：

```text
(type, "caors.foo")       != (capability, "caors.foo")
```

实现可以选择单个 store，key 使用 `(domain, canonical_name)`；也可以在 store 内部按 domain 分表。
逻辑上必须保留 domain 边界，避免 type/capability 同名合并。

### 数字空间

推荐每个 domain 独立递增：

```text
type:       "caors.foo" -> type_id{1}
capability: "caors.foo" -> capability_id{1}
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

当前模板入口：

```cpp
template <typename T>
[[nodiscard]] capability_key capability_key_of() noexcept;
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
- [x] 文档说明 capability id 是进程内 ID，capability GUID / version 仅用于诊断、工具链和协议协商说明，
  不表示 CAORS 提供跨版本 stable capability ABI。

完成标准：动态 capability 不再依赖模板静态地址作为 key。

### 阶段 3：删除 type extension ID

- [x] 删除早期 `extension_id` / `extension_key` 强类型；
- [x] 删除 registry type extension 存储、查询和测试；
- [x] 文档明确类型附加数据统一使用 capability data 或 metadata；
- [x] 保留插件 host extension，它是 C ABI 宿主服务查询机制，不是 `meta::type` 数据槽。

完成标准：公开 API 不再暴露 type extension 机制。

### 阶段 4：诊断与未来扩展

- [x] 为 identity store 增加按 domain/name/id 的调试查询；
- [x] 为 capability 预留 name、GUID、version 元信息挂接点；
- [x] 评估新增 `schema_id`、`event_id` 或 `resource_type_id` 是否复用同一发号器；
- [x] 更新跨 DLL 文档，说明进程内 ID 与稳定 GUID 的边界。

完成标准：identity 机制可以作为后续模块 ID 的统一底座。

评估结论：仅当新 ID 表达“进程内、按 canonical name 唯一”的身份时复用 identity state，并继续
使用独立强类型封装；持久化或跨进程身份仍使用 GUID 或外部协议 token。当前不为尚未出现的
`schema_id`、`event_id` 或 `resource_type_id` 预先分配公开类型。

## 风险

- 如果某处仍裸传 `uint64_t`，会削弱强类型 ID 的边界；
- ID 仍是进程内身份，不应替代持久化 GUID；
- 插件 host extension 返回的指针仍受插件 ABI 和生命周期约束。
