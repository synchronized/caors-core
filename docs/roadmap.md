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
- [x] 建立 `io` 主题，提供同步字节流、文件系统抽象和本地文件系统实现。
- [x] 增加 `io::path_view` / `io::path`，明确虚拟路径和宿主文件系统路径的边界。
- [x] 为 `io::native_file_system` 增加可选 root 目录，使 mount 层只负责虚拟路径分发和可选子路径重映射。
- [ ] 在 core 主题下逐步补充 small_vector、stable_vector 等基础容器。

## 1. 类型元数据底座

- [x] 定义 `type_id`、`meta::type` 和基础类型注册错误状态。
- [x] 在 `meta::registry` 中集中处理运行期类型索引、重名、重复 GUID、不一致类型描述和父类型缺失。
- [x] 提供 `meta::registry`，允许围绕同一个 `type_id` 挂载对象、反射、序列化等元数据组件。
- [x] 提供 `meta::view<T...>()`，查询同时拥有指定元数据组件的类型。
- [x] 补充反射和序列化组件的跨模块查询测试。
- [x] 增加组件 `try_emplace` / `ensure` 语义和组件状态诊断 API。
- [ ] 设计开放 runtime capability / poly concept table，用于外挂用户自定义运行期访问能力，并与内置 `type_descriptor` capability 并行。
- [ ] 梳理 `type_descriptor`、serialization codec 和 runtime capability / poly concept table 的职责边界：
  descriptor 保留内置布局、构造、容器和 wrapper 能力，codec 负责格式读写，runtime capability 作为用户扩展能力入口。
- [ ] 评估组件替换和批量注册 API。

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
- [x] 抽象共享 `meta::registry`，避免对象系统和反射系统重复维护基础类型索引。
- [x] 对象注册表改为基于 `meta::registry` 挂载 `objects::detail::type_data` 组件，并通过 `objects::type` 视图对外查询。
- [x] 提供对象注册表和默认构造工厂。
- [x] 提供注册错误状态，区分重复注册、重名、不一致类型描述和父类型缺失。
- [x] 接入 `memory_resource` 完成对象创建和销毁。
- [x] 设计 `objects::handle`、`objects::typed_handle<T>` 和所有权约束。
- [x] 增加对象生命周期管理器。
- [x] 设计对象引用计数机制边界，明确侵入式引用计数、弱引用、manager 托管对象和序列化引用之间的关系。
- [x] 实现可选侵入式引用计数基础类型和强引用包装。
- [x] 实现引用计数弱引用控制信息和 `objects::weak_ref<T>`。
- [x] 设计并实现 manager 延迟销毁策略，明确它和引用计数对象的边界。
- [x] 增加全局运行期 `object_id` 生成器，避免多个 manager 生成重复运行期 id。
- [x] 增加 `object_guid` 语义别名和 `object_index` 双向索引，用于持久对象引用映射。
- [x] manager 可选联动 `object_index`，支持按持久 GUID 创建、查找和销毁解绑。
- [x] 明确线程安全边界，避免隐式全局状态。
- [x] 设计持久对象 GUID / 业务对象 id，用于跨存档、资源和网络引用。
- [x] 增加对象生命周期、继承关系和错误路径测试。

## 4. 反射

