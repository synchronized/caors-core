<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Roadmap

CAORS 后续按核心方向推进。每个方向应先补最小可用 API、测试和文档，再逐步扩展性能优化与工具链集成。

## 0. Core 基础设施

- [x] 提供 `string_name`，用于运行期名称去重、快速比较和查找。
- [x] 增加 `guid` / `type_guid`，用于序列化和资源引用的稳定标识。
- [x] 增加统一错误码、错误来源域和 `std::error_code` 适配。
- [x] 统一 meta、object、serialization 注册 API 返回 `caors::error`，移除旧注册结果枚举兼容层。
- [x] 增加轻量 `result<T>`，统一携带值的错误返回结构。
- [x] 提供与 CAORS 内存系统适配的 `string` / `vector` / `hash_map` / `hash_set` 基础容器别名。
- [x] 提供与 CAORS 内存系统适配的 `list` 基础容器别名。
- [x] 建立 `io` 主题，提供同步字节流、文件系统抽象和本地文件系统实现。
- [x] 增加 `io::path_view` / `io::path`，明确虚拟路径和宿主文件系统路径的边界。
- [x] 为 `io::native_file_system` 增加可选 root 目录，使 mount 层只负责虚拟路径分发和可选子路径重映射。
- [x] 为 `io::file_system` 增加 `open_read()` / `open_write()` 流入口；第一版 `open_read()` 可通过
  `read_all()` 适配已有后端，`open_write()` 默认返回 `unsupported`，等待显式 close/flush 语义收敛。
- [x] 为可写输出流补充 `write_all()`、`flush()`、`close()` 和提交失败语义；默认
  `open_write()` 使用内存缓冲并在显式 `close()` 时调用 `write_all()` 提交，具体后端后续可覆盖为真正分块写入。
- [x] 将 `io::native_file_system::open_read()` / `open_write()` 覆盖为宿主文件流，避免本地大文件路径先完整读入或写入内存缓冲。
- [x] 为 `io::file_system` 增加 `status()` 基础属性查询和 `list()` 非递归目录枚举，覆盖 native /
  memory / mount 后端，为编辑器资产浏览和资源扫描提供底层入口。
- [x] 提供基于 `file_system::list()` 的通用 `io::walk()` 递归遍历 helper，先覆盖同步扫描场景。
- [x] 设计 `io::uri` / `io::uri_view`，用 URI scheme（例如 `caors://`、`file://`、`http://`、
  `pack://`）表达逻辑资源位置和后端路由边界；MIME / content-type 只用于描述内容格式，不负责路径路由。
- [x] 增加 `io::file_system_resolver` 或 virtual file system 路由层，根据 scheme、authority 或 mount prefix
  把逻辑 URI 映射到 native、memory、HTTP、资源包、manifest 等具体后端。
- [x] 增加 `io::file_system_capabilities`，显式声明 read / write / list / status / watch / seek 等能力；
  上层不根据 scheme 猜测某个后端是否支持目录枚举或监听。
- [x] 设计 manifest-backed listing overlay：当 HTTP、CDN 或远程后端不能直接 `list()` 时，由 manifest
  或资源数据库提供目录枚举和编辑器资源树。
- [x] 完善 VFS 分层约定：`io::mount_file_system` 保持纯路径前缀转发，`io::file_system_resolver`
  作为 URI VFS 负责 scheme / authority / 路径前缀路由；manifest `location` 可保存相对路径或
  `caors://`、`file://`、`http://`、`pack://` 等 URI，最终解析统一交给 resolver / provider 层。
- [x] 将 `result<T>` 逐步收敛为更接近 `expected<T, error>` 的语义，减少 `value/error/status`
  之间的重复表达。
- [x] 清理根命名空间下不常用或不推荐继续使用的别名，只保留高频便捷别名，降低 API 表面噪声；
  序列化模块已先行收敛到 `serialize` / `deserialize` / `to_value` / `to_string`，资源模块低层
  source/provider/loader/request/config/helper 别名已收敛回 `caors::resources` 命名空间。
- [ ] 在 core 主题下逐步补充 small_vector、stable_vector 等基础容器。

## 0.5. Math 基础数学类型

- [x] 增加 `math` 主题和聚合头 `caors/math.h`。
- [x] 增加基础向量模板 `math::vec2_t<T>`、`math::vec3_t<T>`、`math::vec4_t<T>`，并提供
  `vec*` / `dvec*` / `ivec*` / `uvec*` 常用别名。
- [x] 增加基础四元数模板 `math::quat_t<T>`，并提供 `quat` / `dquat` 常用别名。
- [x] 增加基础矩阵模板 `math::mat3_t<T>`、`math::mat4_t<T>`，并提供 `mat*` / `dmat*` 常用别名。
- [x] 增加基础二维矩形 `math::rect_t<T>` 和三维 AABB `math::aabb_t<T>`，并提供 float/double/int 常用别名。
- [x] 增加基础常量、角度转换、clamp、lerp、smoothstep 和组件级 min/max/abs helper。
- [x] 增加基础 transform helper：translation、scaling、rotation、TRS、点/向量/方向变换。
- [x] 增加投影矩阵 helper：`look_at`、`perspective`、`orthographic`，底层通过
  `handedness` / `clip_depth` 模板参数表达 RH/LH 和 ZO/NO 约定，同时保留 `*_rh_zo`
  等便捷函数。
- [x] 增加基础几何 helper：`ray_t<T>`、`sphere_t<T>`、`plane_t<T>`、AABB
  合并/扩展/相交和 sphere/plane 相交，并提供 float/double 常用别名。
- [x] 增加 math traits 底座，统一描述 CAORS 数学类型的标量、维度和成员访问，为后续 GLM/Eigen
  等第三方 adapter 保留入口。
