<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Roadmap

CAORS 后续按核心方向推进。每个方向应先补最小可用 API、测试和文档，再逐步扩展性能优化与工具链集成。

## 0. Core 基础设施

- [x] 提供 `string_name`，用于运行期名称去重、快速比较和查找。
- [x] 增加 `guid` / `type_guid`，用于序列化和资源引用的稳定标识。
- [x] 增加统一错误码、错误来源域和 `std::error_code` 适配。
- [x] 统一 meta、object、reflection 注册 API 返回 `caors::error`，移除旧注册结果枚举兼容层。
- [x] 增加轻量 `result<T>`，统一携带值的错误返回结构。
- [x] 提供与 CAORS 内存系统适配的 `string` / `vector` / `hash_map` / `hash_set` 基础容器别名。
- [x] 提供与 CAORS 内存系统适配的 `list` 基础容器别名。
- [ ] 在 core 主题下逐步补充 small_vector、stable_vector 等基础容器。

## 1. 类型元数据底座

- [x] 定义 `type_id`、`meta_type_data`、`meta_type` 和基础类型注册错误状态。
- [x] 在 `meta_registry` 中集中处理运行期 id 分配、重名、重复 GUID、不一致类型描述和父类型缺失。
- [x] 提供 `meta_registry`，允许围绕同一个 `type_id` 挂载对象、反射、序列化等元数据组件。
- [x] 提供 `meta_view<T...>()`，查询同时拥有指定元数据组件的类型。
- [x] 补充反射和序列化组件的跨模块查询测试。
- [ ] 评估组件替换、批量注册和错误诊断 API。

## 2. 定制化分配器

- [x] 定义统一的内存资源接口，明确分配、释放、对齐和错误处理语义。
- [x] 实现基础后端封装：system 后端和 callback 后端。
- [x] 提供 `try_allocate()` 不抛异常入口，并保留 `allocate()` 包装语义。
- [x] 提供 `allocator<T>`、`make<T>()` 和 `destroy<T>()` 基础入口。
- [x] 实现 `tracking_resource`、`monotonic_resource` 和 `fixed_block_pool`。
- [x] 实现 `debug_tracking_resource`，支持存活分配、调用点标签和错误释放统计。
- [ ] 扩展栈式分配器、分级 size class 池等上层策略。
- [ ] 扩展越界检测、填充值和 guard block 等调试能力。
- [ ] 评估与 `std::pmr` 的可选适配方式。
- [ ] 增加跨平台压力测试和边界用例测试。

## 3. 对象系统

- [x] 定义基础对象类型、对象标识和类型描述。
- [x] 提供轻量级类型层级和运行时查询能力。
- [x] 抽象共享 `meta_registry`，避免对象系统和反射系统重复维护基础类型索引。
- [x] 对象注册表改为基于 `meta_registry` 挂载 `object_type_data` 组件，并通过 `object_type` 视图对外查询。
- [x] 提供对象注册表和默认构造工厂。
- [x] 提供注册错误状态，区分重复注册、重名、不一致类型描述和父类型缺失。
- [x] 接入 `memory_resource` 完成对象创建和销毁。
- [ ] 设计对象 handle、weak handle 和所有权约束。
- [ ] 增加对象生命周期管理器。
- [ ] 明确线程安全边界，避免隐式全局状态。
- [ ] 增加对象生命周期、继承关系和错误路径测试。

## 4. 反射

- [x] 定义类型和字段的基础元信息模型。
- [x] 基于 `meta_registry` 挂载 `reflection_type_data` 组件。
- [x] 设计手写注册 API，并为后续代码生成保留扩展空间。
- [x] 提供运行时类型查询和字段元信息查询入口。
- [x] 提供字段动态 get/set 入口。
- [x] 支持基于 `sequence_traits<T>` 的顺序容器字段描述、元素类型查询和基础修改入口。
- [x] 支持 `caors::list<T>` / `std::list<T>` / `std::forward_list<T>` 等非连续顺序容器类型反射描述。
- [x] 支持基于 `set_traits<T>` 的集合容器字段描述和元素类型查询。
- [x] 支持基于 `associative_traits<T>` 的 hash map 字段描述和 key/value 类型查询。
- [x] 支持 `std::array<T,N>`、`std::deque<T>`、`std::forward_list<T>`、`std::set<T>` 和 `std::map<K,V>` 的默认 traits 适配。
- [x] 支持 `std::pair<T,U>` 的二元组字段描述、first/second 类型查询和基础修改入口。
- [x] 支持 `std::tuple<Ts...>` 的 tuple-like 字段描述、元素类型查询和基础修改入口。
- [ ] 定义函数和属性的元信息模型。
- [ ] 提供函数调用入口。
- [ ] 增加反射注册、查询、访问控制和类型转换测试。