- [x] 定义类型和字段的基础元信息模型。
- [x] 基于 `meta::registry` 挂载内部反射组件，并通过反射视图对外查询。
- [x] 支持基于已注册 `meta::type` 挂载反射组件，减少反射层隐式类型注册。
- [x] 设计手写注册 API，并为后续代码生成保留扩展空间。
- [x] 提供运行时类型查询和字段元信息查询入口。
- [x] 提供字段动态 get/set 入口。
- [x] 支持基于 `traits::sequence_traits<T>` 的顺序容器字段描述、元素类型查询和基础修改入口。
- [x] 支持 `caors::list<T>` / `std::list<T>` / `std::forward_list<T>` 等非连续顺序容器类型反射描述。
- [x] 支持基于 `traits::set_traits<T>` 的集合容器字段描述和元素类型查询。
- [x] 支持基于 `traits::associative_traits<T>` 的 hash map 字段描述和 key/value 类型查询。
- [x] 支持 `std::array<T,N>`、`std::deque<T>`、`std::forward_list<T>`、`std::set<T>` 和 `std::map<K,V>` 的默认 traits 适配。
- [x] 支持 `std::pair<T,U>` 的二元组字段描述、first/second 类型查询和基础修改入口。
- [x] 支持 `std::tuple<Ts...>` 的 tuple-like 字段描述、元素类型查询和基础修改入口。
- [x] 定义成员方法元信息模型；字段继续承担数据属性反射。
- [x] 提供成员方法调用入口。
- [ ] 扩展属性别名、访问控制、重载选择和类型转换测试。

## 5. 序列化

- [x] 定义统一的序列化接口和错误模型。
- [x] 基于 `meta::registry` 挂载序列化元信息组件。
- [x] 支持基于已注册 `meta::type` 挂载序列化组件，减少序列化层隐式类型注册。
- [x] 提供字段级序列化配置，支持字段别名、跳过和注册顺序解耦。
- [x] 设计 writer 后端接口，为文本、二进制和 msgpack 等格式预留扩展点。
- [x] 与反射系统打通，实现基于元信息的基础字段文本序列化。
- [x] 增加 `variant` 中间对象树，支持对象、数组、标量和字段 id/name，并保留 streaming writer 快路径。
- [x] 补充 `variant` 安全访问器、数组/对象 helper 和 `any` / `any_ref` 到 `variant` 的转换入口。
- [x] 拆出 `value` 模块，提供 `any` C++ 类型值和 `variant` 数据值。
- [x] 增加 `any_ref`，支持反射字段零拷贝引用访问和不可拷贝字段读取。
- [x] 为 `value::any` 评估并实现小对象优化，优先减少标量、小型平凡对象和常见临时值的堆分配，同时保持
  `memory_resource` 构造和销毁语义一致。
- [x] 增加 `serialization::reader` 后端接口和 DOM reader，支持基础字段反序列化。
- [x] 增加反序列化缺失字段策略、未知字段默认忽略和基础 schema version 字段过滤。
- [x] 增加类型级 serialization codec 扩展点，并内置 string、string_view、guid 和 type_guid codec。
- [x] 支持值类型嵌套结构递归序列化和 DOM 反序列化。
- [x] 支持 `caors::vector<T>` 数组字段，覆盖标量元素和已注册结构体元素。
- [x] 序列化 vector 字段不再要求手动注册 vector 类型名。
- [x] 支持 `caors::objects::ptr<T>` 对象指针字段和 resolver。
- [x] 增加 JSON writer、JSON DOM parser 和 `serialize_json()` / `deserialize_json()`。
- [x] 为文本和 JSON writer 增加外部输出 buffer / `serialize_*_to()` 入口，允许调用方复用
  `caors::string` 并减少结果拷贝，后续可接入指定 `memory_resource`。
- [ ] 扩展 JSON parser-side options，例如最大嵌套深度、重复 key 策略、尾随逗号开关和完整
  Unicode 转义处理；当前 `json_options` 只影响 writer 格式化。