- [x] 文档化 math 模块和 `meta::value` fast path 的关系：先稳定数学值类型，再基于 benchmark
  决定是否将 `vec2`、`vec3`、`vec4`、`quat` 等高频类型加入 tagged value 快路径。
- [x] 增加基础 math benchmark，覆盖 `vec3`、`mat4`、`quat`、TRS、投影矩阵、double/int
  标量别名和 geometry 纯 C++ 计算路径。
- [x] 为 math 类型补充 meta 注册 helper、serialization codec 和配置/资源示例。
- [x] 增加基础 frustum helper、view-projection helper、project/unproject、meta 注册、serialization codec
  和 benchmark 覆盖。
- [x] 扩展 math benchmark，覆盖 meta 属性访问、方法参数传递和未来 `meta::value` fast path 候选路径。
- [x] 增加渲染向 math 示例，覆盖 camera view-projection、frustum culling、project/unproject 的组合用法。
- [x] 记录第一轮 math fast path 判断策略：当前先收集 benchmark 数据，不立即把数学类型加入
  `meta::value`，后续按真实动态调用和编辑器 workload 决定。
- [ ] 评估 `math::vec2`、`math::vec3`、`math::vec4`、`math::quat` 是否进入 `meta::value` fast path。
- [ ] 基于真实渲染 workload 评估 `mat4 * mat4` 手动展开、SIMD 后端和 affine transform 快路径；
  当前纯标量通用实现优先保持简单正确。
- [ ] 评估双精度、整型向量、完整相机、碰撞和坐标系 helper。

## 1. 类型元数据底座

- [x] 定义 `type_id`、`meta::type` 和 meta 类型注册错误状态。
- [x] 在 `meta::registry` 中集中处理运行期类型索引、重名、重复 GUID、不一致类型描述和父类型缺失。
- [x] 提供 `meta::registry`，允许围绕同一个 `type_id` 挂载对象、序列化和自定义 extension 数据。
- [x] 提供 `meta::registry::extension_view<T...>()`，查询同时拥有指定 extension 数据的类型。
- [x] 补充 meta 属性和序列化 schema capability 的跨模块查询测试。
- [x] 将 registry extension 数据收进 `type_record`，extension 查询直接遍历注册类型记录，不再维护重复反向索引。
- [x] 将动态 capability 挂到注册类型数据，支持 `type_id + capability_key` 查询模块扩展能力。
- [x] 将 serialization codec 接入 `meta::type` 静态 capability；编译期已知 `T` 或携带 `meta::type` 的
  `any` / `any_ref` 路径都可直接查询静态 codec 能力。
- [x] 将对象指针和对象句柄运行期访问迁移到 `meta::type` 静态 capability 查询链路，避免 `type_descriptor`
  直接依赖 object 模块字段。
- [x] 将 descriptor 内置运行期函数表收敛为互斥 `builtin_ops` variant，避免同一类型同时保存多套
  内置 ops 指针。
- [x] 将 descriptor 内置 ops 生成统一到 `*_ops_model<T>` 结构，保持 `*_ops_ptr_of<T>()` 作为内部取表入口。
- [x] 拆分 descriptor 内置 ops 实现边界，内部 `type_descriptor.h` 保留 descriptor 本体，内置 ops 生成移动到
  `builtin_ops_model.h`。
- [x] 将各类 descriptor 内置 ops 继续拆到 `meta/detail/builtin/xxx_ops.h`，`builtin_ops_model.h`
  只保留聚合和分发入口。
- [x] 将 descriptor 内置 ops 的 `*_ops` 函数表、函数指针类型和辅助函数移动到对应
  `meta/detail/builtin/xxx_ops.h`，旧 `container.h` / `type_capabilities.h` 不再保存这些内置 ops 定义。
- [x] 删除旧 `meta/detail/container.h` 聚合头，并将 `type_capabilities.h` 精确改名为
  `parent_cast_ops.h`。
- [x] 将 value 操作函数指针收进 `meta::value_ops` / `meta::factory_ops`，公开层通过
  `meta::factory_capability` 查询构造能力，内部通过 `value_storage_ops` 访问内部值操作，避免 object /
  serialization 横向依赖 `meta::detail`。
- [x] 将内部类型 ops 和静态能力纳入 `meta::type` 的 capability / ops 查询链路，并补充 `meta::type`
  的 kind、wrapper、nullable、value type 和 key type 查询，使字段访问和 serialization 优先使用
  `meta::type` 作为能力 API。
- [x] 将 meta 字段访问收敛为 `field_ops` 函数表，`field_wrapper` 作为 property/concept-table
  包装器承载字段类型、取值、引用和写入能力，调用侧不再直接解释内部类型缓存。
- [x] 将 `type_descriptor.h` 拆分为 descriptor 数据结构、默认名称生成和 descriptor model 构建三个边界。
- [x] 将 descriptor 本体收进 `meta::detail`，公开 API 只通过 `meta::type` 暴露类型视图和能力查询。
- [x] 统一静态与注册 `type_record` 节点，删除 `type_descriptor` 中转结构和 registry descriptor 注册链。
- [x] 增加统一 `traits::type_identity<T>`，让普通类型和 object 类型共享静态名称/GUID 身份及冲突校验。
- [x] 将 set / associative 容器使用的类型擦除迭代器辅助实现移动到 `meta/detail/builtin/container_iterator_ops_model.h`。
- [x] 将 set / associative 共用的迭代器生命周期操作抽成 `container_iterator_ops` 和
  `container_iterator_ops_model<T>`，容器专属取值逻辑继续保留在各自 builtin ops 中。
- [x] 设计开放 dynamic capability / poly concept table，用于外挂用户自定义运行期访问能力。
- [x] 梳理开放 dynamic capability / poly concept table 的最终 API；`meta::type` 提供内置布局、构造、容器和
  wrapper 能力，codec 作为 serialization 模块 capability 负责格式读写，dynamic capability 作为用户扩展能力入口。
