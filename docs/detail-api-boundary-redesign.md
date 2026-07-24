<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Detail API 边界收敛计划

## 文档状态

- 状态：进行中
- 当前阶段：阶段 4，收敛注册事务
- 目标：让公共 facade、稳定内部协议和模块私有实现形成单向依赖

## 背景

当前 `detail` 同时承载节点存储、模板模型、注册事务、facade 访问桥、跨模块协议和普通辅助算法。
这些内容的稳定性和抽象层级不同，但调用方只能看到统一的 `detail::` 名称。结果是 registration、
serialization 和 object 容易直接依赖 meta 的具体存储，模块内部改动也会沿公共模板头传播。

本计划不追求删除所有 `detail`，也不以目录改名作为目标。重点是收窄跨模块接口，使依赖关系变为：

```text
公共 facade / builder
        ↓
稳定的模块内协议
        ↓
节点、模型和私有算法
```

## 边界规则

1. `detail` 数据节点只由所属模块构造和解释，其他模块不得长期保存其裸指针。
2. registration 负责编排注册流程，但不调用其他模块的私有编译算法。
3. 跨模块确实需要的操作应由所属模块提供窄型提交接口，不暴露中间节点。
4. facade 的私有访问桥只负责构造视图或执行稳定操作，不提供任意内部数据提取能力。
5. 头文件模板模型与运行时存储分离，避免普通调用方包含大型 `*_data.h`。
6. 每次迁移保持公共行为、ABI 约束和性能基线不变，并补充对应测试。

## 目标分层

### 节点层

保存 registry 中稳定的运行时数据，例如 `type_data`、`type_details`、`property_data` 和
`method_data`。节点层不依赖 registration、serialization、object 或 resources。

### 模型层

根据 C++ 类型生成类型擦除操作表和静态节点，例如 value、builtin、property 和 method model。
模型层可以依赖节点层，不能依赖 registry 提交流程。

### 注册事务层

负责 pending、validate、commit 和发布后的只读查询。registration builder 只通过事务接口写入，
不直接解释 registry 节点布局。

### Facade 访问层

将稳定节点投影为 `meta::type`、`meta::property`、`meta::method` 等公开视图。跨模块快速路径应通过
窄型操作对象完成，不能通过 `data_of()` 一类接口任意取得节点。

### 模块扩展层

serialization、object 和 resources 自行准备扩展值，再把完整扩展提交给 registry。扩展的校验、
运行时缓存构建和内部生命周期均由所属模块负责。

## 分阶段计划

### 阶段 0：盘点和约束（已完成）

- 盘点公开头中 `detail` 类型的跨模块使用；
- 确认 registration 直接调用 `serialization::detail::compile_type_schema`；
- 确认 serialization 通过 `property_access::data_of()` 保存 `property_data*`；
- 明确本计划不改变 `type_data + type_details` 主结构。

### 阶段 1：收敛 serialization schema 提交（已完成）

- 由 serialization 提供完整的 schema 准备入口；
- 入口内部完成默认字段生成、校验和运行时缓存编译；
- registration 只接收准备完成的 `type_schema` 并提交 extension；
- 删除公开安装的 `serialization/detail/runtime_schema.h`；
- 保持字段缓存只编译一次的性能特征。

完成标准：registration 不再包含或调用 `serialization::detail`。

实现结果：

- 新增 `serialization::prepare_type_schema()`，统一完成默认字段生成、字段引用验证和运行时缓存编译；
- registration 仅调用稳定的 schema 准备入口并提交完整 extension；
- 删除公开安装的 `serialization/detail/runtime_schema.h`；
- Clang Debug 单线程全量构建完成，75 项测试全部通过。

### 阶段 2：收敛属性快速访问（已完成）

- 在 meta 中定义只包含字段取引用操作的窄型已编译访问器；
- serialization 缓存该访问器，不缓存 `property_data*`；
- 删除 `property_access::data_of()`；
- 保留继承属性、只读属性和 accessor property 的现有行为。

完成标准：serialization 不再包含 `meta/detail/property_data.h`，也不持有其裸指针。

实现结果：

- 新增 `meta::property_accessor` 窄型访问对象，隐藏属性节点布局并保留 unchecked 热路径；
- serialization 字段缓存改为保存 accessor，不再保存或解释 `property_data*`；
- 删除 `property_access::data_of()` 任意节点提取入口；
- 新增成员、只读、const 和可写访问测试，Clang/MSVC Debug 各 75 项全量测试全部通过；
- Release 快速复测：`to_value` 约 `0.68 us/iter`，JSON 文本读取约 `1.52 us/iter`，
  与收敛前基线持平。

### 阶段 3：分离节点与模板模型（已完成）

- 将 `property_data.h` 中的节点、ops、model 和 factory 按职责拆分；
- 将 `method_data.h` 按相同方式拆分；
- 公共 registration 模板只包含必要的 model/factory 头；
- 清理重复的 query、helper 和 access 包装。

完成标准：节点头不再包含大规模成员指针推导和调用模板，公开模板的传递包含明显减少。

实现结果：

- `property_data.h` 已只保留属性节点及其运行时操作，成员、accessor、global 模型和 factory
  已迁移到 `property_model.h`；
- 节点头体积由约 30 KB 降至约 14 KB；
- `method_data.h` 已只保留方法节点、重载集合及其运行时匹配操作，调用模型和 factory
  已迁移到 `method_model.h`；
- 方法节点头体积由约 50 KB 降至约 27 KB；
- Clang Debug 的头文件自包含、属性、方法、模块和注册构建器专项测试全部通过。

### 阶段 4：收敛注册事务（进行中）

- 将 builder 对 `pending_type_record` 内部容器的直接修改收敛为事务方法；
- 统一 type、enum、module 的 validate/commit 命名和失败语义；
- 明确 commit 前可变、commit 后 append-only 的边界；
- 评估 object factory 和 resources type entry 是否使用同一扩展提交协议。

完成标准：registration builder 不直接取得 pending 内部节点的可变指针。

当前进展：

- 属性 metadata、属性 schema、方法 metadata、默认参数、参数名及枚举项 metadata
  已改由 `pending_type_record` 的语义化事务方法修改；
- builder 不再取得对应的 `property_data*`、`method_set_data*` 或 `enum_value_data*`；
- 枚举类型级 metadata 已改由事务方法按需创建并更新 extension；
- registration builder 已不再取得 pending 内部节点的可变指针；
- Clang Debug 单线程全量构建完成，75 项测试全部通过。

### 阶段 5：验证和文档

- 运行 MSVC、Clang 的 Debug/Release 单线程全量构建与测试；
- 在 Linux 环境运行 GCC、Clang 的 Debug/Release；
- 复测 meta 查询、属性访问和完整序列化 benchmark；
- 更新架构文档并删除无调用的兼容 helper。

完成标准：公共 API 行为不变，性能不低于收敛前基线，所有可用平台测试通过。

## 非目标

- 不把所有内部类型提升为公共 API；
- 不为追求目录整齐进行一次性大规模重命名；
- 不改变 static capability、type identity 或 registry 的既有语义；
- 不在本计划中承诺跨 DLL 静态节点地址统一。