- [x] 增加 `variant::dictionary` 和 `caors::hash_map<K, V>` 字典 codec。
- [x] 抽出基于 `traits::sequence_traits<T>` 和 `traits::associative_traits<T>` 的通用容器 codec。
- [x] 支持基于 `traits::set_traits<T>` 的 `caors::hash_set<T>` 集合 codec。
- [x] 支持基于 `traits::sequence_traits<T>` 的 `caors::list<T>` 数组 codec。
- [x] 支持裸指针、普通 wrapper、`objects::ptr<T>` 和 `null` 边界处理。
- [x] 支持 `objects::ptr<T>` 通过持久 `object_guid` resolver 写出和读回。
- [x] 支持 `std::array<T,N>`、`std::deque<T>`、`std::forward_list<T>`、`std::set<T>` 和 `std::map<K,V>` 的默认序列化适配。
- [x] 支持 `std::pair<T,U>` 按二元素数组序列化和反序列化。
- [x] 支持 `std::tuple<Ts...>` 按元素数组序列化和反序列化。
- [x] 支持 `std::variant<Ts...>` 按 `[index, value]` tagged union 序列化和反序列化。
- [x] 支持 `std::span<T>` / `std::span<T,N>` 作为借用数组视图反射访问和序列化写出，默认禁止反序列化写回。
- [x] 为 `std::string_view` 设计安全反序列化策略，避免悬空引用。
- [x] 将 `serialization::context::string_view_storage` 从单个字符串存储升级为多段
  `string_storage` / `string_buffer`，支持同一对象内多个 `std::string_view` 字段安全读回。
- [x] 为 `std::weak_ptr<T>`、对象句柄和资源引用设计稳定引用语义。
- [x] 支持 `resource_guid` 和 `resource_ref<T>` 按稳定 GUID 读写；默认反序列化后资源引用保持未解析状态，也可通过 context resolver 立即解析。
- [x] 支持多态对象的动态类型记录、manager 构造和安全下转型。
- [x] 支持普通 C++ 类在 `std::unique_ptr<Base>` / `caors::memory::unique_ptr<Base>` /
  `std::shared_ptr<Base>` 字段中的动态类型记录、默认构造和安全下转型。
- [x] 为普通类多态值补充显式 factory 和可选 `memory_resource` 构造策略。
- [ ] 设计字段级构造策略，覆盖容器新增元素、空 `optional<T>`、`variant` 候选切换和不可默认构造类型；
  优先使用 `serialization::context::memory_resource`、meta factory 和字段策略，失败时返回明确诊断。
- [ ] 放宽多层指针 / 多层 wrapper 限制，采用逐层 indirect descriptor 展开；默认设置合理最大深度，
  并对空中间层、不可写引用和所有权不明确的组合返回清晰诊断。
- [ ] 设计 schema 策略，覆盖版本迁移、默认值恢复、未知字段保留、严格模式和字段重命名 / 删除策略。
- [ ] 设计弱引用序列化策略，默认继续不展开 `std::weak_ptr<T>` / `objects::weak_ref<T>`，
  提供基于 `objects::handle`、`resource_guid` 或自定义 resolver 的显式弱引用策略。
- [ ] 增加紧凑二进制和 msgpack 等二进制后端。
- [x] 增加版本兼容、缺失字段和未知字段基础测试。
- [ ] 增加跨平台字节序测试。

## 6. 资源系统

- [x] 建立 engine-agnostic 资源核心层文档边界：资源表示可持久化、可缓存、可复用资产，对象表示当前运行期实例。
- [x] 定义 `resources::resource`、`resource_guid` 和 `resources::ref<T>` 基础身份与轻量引用模型。
- [x] 提供非拥有 `resources::manager` 资源索引，支持按 `resource_guid` 绑定、查找、解绑、清空和解析 `resources::ref<T>`。
- [x] 提供 `resources::cache<T>` 拥有式资源缓存，支持按 `resource_guid` 创建、持有、释放、销毁和解析资源引用。
- [x] 支持 `resources::ref<T>` 通过序列化 codec 按稳定 `resource_guid` 读写，读回后可由 manager/cache 显式解析，也可通过序列化 context resolver 按策略解析。
- [x] 设计 `resources::loader<T>` / `resources::saver<T>` 接口，并提供基于 serialization reader/writer 的最小读写 helper。
- [x] 支持序列化遍历收集 `resource_ref<T>` 资源依赖，用于后续预加载、打包、热更新和资源数据库。
- [x] 打通资源依赖收集到 manifest / provider / loader / cache 预加载，再由序列化 resolver 解析
  `resource_ref<T>` 的同步链路。