- [x] 增加 capability 稳定元信息扩展点，例如 capability name / guid / version，用于插件、工具链诊断和
  跨模块协议；当前 `capability_key` 是共享 runtime 分配的进程内查询 ID。
- [x] 增加多模块静态 capability 扩展机制，外部模块可通过 module tag 和
  `extension_static_capability_modules<T>` 追加能力贡献，避免多个第三方模块只能共享
  `custom_static_capability_module` 造成特化覆盖。
- [x] 增加类型级 capability facade 查询：用户自定义能力默认返回只读 concept table 指针，内置能力可通过
  `type_capability_traits` 返回 `sequence_capability`、`ptr_capability`、`codec_capability` 等轻量对象。
- [x] 增加第一版 proxy capability helper，支持把 `ops` 字段声明式映射到 C++ 成员函数，并通过
  `CAORS_META_DEF_MEM_DISPATCH + capability_builder::add_convention<Dispatch, Signature>` 等入口直接注册简单成员函数能力。
- [x] 增加 Proxy 风格 convention builder，支持
  `CAORS_META_DEF_MEM_DISPATCH + capability_builder::add_convention<Dispatch, Signature>` 和
  `CAORS_META_DEF_TYPE_DISPATCH + add_type_convention<Dispatch, Signature>` 自动生成 capability，
  并通过成员函数名、静态成员函数名、自由函数指针和函数签名接入自动 model。
- [x] 收敛 Proxy 自定义能力入口，移除早期 X-macro DSL，统一推荐 dispatch + `capability_builder`。
- [x] 增加通用 `meta::metadata_map`，用于给类型、属性、方法和枚举项挂载轻量键值附加信息；
  metadata 只承载工具、配置和编辑器提示，不替代类型身份或 capability。
- [x] 收敛 identity ID 底座：内部提供按 domain/canonical name 发号的统一 identity state，
  外部保留 `type_id`、`capability_id`、`extension_id` 等强类型 ID。
- [ ] 增加 identity store 调试查询，并评估后续 `schema_id`、`event_id`、`resource_type_id`
  等新 ID 是否复用同一发号器模式。
- [ ] 扩展完整 Proxy DSL，进一步减少用户自定义 capability 的 `ops` / `view` / `model` 样板代码。
- [ ] 支持 Proxy 模板方法代理。
- [ ] 支持 Proxy 重载自动选择。
- [ ] 支持 Proxy 返回引用、借用值和生命周期策略。
- [ ] 设计跨 DLL / 插件边界的 capability 稳定协议。
- [ ] 评估完全无宏 Proxy DSL，降低宏接口对诊断和工具链的影响。
- [ ] 评估基于未来静态反射或代码生成的成员自动发现。
- [x] 将运行期类型查询、属性/方法和值构造能力合并到 `meta` 主入口，不再保留独立 `runtime` 模块。
- [x] 将最小对象基类迁移到 `meta`：`meta::object` / `meta::object_base<T>` 只负责对象身份、
  `object_id`、运行期类型入口和虚析构；`objects` 模块继续负责 `ptr` / `ref` / `weak_ref` /
  `handle` / `manager` / `registry` / 序列化 resolver 等生命周期和对象系统组合能力。
  `meta::type::is_object()` 负责查询已注册类型是否是 `meta::object` 或其派生类型。
- [x] 定义 `meta::value` 作为高频动态运行期值，类似 Lua `TValue` / Godot `Variant` 的 tagged value：
  基础标量、`string_name`、轻量字符串、`meta::object*` 走内联或轻量指针快路径，复杂 C++ 类型通过
  `any_ref` 引用值或 `any` 拥有值兜底；`meta::variant` 继续作为序列化/配置 DOM，不承担 userdata 语义。
- [x] 梳理 `meta::value`、`meta::any`、`meta::any_ref`、`meta::variant` 的职责边界和转换 API：
  `value` 面向脚本/编辑器/消息/动态调用，`any` 面向拥有的 C++ 精确类型值，`any_ref` 面向非拥有 C++
  精确类型引用，`variant` 面向可持久化数据树。
- [x] 将 `meta::type_<T>()` 改为 `registration::type_<T>(name)`，并将枚举注册收敛为
  `registration::enum_<T>(name)`。
- [x] 将 `meta::type::create()` 统一为高层创建入口，优先使用 `meta::factory_capability`，再使用
  `meta::constructor_capability` 匹配运行期参数列表，最后回退到 `meta::type` 的默认构造能力；`create_value()`
  保留为高级默认构造入口。
- [x] 增加 `registration::batch_builder` 最小批量注册 API，支持 type/enum builder 顺序提交和首个失败项诊断。
- [x] 文档化当前 append-only 注册生命周期、无隐式 replace、批量注册非事务和 view 稳定性边界。
- [x] 整理未来 registry 管理策略方向：`locked_append_only`、`frozen_read_optimized`、
  `manual_synchronization`，并与 `pinned_process` / `detachable` 插件生命周期分离。
- [x] 收敛 registry 查询快路径第一阶段：发布后的 frozen layer 查询基于 snapshot 无锁读取；
  主进程 pinned layer 调用免 in-flight guard，插件 detachable layer 仅在进入插件 ops 时使用调用级 guard。
- [ ] 继续收敛主 registry freeze 文档和 benchmark，评估显式 `registry::freeze()` 与延迟注册的交互。
- [x] 增加 registry 管理策略 API：`registry_options` / `registry_management_policy`，
  默认保持内部加锁的 append-only 语义，后续支持冻结读优化和高级手动同步。
- [ ] 评估注册项 replace、dry-run 校验和更完整的批量注册事务语义。

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
- [x] 抽象共享 `meta::registry`，避免对象系统和 meta 属性/方法重复维护基础类型索引。
- [x] 对象类型 API 基于 `meta::registry` 挂载底层 `objects::detail::object_factory_ops`，并通过
  `meta::type`、`objects::object_factory_capability` 和对象语义 helper 对外查询。