## 5. 序列化

- [x] 定义统一的序列化接口和错误模型。
- [x] 基于 `meta_registry` 挂载序列化元信息组件。
- [x] 提供字段级序列化配置，支持字段别名、跳过和注册顺序解耦。
- [x] 设计 writer 后端接口，为文本、二进制和 msgpack 等格式预留扩展点。
- [x] 与反射系统打通，实现基于元信息的基础字段文本序列化。
- [x] 增加 `variant` 中间对象树，支持对象、数组、标量和字段 id/name，并保留 streaming writer 快路径。
- [x] 补充 `variant` 安全访问器、数组/对象 helper 和 `any` / `any_ref` 到 `variant` 的转换入口。
- [x] 拆出 `value` 模块，提供 `any` C++ 类型值和 `variant` 数据值。
- [x] 增加 `any_ref`，支持反射字段零拷贝引用访问和不可拷贝字段读取。
- [x] 增加 `serialization_reader` 后端接口和 DOM reader，支持基础字段反序列化。
- [x] 增加反序列化缺失字段策略、未知字段默认忽略和基础 schema version 字段过滤。
- [x] 增加类型级 serialization codec 扩展点，并内置 string、string_view、guid 和 type_guid codec。
- [x] 支持值类型嵌套结构递归序列化和 DOM 反序列化。
- [x] 支持 `caors::vector<T>` 数组字段，覆盖标量元素和已注册结构体元素。
- [x] 序列化 vector 字段不再要求手动注册 vector 类型名。
- [x] 支持 `caors::object_ref<T>` 对象引用字段和 resolver。
- [x] 增加 JSON writer、JSON DOM parser 和 `serialize_json()` / `deserialize_json()`。
- [x] 增加 `variant::dictionary` 和 `caors::hash_map<K, V>` 字典 codec。
- [x] 抽出基于 `sequence_traits<T>` 和 `associative_traits<T>` 的通用容器 codec。
- [x] 支持基于 `set_traits<T>` 的 `caors::hash_set<T>` 集合 codec。
- [x] 支持基于 `sequence_traits<T>` 的 `caors::list<T>` 数组 codec。
- [x] 支持裸指针、普通 wrapper、`object_ref<T>` 和 `null` 边界处理。
- [x] 支持 `std::array<T,N>`、`std::deque<T>`、`std::forward_list<T>`、`std::set<T>` 和 `std::map<K,V>` 的默认序列化适配。
- [x] 支持 `std::pair<T,U>` 按二元素数组序列化和反序列化。
- [x] 支持 `std::tuple<Ts...>` 按元素数组序列化和反序列化。
- [x] 支持 `std::variant<Ts...>` 按 `[index, value]` tagged union 序列化和反序列化。
- [x] 支持 `std::span<T>` / `std::span<T,N>` 作为借用数组视图反射访问和序列化写出，默认禁止反序列化写回。
- [ ] 为 `std::string_view` 设计安全反序列化策略，避免悬空引用。
- [ ] 为 `std::weak_ptr<T>`、对象 handle 和资源引用设计稳定引用语义。
- [ ] 支持多态对象的动态类型记录、构造和安全下转型。
- [ ] 为不可默认构造类型设计显式 factory、placement 构造或字段级构造策略。
- [ ] 评估是否放宽多层指针 / 多层 wrapper 限制，或提供清晰诊断与显式适配点。
- [ ] 增加紧凑二进制和 msgpack 等二进制后端。
- [x] 增加版本兼容、缺失字段和未知字段基础测试。
- [ ] 增加跨平台字节序测试。