- [ ] 扩展资源加载、卸载和失效处理策略，并明确生命周期与线程安全边界。
- [x] 提供 `core::locator<T>` 可选全局服务定位器，用于定位默认资源管理器、文件系统、配置服务等进程级服务，并明确注册、重置和测试隔离规则。
- [x] 设计底层 `io::file_system` 字节来源抽象，统一本地文件、内存文件、资源包和远程网络资源访问的接口边界。
- [x] 设计资源层 `resources::source` / `resources::provider`，负责从 `resource_guid` 或资源位置解析到统一数据流，再交给 loader 解码。
- [x] 实现本地文件系统后端 `io::native_file_system`。
- [x] 实现 `resources::file_source`，把 `io::file_system` 适配为资源字节来源。
- [x] 实现 `resources::json_loader<T>`，打通 `resource_guid -> provider -> JSON -> resource` 的同步加载链路。
- [x] 实现内存文件系统后端 `io::memory_file_system`，用于测试、热更新缓存和后续资源包索引内存镜像。
- [x] 增加 `resources::manifest`，保存 `resource_guid -> location/type/hash/version` 清单，并可批量绑定到 provider。
- [x] 支持从 JSON 文本或 `resources::source` 导入资源 manifest，作为本地资源数据库和调试 fileserver 清单入口。
- [x] 支持 manifest 条目更新和 provider 批量同步，方便调试 fileserver 或资源数据库刷新已有资源位置。
- [x] 支持 manifest 全量替换 provider 绑定，方便完整资源清单刷新时移除旧资源位置。
- [x] 支持 `resources::provider` 通过可写 source 按 GUID 写回字节，为后续 saver 和热更新回写提供基础。
- [x] 实现 `resources::json_saver<T>`，打通 `resource -> JSON -> provider -> source` 的同步保存链路。
- [x] 提供 `resources::save_from()`，把 cache 中指定 GUID 的资源交给 saver 保存。
- [ ] 实现资源包、远程资源或异步 loader/saver 后端。
- [x] 打通资源引用序列化 resolver，支持反序列化后按策略解析稳定 GUID 引用。
- [ ] 设计资源热重载边界：第一阶段放在资源系统主题内，不新增独立主题；热重载只负责发现资源描述或字节内容变化、
  刷新 manifest / provider / cache，并返回 reload 结果，不隐式修改对象、场景树或运行期业务状态。
- [ ] 支持基于 manifest / provider / cache 的同步热重载流程：检测 `hash`、`version` 或 `location` 变化，
  重新通过匹配 loader 加载资源，并按 cache 策略替换旧资源或保留旧资源。
- [ ] 设计热重载失败策略和通知机制：缺资源、加载失败、类型不匹配或替换失败时保留旧资源，返回可诊断错误；
  后续可接入 `events::signal` 或上层编辑器通知。
- [ ] 后续评估调试 fileserver、资源包、远程资源、导入管线、资源数据库和编辑器工具层；当这些能力需要独立
  生命周期、后台任务或 UI/工具协议时，再考虑拆出独立资源工具链主题。

## 7. 事件与委托

- [ ] 设计类似 EnTT 的轻量 `events::delegate`，支持自由函数、成员函数、lambda / 函数对象绑定，并明确所有权和生命周期限制。
- [ ] 设计 `events::signal` / `events::sink`，支持类型安全订阅、取消订阅和同步触发。
- [ ] 设计 `events::dispatcher`，支持按事件类型入队、批量派发和清空队列，并接入 CAORS 容器与 `memory_resource`。
- [ ] 明确事件系统和对象系统的关系：底层事件模块不隐式持有对象，涉及对象生命周期时由 `objects::ptr<T>`、`objects::handle` 或上层系统显式管理。
- [ ] 评估线程安全事件队列、跨线程投递和异步调度策略；第一阶段默认只保证单线程使用边界。
- [ ] 补充委托连接、断开、事件顺序、递归派发和监听器失效等边界测试。