- [x] 提供对象语义查询 API 和默认构造工厂。
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
- [x] 在最小对象基类迁移到 `meta` 后，整理 `objects` 模块职责：保留对象指针、句柄、引用计数、
  manager、object_index、object factory、对象序列化 resolver 和对象注册 facade；根命名空间
  `caors::object` / `caors::object_base<T>` 继续指向最小对象基类。
- [x] 将对象高层访问 API 逐步对齐 `meta::value`：对象属性读写、方法调用和对象指针属性访问优先返回或接收
  `meta::value`，保留 `any_ref` 作为精确引用入口，避免对象系统高层 API 继续强绑定 `any` 拷贝语义。

## 4. Meta 属性和方法

- [x] 定义类型和字段的基础元信息模型。
- [x] 将字段和方法数据并入 `meta::registry` 的类型数据，并通过 meta 视图对外查询。
- [x] 支持基于已注册 `meta::type` 注册字段和方法，减少反射层隐式类型注册。
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
- [x] 设计独立 constructor / 带参构造函数注册组件，并接入 `meta::type::create()` 的统一创建策略。
- [x] 支持字段级 metadata 注册与查询，供配置校验、导入管线和编辑器工具按约定键读取。
- [x] 支持同名成员方法重载集合，提供按名称和参数类型精确查询 / 调用的 overload set API。
- [x] 支持静态字段和静态成员函数反射；当前按全局 module 属性 / 函数注册，不混入实例字段和实例方法语义。
- [x] 支持自由函数和静态函数注册为 module 级函数 / 命令能力，供脚本、编辑器和工具层调用。
- [x] 支持 getter / setter 属性注册，提供 property API 和只读 getter 属性入口。
- [x] 支持 lambda 或函数对象注册为 module 级函数 / 命令能力；当前要求 callable 具有唯一
  `operator()` 签名。
- [x] 支持嵌套类型关系记录，例如 `Outer::Inner`、`Outer::Kind` 可作为独立类型注册，同时能从 `Outer`
  查询 nested type 列表。
- [x] 支持父类字段和方法的递归查询策略，区分只查本类型和查继承链；同名成员查询时子类优先。
- [x] 支持方法尾部默认参数注册，并通过 `argument_match_options` 显式开启默认参数匹配；默认仍保持精确参数数量匹配。
- [x] 支持方法命名参数注册和命名参数调用，并通过 `argument_match_options` 显式开启；默认仍保持位置参数精确匹配。
- [x] 为方法参数匹配增加可配置类型转换策略 API，当前支持 `exact_only` 和 `numeric_safe`。
- [x] 支持方法返回引用和借用值策略，通过 `meta::invoke_result` 区分失败、`void` 成功、拥有值和
  `any_ref` 引用返回。
- [x] 支持不可拷贝但可移动的 by-value 方法返回值，由 `invoke_result` 持有为拥有值。
- [x] 支持方法安全数值 widening 转换策略，默认仍保持精确匹配，避免隐式转换造成歧义。
- [x] 增加 `method_set::match()` 结构化匹配结果，区分未匹配、精确匹配、转换匹配和歧义匹配。
- [x] 增加 `invoke_result::failure_reason()`，覆盖无效方法、无效对象、const 违规、参数不匹配、
  重载歧义和底层调用失败。
- [x] 增加来源类型级 `argument_conversion_ops` capability，并通过
  `argument_match_options::allow_custom_conversions` 显式开启自定义参数转换。
- [x] 为 `meta::property` 增加 `meta::value` 高层 API，例如 `get_value()` /
  `try_set_value(meta::value)`；复杂 C++ 类型通过 `any_ref` 或 `any` 适配现有精确类型路径。
- [x] 整理 `meta::property` 的拥有值、引用值和动态值命名：`get_value()` 返回 `meta::value`，
  `get_any()` 返回稳定 C++ 拷贝，`get_ref()` 继续返回 `any_ref`，避免普通用户把 `any` 当作动态值主入口。
- [x] 为 `meta::method` / `meta::method_set` 增加 `meta::value` 调用入口，例如
  `invoke(instance, span<const meta::value>)` / `instance::call()`；调用时优先尝试 tagged value fast path，
  不匹配时回退到 value 到 `argument` / `any` 的 adapter。
- [x] 为方法注册器生成 `meta::value` fast path：基础数值、布尔、字符串、`string_name`、`meta::object*`
  等常见按值参数直接从 tagged value 转换并调用 C++ 成员函数；复杂参数、引用参数和歧义匹配继续使用现有
  `argument_match_options` / `invoke_result` 机制。
- [x] 为属性读取和方法返回补齐 `meta::value` fast path：常见标量、字符串、`string_name` 和
  `meta::object*` 直接返回 tagged value；复杂 C++ 返回值继续回退到 `any`。
- [x] 补充 `meta::value` 动态调用 benchmark，覆盖直接 C++ 调用、`method::invoke_result_of()`、
  `method::invoke()`、`instance::call()`、`instance::call_any()` 和属性 typed/value/any 路径。
- [ ] 扩展字符串解析或更完整的类型转换策略。
- [ ] 支持既不可复制也不可移动的 by-value 方法返回值诊断策略。
- [ ] 扩展成员函数指针存储和调用适配，覆盖多继承等可能导致成员函数指针尺寸更大的平台 ABI。
- [ ] 扩展属性别名、访问控制、重载选择和类型转换测试。

## 5. 序列化

- [x] 定义统一的序列化接口和错误模型。
- [x] 基于 `meta::registry` 挂载序列化元信息组件。
- [x] 支持基于已注册 `meta::type` 挂载序列化 schema capability，减少序列化层隐式类型注册。
- [x] 提供字段级序列化配置，支持字段别名、跳过和注册顺序解耦。
- [x] 设计 writer 后端接口，为文本、二进制和 msgpack 等格式预留扩展点。
- [x] 与 meta 属性/方法打通，实现基于元信息的基础字段文本序列化。
- [x] 增加 `variant` 中间对象树，支持对象、数组、标量和字段 id/name，并保留 streaming writer 快路径。
- [x] 补充 `variant` 安全访问器、数组/对象 helper 和 `any` / `any_ref` 到 `variant` 的转换入口。
- [x] 将运行期值能力纳入 `meta/value`，提供 `any` C++ 类型值和 `variant` 数据值。
- [x] 增加 `any_ref`，支持反射字段零拷贝引用访问和不可拷贝字段读取。
- [x] 为 `meta::any` 评估并实现小对象优化，优先减少标量、小型平凡对象和常见临时值的堆分配，同时保持
  `memory_resource` 构造和销毁语义一致。
- [x] 增加 `serialization::reader` 后端接口和 DOM reader，支持基础字段反序列化。
- [x] 增加反序列化缺失字段策略、未知字段默认忽略和基础 schema version 字段过滤。
- [x] 增加类型级 serialization codec 扩展点，并内置 string、string_view、guid 和 type_guid codec。
- [x] 支持值类型嵌套结构递归序列化和 DOM 反序列化。
- [x] 支持 `caors::vector<T>` 数组字段，覆盖标量元素和已注册结构体元素。
- [x] 序列化 vector 字段不再要求手动注册 vector 类型名。
- [x] 支持 `caors::objects::ptr<T>` 对象指针字段和 resolver。
- [x] 增加 JSON writer、JSON DOM parser 和 `to_json()` / `write_json_to()` / `read_json_from()`。
- [x] 为文本和 JSON writer 增加外部输出 buffer / `serialize_*_to()` 入口，允许调用方复用
  `caors::string` 并减少结果拷贝，后续可接入指定 `memory_resource`。
- [x] 增加基于 `io::input_stream` 和 `io::file_system` 的 JSON 读入便捷入口，避免上层反序列化直接绑定
  宿主文件系统或一次性字符串来源。
- [x] 将 JSON 输出流入口改为 `json_writer` 直接写入 `io::output_stream`，避免先生成完整 JSON 字符串。
- [x] 扩展 JSON parser-side options，例如最大嵌套深度、重复 key 策略、尾随逗号开关和完整
  Unicode 转义处理。
- [x] 将 JSON DOM parser 内部拆为 token reader 和 DOM 构造两层，并支持文本 token reader 与
  `io::input_stream` 增量 token reader。
- [x] 将 JSON `io::input_stream` 入口切到增量 token reader，使序列化层不必先读取完整文本。
- [x] 增加 `variant::dictionary` 和 `caors::hash_map<K, V>` 字典写出/读回。
- [x] 将容器序列化统一到 `meta::type` + `sequence_view` / `set_view` / `associative_view` 路径，删除独立通用容器 codec 递归实现。
- [x] 支持基于 `traits::set_traits<T>` 的 `caors::hash_set<T>` 集合写出/读回。
- [x] 支持基于 `traits::sequence_traits<T>` 的 `caors::list<T>` 数组写出/读回。
- [x] 支持裸指针、普通 wrapper、`objects::ptr<T>` 和 `null` 边界处理。
- [x] 支持 `objects::ptr<T>` 通过持久 `object_guid` resolver 写出和读回。
- [x] 支持 `std::array<T,N>`、`std::deque<T>`、`std::forward_list<T>`、`std::set<T>` 和 `std::map<K,V>` 的默认序列化适配。
- [x] 支持 `std::pair<T,U>` 按二元素数组序列化和反序列化。
- [x] 支持 `std::tuple<Ts...>` 按元素数组序列化和反序列化。
- [x] 支持 `std::variant<Ts...>` 按 `[index, value]` tagged union 序列化和反序列化。
- [x] 支持 `std::span<T>` / `std::span<T,N>` 作为借用数组视图反射访问和序列化写出，默认禁止反序列化写回。
- [x] 为 `std::string_view` 设计安全反序列化策略，避免悬空引用。
- [x] 将 `serialization::context::state.storage.string_view_storage` 从单个字符串存储升级为多段
  `string_storage` / `string_buffer`，支持同一对象内多个 `std::string_view` 字段安全读回。
- [x] 为 `std::weak_ptr<T>`、对象句柄和资源引用设计稳定引用语义。
- [x] 支持 `resource_guid` 和 `resource_ref<T>` 按稳定 GUID 读写；默认反序列化后资源引用保持未解析状态，也可通过 context resolver 立即解析。
- [x] 支持多态对象的动态类型记录、manager 构造和安全下转型。
- [x] 支持普通 C++ 类在 `std::unique_ptr<Base>` / `caors::memory::unique_ptr<Base>` /
  `std::shared_ptr<Base>` 字段中的动态类型记录、默认构造和安全下转型。
- [x] 为普通类多态值补充显式 factory 和可选 `memory_resource` 构造策略。
- [x] 增加 `serialization/detail/value_access.h` 内部桥接层，集中访问 `any` / `any_ref` 携带的
  `meta::type` 和默认构造能力，避免 serialization detail 文件散落依赖 meta value 内部细节。
- [x] 简化序列化公开 API，保留 `serialize` / `deserialize` / `to_value` / `to_string` 等主路径，
  将低频 JSON/text buffer 入口收敛到 `serialization::` 模块命名空间。
- [x] 拆分 `serialization::context` 的运行期状态、策略配置和 resolver 适配器，减少上下文字段膨胀。
- [x] 设计字段级构造策略，覆盖容器新增元素、拥有型智能指针 wrapper、`variant` 候选切换和不可默认构造类型；
  优先使用 `serialization::context::resolvers.construction`、`memory_resource`、meta factory / constructor
  capability 和 `meta::type` 默认构造，失败时返回明确诊断。
- [x] 增加 `serialization::validate_config()`，基于反射属性 schema 校验 typed config，支持
  `required`、`non_empty`、`range_min/range_max` 和 `one_of`，并一次收集多条诊断。
- [x] 将 `serialization::validate_config()` 对齐 `meta::property_schema`，并提供
  `serialization::apply_config_defaults()` 显式应用 schema 默认值；旧 metadata 键仅作为兼容输入保留。
- [x] 扩展空 `optional<T>` 读入非空值时的安全构造和写入策略，避免把拥有型 wrapper 的接管语义误用于
  `optional` 的按值存储语义。
- [ ] 放宽多层指针 / 多层 wrapper 限制，采用逐层 indirect `meta::type` 展开；默认设置合理最大深度，
  并对空中间层、不可写引用和所有权不明确的组合返回清晰诊断。
- [x] 扩展普通类多态 envelope 的 reader 抽象，使自定义流式 reader 也能提供 type/value envelope
  读入能力；DOM/JSON reader 通过内置 hook 接入，自定义 reader 可不暴露 `current_variant()`。
- [x] 设计 schema 策略第一版，支持字段旧名 / 旧 id 读入、未知字段忽略、保留和严格拒绝。
- [x] 增加 schema migration hook，支持在 DOM 反序列化前按源版本、目标版本和目标类型改写输入数据。
- [x] 支持显式序列化字段缺失时从 `meta::property_schema::default_value` 恢复默认值。
- [x] 增加缺失字段策略，允许 typed config 等配置读入路径采用“保留已有默认值后局部覆盖”的语义。
- [ ] 扩展未知字段按对象树位置保留、跨对象迁移调度和更完整的字段删除转换策略。
- [ ] 设计弱引用序列化策略，默认继续不展开 `std::weak_ptr<T>` / `objects::weak_ref<T>`，
  提供基于 `objects::handle`、`resource_guid` 或自定义 resolver 的显式弱引用策略。
- [ ] 增加紧凑二进制和 msgpack 等二进制后端。
- [x] 增加版本兼容、缺失字段和未知字段基础测试。
- [ ] 增加跨平台字节序测试。

## 6. 资源系统

- [x] 建立 engine-agnostic 资源核心层文档边界：资源表示可持久化、可缓存、可复用资产，对象表示当前运行期实例。
- [x] 定义 `resources::resource`、`resource_guid` 和 `resources::ref<T>` 基础身份与轻量引用模型。
- [x] 增加 `resources::handle<T>` / `weak_handle<T>` 运行期引用计数句柄，和稳定 GUID 引用保持分离。
- [x] 提供非拥有 `resources::manager` 资源索引，支持按 `resource_guid` 绑定、查找、解绑、清空和解析 `resources::ref<T>`。
- [x] 提供 `resources::cache<T>` 拥有式资源缓存，支持按 `resource_guid` 创建、持有、释放、销毁和解析资源引用。
- [x] 将 `cache<T>` 内部升级为强 `resource_handle<T>` 持有模型，并提供 `create_handle()`、
  `find_handle()`、`load_handle_into()` 和 `reload_handle_into()`。
- [x] 支持 `resources::ref<T>` 通过序列化 codec 按稳定 `resource_guid` 读写，读回后可由 manager/cache 显式解析，也可通过序列化 context resolver 按策略解析。
- [x] 设计 `resources::loader<T>` / `resources::saver<T>` 接口，并提供基于 serialization reader/writer 的最小读写 helper。
- [x] 支持序列化遍历收集 `resource_ref<T>` 资源依赖，用于后续预加载、打包、热更新和资源数据库。
- [x] 提供 `resources::add_resource_dependencies()` / `replace_resource_dependencies()`，把
  `resource_dependency_list` 写入 `resources::dependency_graph`。
- [x] 打通资源依赖收集到 manifest / provider / loader / cache 预加载，再由序列化 resolver 解析
  `resource_ref<T>` 的同步链路。
- [x] 提供非拥有 `resources::database` 编排入口，把 manifest、provider、类型化 cache/loader 和
  dependency graph 串成同步资源主流程；序列化资源 resolver 通过 `resources/serialization.h`
  adapter 接入。
- [x] 扩展资源加载、卸载和失效处理策略，并明确生命周期与线程安全边界。
- [x] 提供 `core::locator<T>` 可选全局服务定位器，用于定位默认资源管理器、文件系统、配置服务等进程级服务，并明确注册、重置和测试隔离规则。
- [x] 设计底层 `io::file_system` 字节来源抽象，统一本地文件、内存文件、资源包和远程网络资源访问的接口边界。
- [x] 设计资源层 `resources::source` / `resources::provider`，负责从 `resource_guid` 或资源位置解析到统一数据流，再交给 loader 解码。
- [x] 实现本地文件系统后端 `io::native_file_system`。
- [x] 实现 `resources::file_source`，把 `io::file_system` 适配为资源字节来源。
- [x] 为 `resources::source` / `resources::provider` 增加 `open_read()` 输入流入口，让 loader 和 assets
  配置读取不必直接依赖完整字节缓冲。
- [x] 为 `resources::source` / `resources::provider` 明确资源包和远程后端扩展边界：后端可以覆盖
  `open_read()` / `open_write()` 返回自定义流，provider 只负责 GUID 到 source/location 的索引。
- [x] 实现 `assets::json::loader<T>`，打通 `resource_guid -> provider -> JSON -> resource` 的同步加载链路。
- [x] 实现内存文件系统后端 `io::memory_file_system`，用于测试、热更新缓存和后续资源包索引内存镜像。
- [x] 增加 `resources::manifest`，保存 `resource_guid -> location/type/hash/version` 清单，并可批量绑定到 provider。
- [x] 支持从 JSON 文本或 `resources::source` 导入资源 manifest，作为本地资源数据库和调试 fileserver 清单入口。
- [x] 支持 manifest 条目更新和 provider 批量同步，方便调试 fileserver 或资源数据库刷新已有资源位置。
- [x] 支持 manifest 全量替换 provider 绑定，方便完整资源清单刷新时移除旧资源位置。
- [x] 设计工程/场景资源配置描述格式，用于声明 source/provider、manifest、资源类型、loader/cache
  绑定、预加载根资源和资源解析策略。
- [x] 提供配置反序列化入口，先把 JSON/DOM 读成 `assets::project_config` 或类似描述对象，不直接反序列化
  运行期 `provider` / `database` / `cache` / `loader` 指针对象。
- [x] 增加格式无关 `assets::project_config_loader` 接口，JSON 只是其中一个适配，后续 YAML、包索引或远程配置可复用同一入口。
- [x] 设计拥有式 `assets::asset_manager` 或资源运行期 builder，持有 manifest、provider、dependency graph
  和非拥有 `database`，统一保证核心资源运行期对象生命周期。
- [x] 提供 `assets::asset_manager` 第一版，拥有 manifest、provider、dependency graph 和 database，并可从
  `project_config` 构建运行期资源定位与依赖图。
- [x] 将高层工程资源入口拆到 `assets` 模块，`project_config`、`asset_manager`、JSON loader/saver 和
  manifest/project config JSON 读入 helper 不再挂在 `resources` 核心入口下。
- [x] 提供第一版 loader/source/cache 工厂注册机制，把配置中的 `"json"`、`"native_file_system"`、
  `"memory"` 等策略名映射到运行期 source、cache 和 loader 构造；`"remote"`、资源包和专用缓存后续扩展。
- [x] 为 `assets::asset_manager::build_detailed()` 增加构建阶段诊断，记录失败阶段、配置项索引、
  source/type 名称、资源 GUID 和可读原因。
- [x] 删除 assets 独立轻量 options schema，统一由 typed factory options 复用
  `meta::property_schema` / `serialization::validate_config()` 表达配置约束。
- [x] 扩展 `asset_manager::build_detailed()` 的 typed options 诊断，支持同一 options 对象一次收集多条错误。
- [x] 增加 typed factory options helper：配置 `variant` 先应用属性默认值，再反序列化为结构体，
  最后通过 `serialization::validate_config()` 和 `meta::property_schema` 完成校验。
- [x] 增加 typed file_system/source/type factory 注册入口，让复杂 options 在调用用户工厂前统一完成
  默认值、未知字段拒绝、schema 校验和 `build_detailed().reasons` 诊断汇总。
- [x] 继续收敛资源公开主入口，以 `assets::asset_manager` / 运行期 builder 作为用户优先入口，
  `database`、`provider`、`cache<T>` 和 `loader<T>` 作为高级组合入口。
- [x] 支持 `resources::provider` 通过可写 source 按 GUID 写回字节，为后续 saver 和热更新回写提供基础。
- [x] 实现 `assets::json::saver<T>`，打通 `resource -> JSON -> provider -> source` 的同步保存链路。
- [x] 提供 `resources::save_from()`，把 cache 中指定 GUID 的资源交给 saver 保存。
- [x] 提供 `resources::reload_handle_into()` 和 `cache<T>::replace()`，支持同步重载成功替换、失败保留旧资源。
- [ ] 实现资源包、远程资源或异步 loader/saver 后端。
- [x] 扩展 `resources::database` 的同步预加载类型选择策略，支持按 manifest type 分派到匹配 loader/cache。
- [x] 扩展 `resources::database` 的资源类型选择策略，支持按 type_guid 或外部资源数据库 token 选择匹配
  loader/cache。
- [x] 设计 Godot 风格的异步资源加载 API：`request_load<T>()` / `request_reload<T>()` 提交后台任务，
  `load_request` 保存 queued/loading/loaded/failed/cancelled 状态、错误和结果 handle，调用方跨帧查询状态后再获取结果。
- [x] 补充异步资源请求的基础取消语义：queued/loading 可取消，已开始的 loader 不强制中断，但取消后不再提交
  cache commit。
- [x] 扩展异步资源请求进度语义，支持 loader 报告可选进度和阶段文本，并让
  `preload_request::progress()` 聚合底层 load request 的内部进度。
- [x] 实现资源异步执行器和后台任务队列：后台阶段只做 provider 读取、loader 解码和资源对象构造，不直接修改
  `cache<T>`、manifest 或运行期对象状态。
- [x] 实现主线程 commit 阶段：第一版通过 `tasks::completion_queue::drain()` 将已完成异步结果插入或替换 cache，
  失败时保留旧资源；完成、失败和 reload 通知后续再接入 events/signal 或 dispatcher。
- [x] 提供 `resources::database::request_preload<T>()` / `request_preload()`，支持把依赖闭包提交为
  `preload_request`，并在 completion queue 中按 cache 所属线程提交资源。
- [x] 设计并实现 database 级异步单资源请求合并和生命周期策略：同资源类型、同 GUID 的 direct
  load/reload 重复请求复用同一 request，已缓存资源可立即完成，取消、完成和失败后会从请求表移除。
- [x] 扩展异步预加载内部请求合并：同类型同 GUID 的 direct load / preload load 复用同一请求，
  已缓存资源立即完成，取消和失败按批量请求独立聚合，并提供非阻塞 poll/drain helper 辅助测试和主循环收敛。
  优先级、真正阻塞等待、分层依赖调度和细粒度进度聚合后续再评估。
- [x] 为 `preload_request` 增加每个 preload item 的详细失败列表，便于工具链展示批量预加载中具体失败项。
- [x] 明确异步资源系统线程安全边界：哪些 `source` / `provider` / `memory_resource` 可跨线程访问，
  cache 修改必须在哪个线程执行，未完成 request 的 `try_get()` 是阻塞、返回错误还是禁止调用。
- [x] 打通资源引用序列化 resolver，支持反序列化后按策略解析稳定 GUID 引用。
- [x] 补充 `project_config -> VFS/provider -> asset_manager -> async preload -> resolve -> reload/save`
  端到端示例和集成测试。
- [x] 继续拆分资源序列化 adapter，把 `resources::database` 中强依赖 `serialization` resolver 的高级入口
  收敛到 `resources/serialization.h` 和 `assets/serialization.h` adapter 层。
- [x] 设计资源热重载边界：第一阶段放在资源系统主题内，不新增独立主题；热重载只负责发现资源描述或字节内容变化、
  刷新 manifest / provider / cache，并返回 reload 结果，不隐式修改对象、场景树或运行期业务状态。
- [x] 支持基于 manifest / provider / cache 的手动同步热重载流程：调用方检测 `hash`、`version` 或
  `location` 变化后，重新通过匹配 loader 加载资源，并按 cache 策略替换旧资源或保留旧资源。
- [x] 设计热重载失败策略和通知机制：缺资源、加载失败、类型不匹配或替换失败时保留旧资源，返回可诊断错误；
  可通过 `resources::reload_and_notify()` 显式接入 `events::signal` 或上层编辑器通知。
- [x] 设计资源句柄 generation 机制，让已解析资源引用能够检测 cache 替换后的过期状态。
- [ ] 后续评估资源引用控制块或跨 cache/version 机制，支持更强的自动失效、弱引用和调试诊断能力。
- [x] 定义 reload 后 handle 语义：cache 指向新资源，旧 `resource_handle<T>` 保留旧资源直到最后一个强引用释放。
- [x] 设计资源重载通知事件，reload 完成后可通知材质、场景对象、编辑器面板或上层系统重新解析引用。
- [x] 实现第一版资源依赖图，记录资源 GUID 之间的依赖和反向依赖，用于预加载、打包、热更新和级联刷新。
- [x] 提供 `dependency_graph::affected_by()` 作为 reload 影响范围的直接依赖查询入口。
- [x] 提供 `dependency_graph::affected_closure()` 作为 reload 影响范围的递归查询入口，覆盖循环依赖和去重边界。
- [x] 提供 `dependency_graph::has_cycle()` / `find_cycle()`，让预加载、打包和拓扑排序流程可以显式拒绝循环依赖。
- [x] 提供 `resources::preload_plan` / `preload_result`，记录依赖优先的加载顺序、已缓存资源、缺失 manifest
  条目和循环依赖路径。
- [x] 提供 `resources::database::plan_preload<T>()` / `preload<T>()`，第一版支持同一资源类型的同步依赖闭包预加载。
- [x] 提供 `resources::database::plan_preload()` / `preload()`，支持跨资源类型依赖闭包按 manifest type
  同步预加载。
- [ ] 扩展资源依赖图到对象、场景和资源包层级，并定义跨层级 reload 影响范围计算。
- [ ] 设计编辑器 file server / runtime remote file system 调试链路：编辑器进程暴露统一 IO 服务，
  运行时通过 `caors://` 逻辑路径访问资源；发布版本可切换到 pack/native 等后端而不改上层资源 API。
- [ ] 评估进程内 file server 与跨进程 editor file server 的统一协议边界，使本地调试、远程热更新和
  资源包读取共享同一套 source/provider/loader 上层流程。
- [ ] 后续评估调试 fileserver、资源包、远程资源、导入管线、资源数据库和编辑器工具层；当这些能力需要独立
  生命周期、后台任务或 UI/工具协议时，再考虑拆出独立资源工具链主题。

## 7. 任务调度

- [x] 建立 `tasks` 主题，第一阶段只依赖 C++20 STL，不引入第三方任务库。
- [x] 定义最小 `tasks::executor` 抽象，用于资源异步加载、资源导入管线、后台序列化和跨线程事件投递等模块提交任务。
- [x] 提供 `tasks::inline_executor`，用于测试和确定性执行；提交的任务在调用线程立即运行。
- [x] 提供 `tasks::thread_pool_executor`，基于 `std::jthread` / `std::mutex` / `std::condition_variable_any`
  实现固定线程池，支持安全停止和析构前 drain / cancel 边界。
- [x] 提供轻量 move-only task wrapper，避免把任务系统核心 API 绑定到 `std::function` 的可拷贝要求。
- [x] 提供 `tasks::completion_queue` 主线程回调队列，用于后台任务完成后把结果交给主线程 commit。
- [x] 定义基础任务状态和结果语义，提供 `tasks::request` / `submit_request()`，覆盖
  queued/running/completed/failed/cancelled；进度、优先级、阻塞等待和取消传播后续再评估。
- [x] 明确任务调度边界：第一阶段不做 work stealing、fiber、协程 scheduler、任务图和平台异步 IO；这些能力后续按需求评估。

## 8. 事件与委托

- [x] 设计类似 EnTT 的轻量 `events::delegate`，支持自由函数、成员函数、lambda / 函数对象绑定，并明确所有权和生命周期限制。
- [x] 设计 `events::signal`，支持类型安全订阅、取消订阅和同步触发。
- [x] 提供 `events::connection` 和 `events::scoped_connection`，支持手动断开和 RAII 自动断开。
- [x] 提供类似 EnTT sink 的独立连接视图，允许外部订阅但不暴露事件触发入口。
- [x] 设计 `events::dispatcher`，支持按事件类型入队、批量派发和清空队列，并接入 CAORS 容器与 `memory_resource`。
- [x] 明确事件系统和对象系统的关系：底层事件模块不隐式持有对象，涉及对象生命周期时由 `objects::ptr<T>`、`objects::handle` 或上层系统显式管理。
- [x] 评估线程安全事件队列、跨线程投递和异步调度策略；第一阶段默认只保证单线程使用边界。
- [x] 补充委托连接、断开、事件顺序和 emit 期间断开连接等边界测试。
