<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Serialization

本文档说明 CAORS 序列化层的当前实现边界。当前实现提供基于反射字段的文本/JSON 读写、`variant`
DOM 中间对象、类型级 codec、资源引用处理和受限多态读写，用于连接 `meta::registry`、反射字段访问、
运行期值系统、资源系统和统一错误模型。

## 设计目标

当前已覆盖：

- 用 `serialization::type_schema` 表示类型的序列化元信息。
- 用 `serialization::field_options` 表示用户注册时填写的字段级序列化配置。
- 用 `serialization::field_schema` 表示提交后的公开字段配置；运行期属性视图和调度缓存由序列化 detail 按需编译。
- 默认基于全局 `meta::registry` 挂载序列化 schema capability；需要隔离注册表时，可通过
  `serialization::context::type_context` 指定本次读写使用的 `meta::context`。
- 提供 `serialization::schema_view` 查询序列化 schema；业务类型通过 `registration::type_<T>(name).serialization()` 完成跨模块注册。
- 提供 `serialization::writer` 后端接口，让不同格式复用同一套字段遍历逻辑。
- 提供 `serialization::reader` 后端接口，让不同格式复用同一套字段读入逻辑。
- 提供 `variant` 中间对象树，方便工具、调试和多格式转换。
- 提供 `serialization::text_writer` 和 `serialization::to_text()`，通过反射字段读取对象并输出调试用简单文本结构。
- 提供 `serialization::json_writer`、`serialization::json_reader`、`to_json()`、`serialization::parse_json()` 和 `read_json_from()` JSON 便捷路径。
- `serialization::json_writer` 可写入 `caors::string`，也可直接写入 `io::output_stream`。
- 提供 `serialization::variant_reader` 和 `read_value_from()`，通过 DOM 中间对象写回基础反射字段。
- 提供 `serialization::context`，支持指定 meta 类型上下文、按 schema version 过滤字段、配置多态
  envelope 字段名、传递运行期资源、解析资源引用和收集资源依赖。
- 提供第一版 schema 兼容策略，支持字段旧名 / 旧 id 读入、未知字段忽略、保留和严格拒绝。
- 提供 `serialization::codec_traits<T>` 和 `meta::type` codec capability，允许把特定 C++ 类型作为原子序列化值读写。
- 支持已注册反射和序列化元信息的值类型结构体字段递归序列化和 DOM 反序列化。
- 提供基于 traits 的顺序容器、集合容器、关联容器、pair、tuple 和 variant 访问，支持标量、wrapper、对象引用和已注册结构体元素。
- 通过 `objects::object_ref_resolver` 支持 `caors::objects::ptr<T>` 对象指针字段。
- 普通序列化入口会在 `caors::meta::object` 派生对象的运行期类型不同于声明类型时自动写出多态 envelope。
- 通过默认 `$type` / `$value` envelope 支持 `std::unique_ptr<Base>`、`caors::memory::unique_ptr<Base>`、
  `std::shared_ptr<Base>` 和 `objects::ref<Base>` 字段持有派生类对象，并支持显式 factory 构造策略。
- 用 `caors::error` 和 `caors::result<T>` 表达失败状态和携带值的返回结果。

当前仍不会包含：

- 内置二进制格式。
- 完整版本迁移、字段删除转换和未知字段按对象树位置保留。
- 完整对象图身份保持。
- 自动把任意裸指针或非拥有 wrapper 字段提升为拥有型多态对象。
- 借用视图的自动存储分配和生命周期管理。

这些能力后续应在对象句柄、字段策略和格式扩展点明确后逐步补充。

## 核心概念

### 命名空间

序列化模块的推荐类型名位于 `caors::serialization` 命名空间，例如
`serialization::type_schema`、`serialization::field_options`、`serialization::field_schema`、`serialization::context`、
`serialization::writer`、`serialization::reader`、`serialization::text_writer`、
`serialization::json_writer`、`serialization::json_reader`、`serialization::variant_reader` 和
`serialization::codec_traits<T>`。
根命名空间只保留高频且语义明确的操作函数别名：`serialize()`、`deserialize()`、`to_value()`、
`read_value_from()`、`to_json()` 和 `read_json_from()`。reader、writer、result 以及
`write_*_to()` 这类复用调用方 sink 的入口使用 `serialization::` 下的短名称。

### 文件组织

序列化模块按职责拆分公共头文件：

- `caors/serialization/type_schema.h`：后端能力、字段配置和类型序列化 schema capability 数据。
- `caors/serialization/external_ref.h`：通用外部引用、函数表 resolver/collector 和继承接口适配器。
- `caors/serialization/types.h`：通用外部引用选项、对象工厂函数表、单次调度上下文和结果类型。
- `caors/serialization/schema_query.h`：序列化 schema 查询入口。
- `caors/serialization/writer.h`：`serialization::writer`、`serialization::text_writer` 和
  `serialization::json_writer`。
- `caors/serialization/reader.h`：`serialization::reader`。
- `caors/serialization/codec.h`：类型级 codec 扩展点，以及接入 `meta::capabilities` 的能力查询包装。
- `caors/serialization/dom_reader.h`：从 `variant` DOM 读入的 reader 后端。
- `caors/serialization/json_reader.h`：从 JSON 文本范围直接读入的 reader 后端。
- `caors/serialization/operations.h`：`serialize()`、`deserialize()`、`to_value()`、`to_json()`、
  `serialization::write_value_to()` 等调度入口。
- `caors/serialization.h`：序列化模块聚合入口，包含序列化入口和 STL 容器 traits 适配。
- `caors/object/serialization.h`：对象指针、对象句柄、对象工厂和对象引用 resolver 的序列化适配。
- `caors/resources/serialization.h`：资源 GUID、资源引用、资源依赖收集和资源 resolver 的序列化适配。
- `caors/traits/*.h`：类型名、wrapper 和容器类扩展点主模板。
- `caors/adapters/stl.h`：STL/CAORS 容器、wrapper 和反射 traits 的适配聚合入口。

实现文件按 registry、codec、text writer、JSON writer/parser、DOM/value、writer 接口、DOM
写出、对象序列化、对象反序列化、字段读写、容器读写、中间值构造和文本便捷入口拆分在
`src/caors/serialization/`。共享的字段校验、schema version 过滤和内部调度声明保留在私有
`src/caors/serialization/detail/internal.h` 和 `src/caors/serialization/detail/helpers.h`。

### API 命名规则

序列化公开入口按“动作 + 方向 + 格式”命名，避免同一个函数名同时表达 reader、DOM、JSON 文本和输出
buffer：

- `serialize()` / `deserialize()`：只用于底层 `writer` / `reader` 后端。
- `to_xxx()`：从对象生成一个新的结果，例如 `to_value()`、`to_json()`、`serialization::to_text()`。
- `serialization::write_xxx_to()`：把对象或 DOM 写到调用方提供的 sink，例如
  `serialization::write_json_to()`、`serialization::write_value_to()`。
- `read_xxx_from()`：从调用方提供的 source 读入已有对象，例如 `read_value_from()`、`read_json_from()`。
- `parse_xxx()`：只表示把格式文本、流或路径解析为 DOM，不直接写入 C++ 对象。

有 `serialization::context` 的公开重载统一采用 context-first：`xxx(context, object, ...)`。没有额外上下文时
保留无 context 便捷重载。

### API 速查

| API / 配置 | 用途 | 备注 |
| --- | --- | --- |
| `serialize(object, writer)` | 写入任意 writer 后端 | 适合自定义格式后端 |
| `deserialize(object, reader)` | 从任意 reader 后端读入 | 适合 DOM reader 或自定义 reader |
| `to_value(object)` | 生成 `variant` DOM 中间值 | 根命名空间高频入口，方便工具、调试和多格式转换 |
| `serialization::write_value_to(writer, value)` | 把 `variant` DOM 写入 writer | 不依赖 C++ 对象实例 |
| `to_json(object)` | 输出 JSON 文本 | 根命名空间高频入口，支持 `json_options` 控制格式化 |
| `serialization::to_text()` / `serialization::write_text_to()` | 输出调试用简单文本格式 | 模块入口，`_to` 版本复用调用方 buffer |
| `serialization::write_json_to()` | 输出 JSON 到调用方 buffer 或 `io::output_stream` | buffer 入口适合复用字符串；输出流入口由 `json_writer` 直接写出 |
| `serialization::parse_json()` | 解析 JSON 文本为 `variant` DOM | 支持 parser 侧 `json_options` |
| `serialization::parse_json_from()` | 从 `io::file_system` 路径读取 JSON 并解析为 `variant` DOM | 通过 `io::input_stream` token reader 解析；底层文件系统可选择真实流或内存流 |
| `read_value_from(object, value)` | 从 `variant` DOM 读回对象 | 内部使用 `variant_reader` |
| `read_json_from(object, text)` | 从 JSON 文本读回对象 | 默认使用 `json_reader` 直接按字段读入；开启未知字段保留或严格模式时先解析为 DOM |
| `serialization::read_json_from(object, stream/path)` | 从 `io::input_stream` 或 `io::file_system` 路径读回对象 | 当前解析为 DOM 后复用 `read_value_from()` |
| `serialization::context` | 单次读写上下文 | 按职责使用分组字段配置 |
| `context.type_context` | 本次读写使用的 meta 类型上下文 | 为空时使用 `meta::default_context()` |
| `context.config.fields` | schema version 字段过滤 | `0` 表示不启用版本过滤 |
| `context.config.schema` | schema 兼容策略 | 控制未知字段忽略、保留或严格拒绝 |
| `context.resolvers.external_refs` | 通用外部引用 resolver、解析策略和依赖收集 | 资源、对象等上层引用可共享该入口 |
| `context.resolvers.factories` | 多态读入时使用的通用类型工厂 | 只在需要从 envelope 创建运行期实例时使用 |
| `context.resolvers.construction` | 反序列化新建运行期值时使用的构造策略 | 可按字段、容器元素、wrapper 内部值和 variant 候选定制 |
| `context.resolvers.memory` | 多态值、空拥有型 wrapper 和容器新增元素的运行期构造资源 | 不强行替换已有容器 allocator |
| `context.state.storage` | 借用视图读回所需外部存储 | 主要用于 `std::string_view` |
| `context.config.borrowed_views` | 调用方缓冲区写入策略 | 显式允许长度匹配的可写 `std::span<T>` 接收数据 |
| `context.config.polymorphic` | 多态 envelope 字段名和调试 type name | 默认字段名为 `$type` / `$value` |
| `context.config.limits` | 递归深度等安全限制 | `max_depth = 0` 表示关闭递归深度限制 |
| `json_options` | JSON reader、writer 和 parser 自己持有的格式层选项 | 不放入通用 context；控制格式化和 parser 安全/兼容策略 |

公开序列化入口保留无 context 便捷重载；一旦需要上下文，统一把
`serialization::context` 放在第一个参数：

```cpp
caors::serialize(object, writer);
caors::serialize(context, object, writer);

caors::deserialize(object, reader);
caors::deserialize(context, object, reader);

caors::read_value_from(object, value);
caors::read_value_from(context, object, value);

caors::to_value(object);
caors::to_value(context, object);

caors::serialization::to_text(object);
caors::serialization::to_text(context, object);

caors::to_json(object);
caors::to_json(context, object);

caors::serialization::write_json_to(object, output);
caors::serialization::write_json_to(context, object, output);

caors::read_json_from(object, text);
caors::read_json_from(context, object, text);
```

`serialization::context` 不再提供旧式扁平配置字段。调用方应按职责写入
`context.config.fields`、`context.resolvers.external_refs`、`context.resolvers.factories`、
`context.resolvers.construction`、`context.resolvers.memory`、`context.state.storage` 和
`context.config.limits` 等分组字段。

`context.config.limits.max_depth` 限制对象字段、容器元素、指针和 wrapper 内部值的递归展开深度，
默认值为 `64`。超过限制时返回 `errc::recursion_limit`；设置为 `0` 表示关闭该保护。
这个限制只是防止意外无限递归的安全网，不提供对象图身份追踪、共享实例去重或循环引用恢复。
当前测试覆盖裸指针自环、指针读入深链、拥有型 wrapper 自环和多层 pointer / wrapper
逐层展开路径。

### 单次操作内存

`context.resolvers.memory` 是 `serialization::operation_memory`，明确区分两种资源：

- `result_resource`：用于返回字符串、byte buffer、`variant` DOM、构造后写入目标的拥有型值，以及需要
  跨出当前调用的错误路径数据；为空时由具体结果类型在分配边界解析当前默认资源。
- `scratch_resource`：用于 reader/writer 帧、字段扫描、参数转换和格式解析暂存；为空时使用当前线程的
  `library_temporary_resource()`，公共同步入口会建立 scope 并在返回时回滚。

结果不得引用 scratch。需要从 scratch 数据生成返回字符串、DOM 或对象时，核心实现会 materialize/copy
到结果资源。显式传入的资源均由调用方拥有，并且必须覆盖对应结果或后端对象的生命周期。异步操作不得借用
发起线程的默认 TLS scratch；应为 request/task 提供生命周期覆盖整个异步操作的独立资源。

### 运行期构造策略

反序列化需要新建运行期值时，会按同一套构造顺序尝试：

1. `context.resolvers.construction.construct` 自定义构造策略。
2. 已注册的 `meta::factory_capability`。
3. 已注册且匹配空参数列表的 `meta::constructor_capability`。
4. 类型默认构造能力或资源感知默认构造能力。

自定义构造策略会收到 `serialization::construction_request`，其中包含目标 `type`、所属
`owner_type`、字段名 `field_name`、构造位置 `site` 和容器/tuple/variant 下标 `index`。它适合为不可默认构造
类型提供字段级或容器元素级初始对象。返回的 `construction_result` 必须包含对象指针和匹配销毁函数；如果返回空结果，
序列化层继续尝试后续 fallback。

`construction_result` 是序列化回调的低层桥接结构，保留裸指针是为了兼容自定义构造策略。序列化内部会立即把它转为
`meta::owned_value`，失败路径自动销毁；普通 meta factory / constructor 路径直接使用
`construct_owned()`，不会向公开层暴露裸销毁函数。

序列化模块不直接访问 `meta::detail`。需要创建、托管或转移运行期值时，只使用
`meta::factory_capability`、`meta::constructor_capability`、`meta::owned_value`、
`meta::released_owned_value_guard` 和 `meta::any` 这些公开接口。这样自定义构造策略、factory、
类型默认构造能力和资源感知构造都能通过同一套 RAII 路径保留匹配销毁函数。

当前调用点包括：需要临时值的容器元素、集合元素、关联容器 key/value、空拥有型智能指针 wrapper 的内部值、variant
候选切换、普通多态 wrapper 新建对象，以及字段整体临时值。裸指针仍表示借用，不会通过该策略自动分配内存。

### 多态 object

普通序列化入口默认支持 `meta::object` 的运行期真实类型。写出时，调度层读取对象的
`type_id()`；如果运行期类型不同于调用点声明类型，就按真实类型写出 DOM envelope。动态类型必须已经
注册到当前序列化上下文使用的 meta registry，完成 object、meta 字段和 serialization 注册，并且拥有非
nil `type_guid`。输出 envelope 默认包含：

- `$type`：动态类型的稳定 `type_guid` 字符串。
- `$value`：按动态类型反射字段生成的对象数据。

```cpp
player instance{};
actor& base = instance;

caors::serialization::variant_result value = caors::to_value(base);
caors::serialization::serialization_text_result json = caors::to_json(base);
```

多态 envelope 字段名可以通过 `serialization::context::config.polymorphic` 调整。空配置使用默认字段名；
`write_type_name` 打开后会额外写出调试用类型名，读入时仍只以 `type_guid` 作为类型身份依据：

```cpp
caors::serialization::context context{};
context.config.polymorphic.type_field = caors::string_name{"kind"};
context.config.polymorphic.value_field = caors::string_name{"data"};
context.config.polymorphic.type_name_field = caors::string_name{"debug_type"};
context.config.polymorphic.write_type_name = true;

caors::serialization::serialization_text_result json = caors::to_json(context, base);
```

读写双方需要使用一致的字段名配置。普通反序列化入口读到配置后的 type/value envelope 时，会先在
`context.type_context` 指向的 meta registry 中查找类型，并校验动态类型是声明基类的派生类型。如果目标对象
当前运行期类型和 type 字段一致，则把 value 字段写入这个已有对象；如果没有 type 字段，则按普通声明类型读入。

当前不再提供单独的 `deserialize_polymorphic_*` 兼容入口。需要从 envelope 新建对象的场景应走更高层的
runtime、对象或智能指针 codec 路径，由对应模块在内部使用 `context.resolvers.factories.polymorphic_factory`
负责分配和销毁。

读入流程：

1. 读取 type 字段并用 `type_guid` 查找动态类型。
2. 校验动态类型是调用点声明基类的派生类型。
3. 已有对象读入时，要求目标对象当前 `type_id()` 和 type 字段对应的动态类型一致。
4. 用 value 字段按动态类型反序列化字段。
5. 如果更高层路径需要新建对象，应由该路径负责构造失败回滚和销毁。

当前边界：

- 只支持 `meta::object` 派生类型。
- 写入已有对象不需要对象工厂；需要新建对象的高层路径必须提供匹配的对象或类型工厂。
- 类型身份只写稳定 `type_guid`，不写运行期 `type_id`。
- 不自动接管裸指针、`std::shared_ptr<Base>` 等字段的所有权；这类字段仍按现有指针/wrapper 规则处理。
- 不保留对象图中的共享实例身份和循环引用；需要对象引用时继续使用 `objects::ptr<T>` 和 resolver。

### 拥有型多态 wrapper

对于不走 `objects::manager` 托管的拥有型 wrapper 字段，序列化层支持一版明确边界的多态值语义：

- 支持 `std::unique_ptr<Base>`、`caors::memory::unique_ptr<Base>`、`std::shared_ptr<Base>`
  和 `objects::ref<Base>`。
- `objects::ref<Base>` 要求 `Base` 继承 `meta::object` 和 `objects::ref_counted`。
- 写出时通过 RTTI 查询真实动态类型，动态类型必须已经注册到当前序列化上下文使用的 meta registry。
- 派生类必须通过 typed parent 注册，例如 `.parent<Base>()`，这样注册表同时保存继承关系和安全 upcast 回调。
- 动态类型必须拥有稳定 `type_guid`，并完成 meta 字段和 serialization 注册。
- 读入时优先走 `reader` 的多态 envelope 接口；DOM/JSON 路径由 `variant_reader` 内置支持。自定义 reader
  如果不暴露 `current_variant()`，也可以通过重写 `is_polymorphic_envelope()`、`read_polymorphic_type_guid()`、
  `begin_polymorphic_value()` 和 `end_polymorphic_value()` 接入同一语义。

示例：

```cpp
struct actor {
  virtual ~actor() = default;
};

struct player : actor {
  int level = 0;
};

struct holder {
  caors::memory::unique_ptr<actor> owner;
};

caors::registration::type_<actor>("game.actor")
    .guid(actor_guid)
    .serialization()
    .commit();

caors::registration::type_<player>("game.player")
    .guid(player_guid)
    .parent<actor>()
    .meta()
    .property("level", &player::level)
    .serialization()
    .commit();

caors::registration::type_<caors::memory::unique_ptr<actor>>(
    "caors.memory.unique_ptr<game.actor>")
    .commit();

caors::registration::type_<holder>("game.holder")
    .meta()
    .property("owner", &holder::owner)
    .serialization()
    .commit();
```

字段写出格式和多态 object 相同，默认字段名如下：

```json
{
  "owner": {
    "$type": "5ed39a97-7b20-4d87-86ac-e62a4a615b10",
    "$value": {
      "level": 10
    }
  }
}
```

读入流程：

1. 读取 type 字段并用 `type_guid` 查找动态类型。
2. 校验动态类型继承自字段声明的 `Base`。
3. 通过动态类型 factory 或类型默认构造能力构造派生对象。
4. 用 value 字段按动态类型反序列化字段。
5. 通过 wrapper 的接管函数把对象放入 `unique_ptr<Base>`、`memory::unique_ptr<Base>`、
   `shared_ptr<Base>` 或 `objects::ref<Base>`。

不可默认构造的派生类可以在 meta 注册时提供 factory：

```cpp
void* construct_player(caors::memory_resource* resource);
void destroy_player(caors::memory_resource* resource, void* value) noexcept;

caors::registration::type_<player>("game.player")
    .guid(player_guid)
    .parent<actor>()
    .factory(&construct_player, &destroy_player);
```

反序列化时可以通过 `serialization::context::resolvers.memory.result_resource` 指定构造动态值使用的结果资源：

```cpp
caors::tracking_resource tracking(&caors::get_system_memory_resource());

caors::serialization::context context{};
context.resolvers.memory.result_resource = &tracking;

caors::error status = caors::read_json_from(context, holder, json);
```

资源构造只会在目标 wrapper 能保存匹配销毁回调时启用。当前 `std::shared_ptr<Base>`、
`caors::memory::unique_ptr<Base>` 和 `objects::ref<Base>` 可以安全接管由 `memory_resource`
分配的派生对象；默认
`std::unique_ptr<Base>` 的 deleter 不能保存运行期销毁上下文，因此会回退到标准 `new` / `delete`
或无资源 factory 路径。自定义 wrapper 或自定义 deleter 需要通过 wrapper traits 明确提供匹配的接管语义。

同一个 `context.resolvers.memory.result_resource` 也会用于普通反序列化路径里需要构造并存入目标对象的 `any`、空拥有型智能指针 wrapper
读入非空值时创建内部 `T`、顺序容器新增元素、关联容器 key/value 临时值，以及
`std::variant` 切换候选时构造新的候选值。普通 `std::unique_ptr<T>` 没有可替换的资源释放语义，
仍按标准 deleter 的边界处理。标准容器元素的实际存储由容器自身 allocator 决定，序列化上下文不会强行替换目标容器的 allocator。
如果希望 `caors::vector<caors::string>`、嵌套容器或 `caors::hash_map` 的最终元素也使用某个资源，
目标对象或字段容器本身需要用该资源构造。

构造优先级是：字段或 view 已持有目标值时直接写入已有对象；需要新建对象时，先使用
`context.resolvers.construction` 自定义构造策略，再使用 meta factory、空参数 constructor，最后回退到
类型默认构造能力或 traits 的资源重载。自定义顺序容器可以通过
`traits::sequence_traits<T>::resize(values, size, resource)` 接入 resize 路径；如果 resize 无法创建默认元素，
序列化层会尝试逐个构造临时元素并追加。

构造失败和边界错误按以下语义返回：

- 输入形态不符合约定时返回 `errc::invalid_argument`，例如固定长度数组、pair、tuple 或 variant
  envelope 的数组长度不匹配，或者 variant 下标越界。
- 目标字段不可写、裸指针为空且需要写入非空值、引用 wrapper 不能置空或重绑定时返回
  `errc::invalid_field`。
- 需要序列化层创建新对象但没有可用构造策略、factory、constructor 或默认构造能力时返回
  `errc::invalid_type`。
- 借用视图缺少所需 storage / 调用方缓冲区策略、弱引用缺少 resolver，以及当前刻意禁止的类型形态
  返回 `errc::unsupported`；已提供对应 context 条件后，支持的非拥有类型按各自策略读回。

当前不支持：

- `Base*` 自动创建派生对象；裸指针仍只表示借用。
- `std::optional<Base>`，它按值保存 `Base`，会发生对象切片，不适合作为多态所有权。
- `std::reference_wrapper<Base>` 自动重绑定。
- `std::weak_ptr<Base>` 读写；弱引用应通过对象句柄、资源 id 或 resolver 表达。

### serialization::type_schema

`serialization::type_schema` 是挂载在 meta 类型上的 `serialization::schema_capability`
动态数据。它保存提交后的公开字段 schema；具体输出格式由当前使用的 `writer` 后端决定：

```cpp
caors::registration::type_<actor>("game.actor")
    .meta()
    .property("health", &actor::health)
    .property("debug_only", &actor::debug_only)
    .serialization()
    .field("health", "hp")
    .field("debug_only", {}, true)
    .commit();
```

用户注册时通过 `serialization::field_options` 按字段名引用 meta 属性。公共注册入口会在同一次
`commit()` 中发布 meta 属性和 serialization schema；提交时会校验显式字段配置能解析到对应属性。
运行期读写时，序列化 detail 会基于 `meta::type` 和 `field_schema` 临时编译字段访问、codec
和标量调度缓存。如果显式字段配置找不到对应属性，`commit()` 会返回 `errc::not_found`。

如果 `serialization::type_schema::fields` 为空，`commit()` 会按当前类型已注册属性生成默认字段；
`has_explicit_fields == false` 用于保留默认字段读取语义。

运行期读取 schema 时优先使用不复制数据的类型能力视图：

```cpp
caors::meta::type type = caors::meta::find<actor>();
caors::serialization::schema_view schema =
    type.get_capability<caors::serialization::schema_capability>();

if (schema) {
  for (std::size_t i = 0; i < schema.field_count(); ++i) {
    const caors::serialization::field_schema* field = schema.field_at(i);
  }
}
```

`serialization::find_schema(type)` 是同一能力查询的便捷入口。序列化 schema 查询只接受
`meta::type`，不再提供按 `type_id` 复制 `type_schema` 的旧入口。

### serialization::field_options / field_schema

`serialization::field_options` 描述用户注册时填写的单个字段序列化规则：

- `field_name`：反射字段名。
- `serialized_name`：输出字段名；为空时使用 `field_name`。
- `legacy_serialized_name`：读入时接受的旧字段名；写出仍使用当前 `serialized_name`。
- `field_id`：稳定字段编号，供紧凑二进制、数字键 msgpack 等不写字段名的格式使用。
- `legacy_field_id`：读入时接受的旧字段编号；写出仍使用当前 `field_id`。
- `version_added` / `version_removed`：配合 `serialization::context::config.fields.schema_version` 做字段版本过滤。
- `required`：反序列化时缺失该字段会返回 `errc::not_found`。
- `skipped`：序列化和反序列化时跳过该字段。

`serialization::field_schema` 继承这组配置，不暴露 `meta::property`、codec ops 或字段调度标记。
普通用户只需要通过 builder 填写字段名；序列化运行期会在内部编译 `field_runtime_schema`，
用于保存本次读写需要的属性视图和调度缓存。

`serialization::context::config.fields.schema_version == 0` 表示不启用版本过滤，保持兼容默认行为。传入非 0 版本时：

- `version_added != 0 && schema_version < version_added` 的字段会被跳过。
- `version_removed != 0 && schema_version >= version_removed` 的字段会被跳过。

序列化字段配置属于 serialization extension，不直接修改属性自身。meta 负责描述字段是什么，
序列化负责描述字段如何存储；运行期缓存属于 serialization detail，不属于公开 schema ABI。
调用方应在首次读写前完成类型、属性和 serialization schema 注册。

### serialization::writer

`serialization::writer` 是格式后端接口。序列化调度层负责通过当前 `serialization::context`
定位 meta registry、匹配反射字段和读取字段值；writer 只负责把对象、字段和值写成目标格式。

writer 通过 `serialization::format_features` 声明能力：

- `named_fields`：格式需要字段名，例如 text/json/msgpack map。
- `numeric_fields`：格式需要字段 id，例如紧凑二进制或数字键 map。
- `ordered_fields`：格式按字段顺序写出。
- `self_describing`：格式较自描述，例如文本或 map。
- `schema_required`：格式需要外部 schema 才能解释。

调度层会根据 writer 能力检查字段配置。例如 writer 声明 `numeric_fields` 时，
`serialization::field_schema::field_id` 必须非 0；writer 声明 `named_fields` 时会使用
`serialized_name` 或 `field_name`。

当前内置后端：

- `serialization::text_writer`：声明 `named_fields | ordered_fields | self_describing`。
- `serialization::json_writer`：声明 `named_fields | ordered_fields | self_describing`，默认输出紧凑 JSON。

`text_writer` 和 `json_writer` 默认持有内部 `caors::string`。如果调用方希望复用输出 buffer，可以把
外部字符串传给 writer，或使用 `write_text_to()` / `write_json_to()`：

```cpp
caors::string output;
caors::error status = caors::serialization::write_json_to(instance, output);

caors::serialization::json_writer writer{output};
status = caors::serialize(instance, writer);
```

JSON 可通过 `serialization::json_options` 控制格式化输出。未传入时使用默认紧凑格式：

```cpp
caors::serialization::json_options options{};
options.pretty = true;
options.indent = 2;

caors::serialization::serialization_text_result text =
    caors::to_json(instance, options);

caors::string output;
caors::error status = caors::serialization::write_json_to(context, instance, output, options);
```

根命名空间 `to_json()` 是 JSON 高频便捷封装；`serialization::to_text()` 和
`serialization::write_json_to()` 保留为模块入口。核心序列化入口使用 context-first 形式：
`serialize(context, object, writer)`。没有额外上下文时仍可使用 `serialize(object, writer)` 便捷重载。

### serialization::reader

`serialization::reader` 是格式读入后端接口。反序列化调度层负责通过当前 `serialization::context`
定位 meta registry、匹配反射字段和写入对象；reader 只负责按字段配置提供当前字段值。

当前内置 reader：

- `serialization::variant_reader`：从 `variant` 对象树读取字段，声明
  `named_fields | numeric_fields | self_describing`。
- `serialization::json_reader`：从 JSON 文本范围读取字段，声明
  `named_fields | ordered_fields | self_describing`；对象/数组只缓存成员或元素的文本范围，不构造
  `variant` DOM。

reader 还可以选择支持多态 envelope 读入。默认实现会尝试通过 `current_variant()` 识别 DOM envelope；
如果 reader 只暴露 `current_variant()`，序列化层仍可回退到临时 `variant_reader` 读取 `$value`。
真正的流式 reader 可以不暴露 DOM，只重写以下接口：

- `is_polymorphic_envelope(context)`：判断当前位置是否是 `$type/$value` 结构。
- `read_polymorphic_type_guid(context, guid)`：读取动态类型 GUID。
- `begin_polymorphic_value(context)` / `end_polymorphic_value()`：进入和离开 `$value` 子值。

可以直接从 DOM 中间对象反序列化到已注册反射和序列化元信息的对象：

```cpp
actor instance{};
caors::error status = caors::read_value_from(instance, *value);
```

字段查找优先使用非 0 的当前 `field_id`，再回退到 `legacy_field_id`、当前
`serialized_name` / `field_name` 和 `legacy_serialized_name`。当前支持写入
`bool`、`char`、整数、浮点、`caors::string`、`std::string`、`guid`、`type_guid`、枚举、
已注册反射/序列化元信息的值类型结构体字段、已适配的顺序/集合/关联容器字段、二元组字段、tuple-like 字段、裸指针和普通 wrapper
字段，以及通过 resolver 解析的 `caors::objects::ptr<T>` 字段。

反序列化缺失字段策略：

- 显式配置字段且 `required == false` 时，缺失字段会被跳过；如果对应 `meta::property_schema`
  配置了 `default_value`，读入时会写入默认值，否则目标对象保留已有值。
- 显式配置字段且 `required == true` 时，缺失字段返回 `errc::not_found`。
- 未显式配置字段时，反序列化按全部反射字段读入，缺失字段返回 `errc::not_found`。
- DOM 中存在但未被目标类型声明的未知字段默认忽略。

普通反序列化默认要求输入完整。配置对象等“先有默认值、再用输入局部覆盖”的场景，可以把
`context.config.schema.missing_fields` 设置为 `missing_field_policy::keep_existing` 或
`missing_field_policy::apply_default`。`assets::read_typed_factory_options<T>()` 默认使用
`keep_existing`，因此会保留 `apply_config_defaults()` 和结构体成员初始化得到的值。

未知字段策略通过 `context.config.schema.unknown_fields` 指定：

- `unknown_field_policy::ignore`：默认行为，忽略目标类型 schema 未声明的 DOM 成员。
- `unknown_field_policy::preserve`：把未知成员复制到
  `context.state.schema.preserved_unknown_fields` 指向的兼容列表；如果提供
  `context.state.schema.preserved_unknown_field_entries`，还会记录未知字段所在对象路径、目标 owner type
  和未知 member 本身。两个列表都未提供时等价于忽略。
- `unknown_field_policy::reject`：遇到未知成员立即返回 `errc::invalid_field`。

未知字段策略会递归检查已注册 schema 的嵌套对象、顺序容器、集合容器和关联容器 mapped value。
路径中的 `member` 片段表示对象字段，`element` 表示数组元素，`dictionary_value` 表示字典条目的 value；
弱引用、对象指针和未注册数据仍按各自边界处理，不隐式展开。

字段重命名的第一版策略是给字段配置 `legacy_serialized_name` 或 `legacy_field_id`。读入时接受旧字段，
写出时只输出当前字段名和当前字段 id。属性级通用 schema 默认值可通过
`serialization::apply_config_defaults(config)` 显式恢复；显式序列化字段缺失时也会使用属性
`default_value` 自动补齐。

需要处理字段删除、复杂重命名或旧数据结构转换时，可以在 context 上挂 DOM 迁移函数：

```cpp
caors::serialization::schema_migration_ops migration{
    [](const caors::serialization::schema_migration_request& request,
       caors::variant& value,
       void* user_data) -> caors::error {
      // 根据 request.type / source_version / target_version 改写 value。
      // request.path 指向当前正在迁移的对象路径；根对象为空。
      return {};
    }};

caors::serialization::context context;
context.config.schema.source_version = 1;
context.config.fields.schema_version = 2;
context.config.schema.migration = &migration;
context.config.schema.unknown_fields = caors::serialization::unknown_field_policy::reject;

caors::read_value_from(context, object, value);
```

迁移函数只在 DOM 路径生效，并会按已注册 schema 的对象树递归调度：根对象先迁移，然后进入已知字段中的
嵌套对象、容器元素和关联容器 mapped value。`read_json_from(text)` 默认仍走直接 JSON reader；当启用
migration、缺失字段宽松策略或未知字段保留/严格拒绝时，会先解析为 DOM，再应用迁移、缺失字段和未知字段策略。

### JSON

JSON 写出入口：

```cpp
caors::serialization::serialization_text_result text = caors::to_json(instance);
caors::string output;
caors::error write_status = caors::serialization::write_json_to(instance, output);

caors::io::memory_file_system files;
caors::io::output_stream_result stream = files.open_write("config/player.json");
if (stream) {
  write_status = caors::serialization::write_json_to(instance, *stream.value);
  if (!write_status) {
    write_status = stream.value->close();
  }
}
```

JSON DOM 入口仍然通过 `parse_json()` 解析为 `variant`：

```cpp
caors::serialization::variant_result value = caors::serialization::parse_json(text);
```

JSON 文本对象读入入口会使用 `serialization::json_reader` 直接按字段写回对象，不构造完整 DOM：

```cpp
caors::error status = caors::read_json_from(instance, text);
```

也可以从统一 IO 抽象读取 JSON，而不是直接绑定宿主文件系统路径：

```cpp
caors::io::memory_file_system files;
files.write_all("config/player.json", R"({"id":1})");

caors::serialization::variant_result value =
    caors::serialization::parse_json_from(files, "config/player.json");
caors::error status =
    caors::serialization::read_json_from(instance, files, "config/player.json");
```

`serialization::parse_json(stream)` 和 `serialization::parse_json_from(file_system, path)` 会通过 JSON
token reader 从 `io::input_stream` 读取，不再在序列化层先构造完整 JSON 字符串。解析结果仍是
`variant` DOM，因此对象树本身会完整驻留内存；底层 `file_system` 可以选择真实文件流、内存流或其他
输入流实现。当前 `read_json_from(stream/file_system)` 仍复用这条 DOM 路径；文本
`read_json_from(object, text)` 已经改为 `json_reader` 直接读入。
`serialization::write_json_to(object, output_stream)` 通过 `json_writer` 直接写入输出流，不再先构造完整
JSON 字符串。

当前 JSON parser 支持对象、数组、字符串、整数、浮点、`true`、`false` 和 `null`。字符串支持常见转义、
完整 `\uXXXX` UTF-8 解码和 surrogate pair。字段 id 无法从 JSON 对象名表达，因此 JSON 读入主要按字段名匹配。

`json_options` 同时影响 JSON writer 和 parser：

- `pretty` / `indent`：writer 输出换行和缩进。
- `max_parse_depth`：parser 最大对象 / 数组嵌套深度，`0` 表示不限制。
- `allow_trailing_commas`：是否允许对象和数组末尾多一个逗号。
- `duplicate_key_policy`：对象重复 key 策略，可保留全部、保留第一项、保留最后一项或直接拒绝。

JSON 格式选项不属于 `serialization::context`。需要同时指定跨格式语义和 JSON 行为时，应显式把两者
分别传给 JSON 便捷 API，或先使用选项构造 `json_reader` / `json_writer`，再调用通用
`serialize(context, ...)` / `deserialize(context, ...)`：

```cpp
caors::serialization::json_options options{};
options.pretty = true;

caors::serialization::json_writer writer{output, options};
caors::serialize(context, object, writer);
```

### MessagePack

MessagePack 后端不依赖第三方库。`messagepack_writer` 实现通用 `serialization::writer`，对象字段按
序列化名称写为 map，数组和字典分别写为 MessagePack array 和 map。typed 便捷 API 复用与 JSON 相同的
schema、context、codec 和 `variant_reader` 读入路径：

```cpp
caors::io::byte_buffer_result bytes = caors::to_messagepack(context, object);
if (bytes.succeeded()) {
  caors::error status = caors::read_messagepack_from(context, decoded, bytes.value);
}
```

`messagepack_options` 是 parser 自己持有的格式层限制，不进入 `serialization::context`。当前支持
null、bool、整数、float32/float64、字符串、数组和 map；写出浮点统一使用 float64。bin、extension 和
timestamp extension 尚未实现，遇到这些标记会返回 `errc::unsupported`。

MessagePack 支持 byte span 和 `io::input_stream` 读入，也可以把通用 writer 事件写入
`io::output_stream`：

```cpp
caors::serialization::variant_result value =
    caors::serialization::parse_messagepack(input_stream, options, result_resource);

caors::serialization::messagepack_writer writer{output_stream, scratch_resource};
caors::error status = caors::serialize(context, object, writer);
if (!status) {
  status = output_stream.flush();
}
```

输入流入口会处理短读并一直读取到 EOF，随后解析一个完整根值；它不是增量 parser。输出流入口通过
`output_stream::write_all()` 处理短写。无进展读写和底层失败返回 `error_domain::io`，格式错误返回
`error_domain::serialization`。

`messagepack_options` 的 `max_parse_depth`、`max_string_size` 和 `max_container_size` 会在对应分配前
限制嵌套深度、字符串字节数及单个 array/map 的元素数量，`0` 表示不限制。多字节整数和长度始终按
MessagePack 规定的大端字节序处理。

`encode_messagepack(value, result_resource)` 的返回 buffer 及 `parse_messagepack()` 返回的 DOM
使用结果资源。parser 的流输入缓冲、map 转换暂存和 writer 容器帧使用调用方提供的 scratch 资源；
为空时使用当前线程的库 scratch 资源。返回 buffer 和 DOM 不引用 scratch 资源。

带 `serialization::context` 的 JSON/DOM 便利入口同样使用
`context.effective_scratch_resource()`：JSON reader 的字段、数组和字典扫描帧，JSON writer 的容器帧，
以及 `variant_reader` 的遍历栈都在一次同步操作的 scratch scope 中分配。直接构造这些后端时，可以在
构造函数最后一个参数显式传入 scratch 资源；此时调用方必须保证资源覆盖后端对象生命周期。输出字符串、
DOM 和目标对象仍属于结果/调用方资源，不引用 scratch scope。

MessagePack typed 读入先完整解析 `variant` DOM，再复用 `variant_reader`，因此 schema migration、
未知字段策略、多态 envelope 和 external-ref resolver 与 JSON/DOM 路径一致。格式解析完成前不会修改
目标；进入 typed 字段写入后按字段顺序提交，后续字段失败不会回滚此前字段。整对象事务读取需要单独设计
通用 clone/commit，不属于格式后端职责。

### object pointers

`caors::objects::ptr<T>` 是非拥有对象引用。序列化层不会自行管理对象生命周期，也不会假设
`object_id` 可以跨存档稳定使用。调用方需要在 `serialization::context` 中提供
对象引用 resolver，并挂到通用外部引用入口：

```cpp
caors::objects::object_ref_resolver resolver = make_custom_object_ref_resolver(...);
caors::serialization::context context{};
context.resolvers.external_refs.resolver = &resolver;
```

运行期 resolver 写出时把 `object*` 转换为 `object_id`，输出为无符号整数；空引用输出 `0`。读入时，
resolver 把非 0 `object_id` 转换回对象指针，再写入 `objects::ptr<T>` 字段。持久 resolver 写出
`object_guid` 字符串，空引用写出 `null`。

对象图中的共享关系应通过 `objects::ptr<T>` 表达。序列化层只写出引用身份，不展开被引用对象本体；
同一个对象被多个字段或容器元素引用时会重复写出同一个 id / GUID，读回后 resolver 会把它们解析回同一个
运行期对象指针。循环引用也应按身份引用处理，而不是通过普通 inline value 递归展开。

如果对象由 `objects::manager` 管理，可以直接创建基于 manager 的 resolver：

```cpp
caors::objects::manager manager;
caors::objects::object_ref_resolver resolver = caors::objects::make_object_ref_resolver(manager);
caors::serialization::context context{};
context.resolvers.external_refs.resolver = &resolver;
```

该 resolver 只解析 manager 当前仍持有的对象。对象销毁后，同一个 `object_id` 会解析失败并返回
`errc::not_found`。

如果对象需要跨存档或跨运行期稳定引用，应让 manager 关联 `objects::object_index`，并使用持久
resolver。该模式会把非空对象引用写成 GUID 字符串，空引用写成 `null`：

```cpp
caors::objects::object_index index;
caors::objects::manager manager(index);

player* instance = manager.create_with_guid<player>(persistent_guid);

caors::objects::object_ref_resolver resolver = caors::objects::make_persistent_object_ref_resolver(manager);
caors::serialization::context context{};
context.resolvers.external_refs.resolver = &resolver;
```

持久 resolver 写出时通过 `manager.guid_of(instance->id())` 查找 `object_guid`，读入时通过
`manager.find(object_guid)` 解析对象。对象销毁或 manager 清空后，对应 GUID 会从 index 解绑，
再次读入该 GUID 会返回 `errc::not_found`。

失败策略：

- 未提供 resolver 或 resolver 缺少对应方向函数时返回 `errc::invalid_argument`。
- 非空引用写出时无法映射到 id，或读入 id 找不到对象时返回 `errc::not_found`。
- 读入对象不能转换为 `T*` 时返回 `errc::invalid_type`。

### 资源引用 resolver

`caors::resource_ref<T>` 默认只按稳定 `resource_guid` 读写，不展开资源对象。需要反序列化时立即解析
资源引用时，需要包含 `caors/resources/serialization.h` 或 `caors/resources.h`，并在
`serialization::context` 中挂通用外部引用 resolver：

```cpp
caors::resource_cache<texture> cache;
caors::resources::resource_ref_resolver_ops resolver = caors::resources::make_resource_resolver(cache);

caors::serialization::context context{};
context.resolvers.external_refs.resolver = &resolver;
context.resolvers.external_refs.resolution = caors::resources::resource_ref_resolution::resolve_if_present;

material decoded{};
caors::read_json_from(context, decoded, json_text);
```

当前内置 resolver 创建入口：

- `make_resource_resolver(resources::manager&)`：从非拥有资源索引查找已存在资源。
- `make_resource_resolver(resources::database&)`：从非拥有资源 database 查询已注册 cache 中的资源。
- `make_resource_resolver(resources::cache<T>&)`：从拥有式 cache 查找已加载资源。
- `assets::make_resource_resolver(assets::asset_manager&)`：从高层 asset manager 的内部 database 查询资源。

解析策略：

- `keep_unresolved`：默认策略，只恢复 GUID。
- `resolve_if_present`：尽量解析；缺资源时保持 unresolved 并返回成功。
- `require_resolved`：必须解析成功；缺少 resolver 返回 `serialization / invalid_argument`，缺资源返回
  `resources / not_found`，类型不匹配返回 `resources / invalid_type`。

资源依赖收集通过 `context.resolvers.external_refs.collector` 完成。序列化和反序列化遍历到非空 `resource_ref<T>`
时会记录 `{resource_guid, type_id}`：

```cpp
caors::resources::resource_dependency_list dependencies;
caors::resources::resource_dependency_collector_ops collector =
    caors::resources::make_resource_dependency_collector(dependencies);

caors::serialization::context context{};
context.resolvers.external_refs.collector = &collector;

caors::to_json(context, material_value);

caors::resource_dependency_graph graph;
caors::resources::replace_resource_dependencies(graph, material_resource_guid, dependencies);
```

依赖收集不加载资源，也不改变 `resource_ref<T>` 的 resolved 状态。它主要服务于预加载、打包、
热更新和资源数据库。`resource_dependency_list` 是一次遍历得到的依赖事实列表；`resources::dependency_graph`
是运行期资源数据库里的关系索引。需要把事实写入索引时，应显式调用
`resources::add_resource_dependencies()` 或 `resources::replace_resource_dependencies()`。

推荐的预加载链路是：

1. 先用带 `context.resolvers.external_refs.collector` 的序列化或反序列化遍历收集所有资源 GUID。
2. 如果当前遍历对象本身是资源，调用 `replace_resource_dependencies(graph, resource_guid, dependencies)`
   刷新依赖图。
3. 按依赖条目中的 `guid` 和声明 `type_id` 查询 manifest 或资源数据库，确定资源位置和 loader。
4. 调用 `request_preload`，通过匹配 loader 异步读取并构造资源；来源策略由工程配置提供。
5. 推进所配置的 worker/completion 或宿主循环，检查请求结果后使用已提交的拥有式 cache。
6. 创建 `resources::make_resource_resolver(cache)`，并在后续反序列化 context 中设置
   `resolvers.external_refs.resolver` 和 `resolvers.external_refs.resolution`。

依赖收集器只提供“需要哪些资源”的事实，不决定加载顺序、缺资源策略或异步调度。资源缺失是否报错由后续解析阶段的
`resolvers.external_refs.resolution` 决定。

### object handles

`caors::objects::handle` 和 `caors::objects::typed_handle<T>` 表达已经存在的对象身份，而不是对象地址。
它们序列化时不需要对象引用 resolver，因为句柄本身已经保存 `object_id` 和 `type_id`：

```json
{
  "target": {
    "id": 42,
    "type": 84
  }
}
```

反序列化时只恢复身份，不自动查询全局对象表，也不保证对象仍然存活。调用方需要显式交给
`objects::manager`、场景数据库或资源系统解析：

```cpp
caors::objects::typed_handle<player> target;
caors::read_json_from(target_holder, text);

if (player* current = manager.find(target)) {
  // 句柄当前仍可解析。
}
```

`objects::typed_handle<T>` 的反序列化行为和 `objects::handle` 一样，只写回 `{id,type}`。类型兼容性
在后续解析阶段由 `objects::manager::find(typed_handle<T>)` 或上层 resolver 判断。这样可以避免读入
数据时隐式访问全局状态，也避免序列化层决定对象生命周期。

### serialization codec

`serialization::codec_traits<T>` 是类型级扩展点。编译期已知 `T` 时可以通过 `serialization::codec_of<T>()`
直接取得静态 codec capability；运行期拿得到 `meta::type` 视图时可以通过
`serialization::find_codec(meta::type)` 或
`meta::type::get_capability<serialization::codec_capability>()` 查询 codec 能力。`any` / `any_ref`
和反射字段路径会先从它们携带的 `meta::type` 查询 codec capability。未命中时继续走标量、枚举、
容器 view、wrapper view、pair、tuple、variant 和已注册对象字段递归路径。
`codec_of<T>()`、`find_codec()` 和 `meta::type::get_capability<T>()` 均返回
`serialization::codec_capability`。它负责类型级能力查询，并通过 `bind(any_ref)` 返回
`codec_view`；实例级 `write()`、`read()` 和 `make_value()` 由绑定 view 提供。需要按 C++ 类型查询时
使用 `codec_of<T>()`，已经有 `meta::type` 时使用 `find_codec(meta::type)`。
这样 `guid`、`type_guid` 这类语义上是
原子值的类型，不需要暴露内部字段结构。容器、pair、tuple、variant、裸指针和普通 wrapper 字段优先通过
`meta::type` 生成的视图递归处理内部值，因此这类字段通常不需要显式注册
`serialization::codec_traits<T>`。

codec 和 meta 类型系统的边界：

- 序列化公开 API 使用 `meta::type` 查询 C++ 值本身的基础能力，例如布局、构造、销毁、枚举、容器和
  wrapper 访问。
- `serialization::codec_ops` 描述序列化专用能力，例如写入 writer、从 reader 读回和构造
  `variant` 中间值。它作为 serialization 模块静态能力接入 `meta::type` 能力查询链路，并通过
  `serialization::codec_capability` facade 对外查询。
- `codec_ops::requirement` 描述执行读入前必须由 `serialization::context` 提供的外部条件。
  `none` 不需要附加服务；`borrowed_storage` 需要稳定的借用存储；
  `reference_resolver` 需要外部引用解析器。`caller_buffer` 和 `object_graph` 只允许由对应的专用
  解码入口满足，通用反序列化入口不会隐式提供。
- codec 特化可通过 `static constexpr decode_requirement requirement` 声明条件。调度层会在调用
  `read` 前统一执行 `context::satisfies()` 预检；不满足时返回序列化域的 `unsupported`，不会进入
  codec 实现。
- 编译期已知 `T` 时，内部可以从 `serialization::codec_traits<T>` 静态生成一份 `codec_ops` 函数表，
  并通过 `meta::type` 的静态能力 facade 暴露。
- `any`、`any_ref` 和反射字段这类内部携带 `meta::type` 的路径会先查询该类型的 codec capability，
  因此只要 `codec_traits<T>` 特化在生成该类型视图的翻译单元可见，就不需要额外注册 codec。
- 公开层不再提供 `find_codec(type_id)`。运行期只剩 `type_id` 的场景应由调用方先按自己的上下文解析为
  `meta::type`，再调用 `find_codec(meta::type)` 或 `type.get_capability<codec_capability>()`。

内置 codec 和默认 traits 处理：

- `caors::string`：按字符串读写，默认走 CAORS allocator。
- `std::string`：按字符串读写，用于兼容外部结构。
- `std::string_view`：按字符串写出和构造 DOM；反序列化默认拒绝，只有在
  `serialization::context::state.storage.string_view_storage` 指向外部 `serialization::string_storage`
  时才会读入该存储并让 view 指向其中一段字符串。
- `std::filesystem::path`：按通用路径字符串读写，写出使用 `/` 分隔的 `generic_string()`。
- `std::chrono::nanoseconds` / `microseconds` / `milliseconds` / `seconds` / `minutes` / `hours`：
  按对应 duration 的整数 `count()` 读写。
- `std::chrono::system_clock::time_point`：按毫秒 epoch 整数读写，不包含时区或格式化字符串策略。
- `caors::guid`：按标准 UUID/GUID 字符串读写。
- `caors::type_guid`：按内部 `guid` 的标准字符串读写。
- `caors::resource_guid`：按标准 UUID/GUID 字符串读写。
- `caors::resource_ref<T>`：非空引用按 `resource_guid` 字符串读写；空引用写 `null`，默认读回后只恢复 GUID。
  如果 `serialization::context` 配置了资源 resolver，也可以按策略立即解析到 manager/cache 中已有资源。
- `caors::math::vec2_t<T>` / `vec3_t<T>` / `vec4_t<T>` / `quat_t<T>`：按分量数组读写。
- `caors::math::mat3_t<T>` / `mat4_t<T>`：按行主序扁平数组读写。
- `caors::math::rect_t<T>` / `aabb_t<T>`：按命名对象读写，分别使用 `position/size` 和 `min/max` 字段。
- `caors::math::ray_t<T>` / `sphere_t<T>` / `plane_t<T>` / `frustum_t<T>`：按命名对象读写，分别使用
  `origin/direction`、`center/radius`、`normal/distance` 和 `left/right/bottom/top/near/far` 字段。
  默认短名 `vec2`、`vec3`、`mat4` 等仍是 float 别名，double/int/uint 常用别名也走同一套 codec。
- `caors::vector<T>` / `std::vector<T>`：通过 `traits::sequence_traits<T>` 和 `sequence_view` 输出为数组。
- `caors::list<T>` / `std::list<T>`：通过 `traits::sequence_traits<T>` 和 `sequence_view` 输出为数组。
- `std::forward_list<T>`：通过 `traits::sequence_traits<T>` 和 `sequence_view` 输出为数组；反射随机访问需要线性遍历。
- `std::deque<T>`：通过 `traits::sequence_traits<T>` 和 `sequence_view` 输出为数组。
- `std::array<T, N>`：通过 `traits::sequence_traits<T>` 和 `sequence_view` 输出为数组；读入长度必须等于 `N`。
- `std::span<T>` / `std::span<T, N>`：通过 `traits::sequence_traits<T>` 输出为数组；默认拒绝读回，
  显式开启 `context.config.borrowed_views.allow_caller_buffer_write` 后可写入长度完全一致的可写 span。
- `caors::hash_set<T>` / `std::unordered_set<T>` / `std::set<T>`：通过 `traits::set_traits<T>` 和 `set_view`
  输出为数组，读入时拒绝重复元素。
- `caors::hash_map<K, V>` / `std::unordered_map<K, V>` / `std::map<K, V>`：通过
  `traits::associative_traits<T>` 和 `associative_view` 输出为字典 pair 数组。
- `std::variant<Ts...>`：通过 `traits::variant_traits<T>` 输出为 tagged union 数组 `[index, value]`。

`caors/serialization.h` 会包含 STL 容器 traits 适配和 math codec 适配，常见容器字段和内建 math
字段不需要额外 include。自定义容器只需要在
`caors::traits` 命名空间特化对应容器 traits；序列化层不再提供 `sequence_codec`、`set_codec`
或 `associative_codec` 这类容器级 codec 基类，避免容器递归和 `meta::type` / view 路径重复实现。

内置原子 codec 也是 `serialization::codec_traits<T>` 特化，不要求内置类型显式 meta 注册，也不再维护独立的
运行期 codec 索引。内置 codec 和外部自定义 codec 都通过同一套函数表构造逻辑生成
`codec_ops`，并通过 `meta::type` 静态能力参与查询。携带 `meta::type` 的 `any` / `any_ref` /
反射字段路径会优先使用 codec capability。
如果没有命中 codec，序列化会回退到 `meta::scalar_ops` 标量能力；这条路径覆盖 bool、字符、整数、
浮点、`std::string`、`caors::string`、`std::string_view` 和 `core::string_name` 等基础类型。
因此基础标量和常见字符串不需要显式注册 codec，也不要求先完成 meta 注册。用户自定义 codec 始终优先于
默认标量 fallback。

反射字段中的容器、pair、tuple、variant、裸指针和普通 wrapper 不要求显式调用
额外 codec 注册，也不要求每一种容器模板组合都注册为 meta 类型。字段类型会根据
`traits::sequence_traits<T>`、`traits::set_traits<T>`、`traits::associative_traits<T>`、`traits::pair_traits<T>`、`traits::tuple_traits<T>` 或
`traits::variant_traits<T>` 生成运行期访问能力，序列化层随后按元素、key、first、候选下标和 value 递归处理。
递归用到的业务类型如果需要稳定名称、GUID、反射字段、多态构造或对象工厂，仍应通过
`registration::type_<T>("name")` 显式注册。自定义容器扩展示例：

### 配置校验 schema

`serialization/config_validation.h` 提供基于反射属性 schema 的 typed config 校验。推荐流程是先把
`variant` / JSON 反序列化成明确的配置结构体，再调用 `serialization::validate_config(config)`：

```cpp
struct texture_config {
  std::string format;
  int priority = 1;
};

caors::registration::type_<texture_config>("game.texture_config")
    .meta()
    .property("format", &texture_config::format,
              caors::meta::registration::required(),
              caors::meta::registration::non_empty(),
              caors::meta::registration::one_of({"json", "binary"}),
              caors::meta::registration::default_value("json"))
    .property("priority", &texture_config::priority,
              caors::meta::registration::range(1, 100),
              caors::meta::registration::default_value(1))
    .serialization()
    .commit();

texture_config config;
caors::serialization::apply_config_defaults(config);
caors::read_value_from(config, options_value);
caors::serialization::config_validation_result result =
    caors::serialization::validate_config(config);
```

当前内置约定键包括：

- `required`：字段必须有有效值。对 typed C++ 对象来说，标量字段天然存在；字符串、`string_name`
  等空值会被视为缺少有效值。
- `non_empty`：字符串、`string_view`、`string_name` 等文本字段不能为空。
- `range_min` / `range_max`：数值字段范围。
- `one_of`：字符串字段允许值数组。
- `default_value`：通过 `apply_config_defaults(config)` 显式写回默认值。

校验结果会一次收集多条 `config_validation_diagnostic`。这套机制适合资源 factory options、导入配置、
编辑器面板和工具配置。`validate_config()` 只读取 `meta::property_schema`，不再识别旧 metadata key。
跨字段关系、条件约束和更完整 schema 策略仍放在后续路线图中。

```cpp
namespace caors::traits {

template <typename T>
struct sequence_traits<my_sequence<T>> {
  static constexpr bool is_sequence = true;
  using element_type = T;

  static std::size_t size(const my_sequence<T>& values) noexcept;
  static const element_type* element(const my_sequence<T>& values, std::size_t index) noexcept;
  static element_type* mutable_element(my_sequence<T>& values, std::size_t index) noexcept;
  static void clear(my_sequence<T>& values);
  static bool resize(my_sequence<T>& values, std::size_t size);
  static bool append(my_sequence<T>& values, const element_type& value);
};

}
```

当前 text/json writer 使用 `[[key, value], ...]` 的 pair 数组表示字典，因此可以支持非字符串 key。
DOM reader 同时接受 `variant::dictionary` 和 pair 数组形态。普通 `std::pair<T, U>` 字段会写成
`[first, second]`，读入时数组长度必须等于 2，长度不匹配会返回失败。
普通 `std::tuple<Ts...>` 字段会写成 `[v0, v1, ...]`，读入时数组长度必须等于 tuple 元素数量。
普通 `std::variant<Ts...>` 字段会写成 `[index, value]`，`index` 是当前候选下标；读入时数组长度必须等于 2，
`index` 必须在候选范围内。目标对象已经处于相同候选时会更新已有值；切换到其他候选时，该候选类型需要支持默认构造。

外部类型可以通过特化 `serialization::codec_traits<T>` 把某个类型作为原子值处理。编译期已知 `T`
时可直接使用 `serialization::codec_of<T>()`；运行期已持有 `meta::type` 时可使用
`serialization::find_codec(type)` 或 `type.get_capability<serialization::codec_capability>()`。
codec 可以分别提供 writer、reader 和 DOM 构造函数；没有提供的方向会继续走默认逻辑或返回不支持。
带 context 的 codec 扩展点同样使用 context-first 形式；如果 codec 只需要上下文路径，可以只提供
context-first 版本：

```cpp
namespace caors::serialization {

template <> struct codec_traits<custom_value> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  static caors::error write(const serialization::context& context,
                            writer& writer,
                            const custom_value& value);
  static caors::error read(const serialization::context& context,
                           reader& reader,
                           custom_value& value);
  static variant_result make_value(const serialization::context& context,
                                   const custom_value& value);
};

}
```

### 指针和 wrapper 字段

反射层把裸指针和普通 wrapper 拆成“外层持有者”和“内部 value”两层。序列化层只处理内部 value：

- `T*`、`std::unique_ptr<T>`、`caors::memory::unique_ptr<T>`、`std::shared_ptr<T>`、
  `std::optional<T>`、`std::reference_wrapper<T>` 和 `caors::objects::ref<T>` 会按内部 `T` 写出。
- 空裸指针、空智能指针、空 `optional` 或空 `objects::ref<T>` 写出为 `null`；
  `std::reference_wrapper<T>` 不表示空值。
- 读入 `null` 会重置裸指针、`std::unique_ptr<T>`、`std::shared_ptr<T>`、`std::optional<T>`
  和 `objects::ref<T>`；写入 `std::reference_wrapper<T>` 时返回 `errc::invalid_field`。
- 读入非空值时，裸指针要求目标指针已经指向有效对象，不会自动分配内存。
- 读入非空值时，`std::unique_ptr<T>`、`std::shared_ptr<T>` 和 `objects::ref<T>` 会在内部为空时尝试使用上下文构造策略、factory、
  constructor 或默认构造创建 `T` 并让 wrapper 接管新对象；`std::optional<T>` 会按同一构造策略创建临时
  `T`，再按值 move/copy 到 optional 内部，临时对象仍由序列化层销毁；`std::reference_wrapper<T>` 会写入已经绑定的对象。
- 如果 `T` 不支持默认构造，已经存在的 `T` 对象仍可被字段反序列化写入；空智能指针和需要新增元素的容器只有在上下文构造策略或已注册
  factory / constructor 能创建 `T` 时才会读回成功；空 `optional<T>` 读入不可默认构造 `T` 也遵循同一规则。
- 裸指针、`std::reference_wrapper<T>` 和普通 inline wrapper 都按借用值递归处理。字段形成父指针、
  双向链表、自环或共享对象图时，调用方应跳过反向字段或改用显式引用；`context.config.limits.max_depth`
  只负责在递归过深时返回 `errc::recursion_limit`。
- 多层组合会逐层通过 `indirect_view` 展开，例如 `std::optional<std::optional<T>>` 和
  `std::shared_ptr<std::optional<T>>` 会最终读写内部 `T`。遇到空中间层时，拥有型 wrapper 和
  `optional` 会按当前构造策略创建下一层；裸指针仍要求目标已经指向有效对象。
- `std::weak_ptr<T>` 仍不作为普通 value 序列化；`objects::weak_ref<T>` 通过
  `context.resolvers.external_refs.resolver` 按对象身份读写，不展开对象本体。
- `caors::objects::ptr<T>` 不走内部 value 语义，仍通过 `objects::object_ref_resolver` 写出和读入对象 id 或持久 GUID。
- 当 `std::unique_ptr<Base>`、`caors::memory::unique_ptr<Base>`、`std::shared_ptr<Base>`
  或 `objects::ref<Base>`
  实际持有已注册派生类对象时，序列化层会写出多态 envelope，默认字段名为 `$type` / `$value`。读回时要求派生类型可通过
  上下文构造策略、`registration::type_<T>(name).meta().factory()`、空参数 constructor 或类型默认构造能力创建，并通过 wrapper 接管新对象。
- 如果反序列化上下文提供 `context.resolvers.memory.result_resource`，`std::shared_ptr<T>`、
  `caors::memory::unique_ptr<T>` 和 `objects::ref<T>` 可以保存运行期销毁回调，让资源分配和释放匹配。普通
  `std::unique_ptr<T>` 使用标准 deleter，不能安全接管资源构造出的运行期派生对象，因此会回退到
  默认 `new/delete` 或非资源 factory 路径。
- `std::optional<T>` 在读入非空值且内部为空时不会接管外部分配出的指针，只会从临时 `T`
  按值写入自身；如果 move/copy 到 optional 失败，原临时对象会被匹配销毁函数清理。
- `objects::ref<T>` 读入非空普通值且内部为空时会通过
  `objects::make_ref<T>(context.resolvers.memory.result_resource)` 创建对象；引用计数本身不写入存档。
  读入多态 envelope 时会按 `$type` 构造真实派生对象，并让 `objects::ref<Base>` 接管对应控制块和销毁回调。

这条规则避免序列化层隐式决定所有权。后续如果要让某类 wrapper 采用引用语义、共享身份或自定义构造策略，
应在字段策略或专用 wrapper trait 中显式声明。

## 当前类型处理边界

当前实现优先覆盖可明确表达所有权和生命周期的类型。以下限制是刻意保留的边界，后续应逐项解决：

| 类型形态 | 反射 / any 访问 | 序列化写出 | 反序列化读回 |
| --- | --- | --- | --- |
| 基础标量、枚举、`caors::string`、`std::string` | 保存真实 C++ 类型，可读写字段；枚举可通过 `meta::type` 的枚举能力转换 | 支持 | 支持 |
| `std::string_view` | 保存 view 本身，不拥有字符存储 | 支持，按字符串写出 | 默认拒绝；可通过 `serialization::context::state.storage.string_view_storage` 指向外部 `string_storage` 后读回 |
| `std::filesystem::path` | 作为普通值类型 | 支持，按通用路径字符串写出 | 支持，从字符串构造 path |
| `std::chrono` duration / `system_clock::time_point` | 作为普通值类型 | 支持，按整数写出 | 支持，从整数构造 |
| `guid`、`type_guid`、`resource_guid` | 作为普通值类型 | 支持，按字符串写出 | 支持，从字符串解析 |
| `resource_ref<T>` | 保存稳定资源 GUID 和可选已解析资源指针；资源序列化适配只处理 GUID | 空值写 `null`，非空写 `resource_guid` 字符串，不展开资源对象；可收集依赖 | `null` 会清空引用；非空字符串默认只恢复未解析 GUID，也可通过 `context.resolvers.external_refs.resolver` 按策略解析 |
| 裸指针 `T*` | 通过 `indirect_view` 访问已指向的 `T` | 空值写 `null`，非空展开内部值 | `null` 可重置；非空要求目标已经指向有效对象 |
| `std::unique_ptr<T>`、`caors::memory::unique_ptr<T>`、`std::shared_ptr<T>`、`std::optional<T>` | 通过 `indirect_view` 访问内部 `T` | 空值写 `null`，非空展开内部值；拥有型智能指针可写出普通类多态 envelope | `null` 可重置；拥有型智能指针非空读入可通过上下文构造策略、factory、空参数 constructor 或默认构造创建内部值并接管所有权；`std::optional<T>` 非空读入会用同一构造策略创建临时值，再按值 move/copy 到 optional；空 wrapper 读入非空值但 `T` 不可构造时返回错误；需要 `memory_resource` 托管 unique pointer 时使用 `caors::memory::unique_ptr<T>` |
| `objects::ref<T>` | 通过 `indirect_view` 访问引用计数对象内部 `T` | 空值写 `null`，非空展开内部值，不写引用计数；`ref<Base>` 持有派生对象时可写出多态 envelope | `null` 可重置；普通非空值可在 `T` 默认可构造时通过 `objects::make_ref<T>(context.resolvers.memory.result_resource)` 创建对象；多态 envelope 可构造真实派生对象并接管匹配销毁回调 |
| `std::reference_wrapper<T>` | 通过 `indirect_view` 访问已绑定的 `T` | 按内部 `T` 写出 | 只能写入已绑定对象，不能置空或重绑定 |
| `std::weak_ptr<T>` | 只标记为不支持 wrapper，不展开 | 不支持 | 不支持 |
| `objects::weak_ref<T>` | 弱引用本身不暴露普通 inline value | 通过 resolver 写出 `object_id` 或 `object_guid`；空或已失效引用写 `null` / `0` | 通过 resolver 找到仍由 `objects::ref<T>` 管理的对象并恢复弱引用 |
| `objects::ptr<T>` | 作为对象指针类型，可通过 `any_ref::get_capability<objects::ptr_capability>()` 识别 | 通过 resolver 写出 `object_id` 或 `object_guid` | 通过 resolver 从 `object_id` 或 `object_guid` 写回引用 |
| `objects::handle`、`objects::typed_handle<T>` | 作为对象身份句柄，可通过 `any_ref::get_capability<objects::handle_capability>()` 识别 | 写出 `{id,type}`，不访问 resolver | 读回 `{id,type}`，只恢复身份，不自动解析对象 |
| 顺序容器、集合、关联容器 | 通过 `sequence_view`、`set_view`、`associative_view` 访问 | 支持已适配 traits 的类型 | 支持拥有型容器；新增元素或临时 key/value 可走上下文构造策略，仍不可构造时返回 `errc::invalid_type`；借用视图默认拒绝 |
| `std::span<T>` / `std::span<T, N>` | 借用顺序视图，元素是否可写由 `T` 和字段可写性决定 | 支持，按数组写出 | 默认拒绝；显式允许调用方缓冲区写入后，长度完全一致的可写 span 可以读回 |
| `std::array<T, N>` | 固定长度顺序视图 | 支持 | 支持，但输入长度必须等于 `N`；长度不匹配返回 `errc::invalid_argument` |
| `std::pair`、`std::tuple`、`std::variant` | 通过 pair / tuple / variant 视图访问 | 支持 | 支持；pair / tuple 长度、variant envelope 长度或下标错误返回 `errc::invalid_argument`；切换 variant 候选时目标候选可走上下文构造策略、factory、constructor 或默认构造 |

- `std::string_view` 默认拒绝反序列化写回，避免把临时字符串读入成悬空引用。调用方如果提供
  `serialization::context::state.storage.string_view_storage`，序列化层会把每个读入文本追加为独立字符串，再让字段
  view 指向对应字符串；调用方必须保证该 storage 生命周期长于目标对象或目标字段。
- `resource_ref<T>` 不拥有资源，也不延长资源生命周期。默认反序列化只恢复 GUID；运行期
  `resource_generation` 不写入存档。调用方可以后续按 GUID 使用
  `resources::manager::resolve()` 或 `resources::cache<T>::resolve()` 绑定实际资源对象，也可以在
  `serialization::context` 中设置 `resolvers.external_refs.resolver` 和
  `resolvers.external_refs.resolution` 让读入过程立即解析。
- `resource_handle<T>` / `resource_weak_handle<T>` 是运行期所有权句柄，当前不作为默认序列化 codec。
  需要持久化资源关系时应序列化 `resource_ref<T>` 或稳定 `resource_guid`。
- `std::weak_ptr<T>` 默认不展开内部值。`objects::weak_ref<T>` 使用与 `objects::ptr<T>` 相同的
  external-ref 身份格式；写出期间会短暂提升为强引用，读回目标必须仍绑定 `objects::ref<T>` 控制块。
  resolver 未命中、动态类型不匹配或对象没有控制块时返回错误并保留字段原值。resolver 返回的裸对象
  在解析和建立弱引用期间必须保持存活，跨线程销毁由调用方与 resolver 自行同步。
- 裸指针、普通智能指针和 `objects::ref<T>` 按 inline value 处理，不保留对象身份、共享关系或对象图引用；
  字段存在回边或自环时应由调用方跳过对应字段，或改用显式引用类型。`context.config.limits.max_depth`
  会阻止无限展开，但不会把对象图转换为引用图；需要身份保持时使用 `caors::objects::ptr<T>`、
  `caors::objects::handle` 或 `caors::objects::typed_handle<T>`。
- 未注册枚举可以通过 `meta::type` 在字段、`any` / `any_ref` 和容器元素路径上按底层整数读写；如果枚举需要稳定名称、GUID 或多态身份，仍应显式注册。
- 容器、指针、wrapper、pair、tuple 和 variant 通过 `meta::type` 生成运行期视图，不要求每一种
  `std::vector<T>`、`std::map<K, V>` 或嵌套组合都注册为 meta 类型。元素或候选值如果需要稳定名称、
  GUID、多态构造或对象工厂，仍应作为业务类型显式注册。
- 多层指针 / wrapper 组合按每一层 `indirect_view` 递归展开，受
  `context.config.limits.max_depth` 保护。空裸指针、不可写引用 wrapper、弱引用或所有权不明确的中间层仍会返回
  `errc::invalid_field` 或 `errc::unsupported`，不会隐式分配 backing storage 或决定弱引用语义。
- `std::array<T, N>` 已支持固定长度数组读写；反序列化时数组长度必须等于 `N`，长度不匹配会返回
  `errc::invalid_argument`。
- `std::span<T>` / `std::span<T, N>` 已支持按借用数组视图写出和反射访问；默认不支持反序列化写回。
  调用方显式开启 `context.config.borrowed_views.allow_caller_buffer_write` 后，只允许写入长度完全一致的
  `std::span<T>`；所有元素会先读入暂存值，读取失败不会部分修改调用方缓冲区。
  `std::span<const T>` 始终拒绝读回。
- 不可默认构造类型可以作为已有直接字段、已持有 wrapper 内部值或 `variant` 当前候选被写入；需要序列化层自动实例化对象时，
  可以通过 `context.resolvers.construction`、`registration::type_<T>(name).meta().factory()` 或空参数
  `registration::type_<T>(name).meta().constructor<>()` 提供构造能力。没有可用构造策略时仍返回 `errc::invalid_type`。
- `serialization::context::resolvers.memory.result_resource` 会用于资源感知 `any` 默认构造、空 `std::shared_ptr<T>`、
  空 `caors::memory::unique_ptr<T>`、空 `objects::ref<T>` 的内部值创建、顺序容器新增元素、
  `std::variant` 切换候选、关联容器 key/value 临时值，以及普通类多态值 factory。容器最终存储仍由目标容器
  自身 allocator 管理；上下文不会替换已存在容器的 allocator。标准顺序容器适配会把该资源传给新增的
  allocator-aware 元素。
- `std::pair<T, U>` 已支持按二元素数组 `[first, second]` 读写；反序列化时数组长度必须等于 2，
  否则返回 `errc::invalid_argument`。
- `std::tuple<...>` 已支持按元素数组 `[v0, v1, ...]` 读写；反序列化时数组长度必须等于 tuple
  元素数量，否则返回 `errc::invalid_argument`。
- `std::variant<...>` 已支持按 tagged union 数组 `[index, value]` 读写；反序列化时数组长度必须等于 2，
  `index` 必须在候选范围内，否则返回 `errc::invalid_argument`。相同候选会更新已有值；切换候选时目标候选
  需要可通过上下文构造策略、factory、空参数 constructor 或默认构造创建，否则返回
  `errc::invalid_type`。处于无值状态的 variant 当前返回不支持。
- 直接值字段仍按字段声明静态类型处理；运行期动态类型记录发生在 `meta::object`
  声明类型和真实类型不一致时，以及拥有型智能指针 wrapper envelope 上。
- JSON 文本 reader 已支持按字段直接读入；当启用 schema migration、缺失字段宽松策略、未知字段保留或严格拒绝时会先走
  DOM parser，以便检查、跳过或改写完整对象成员。输入流 / 文件系统便捷读入仍走 DOM parser。二进制格式、跨对象迁移调度
  和未知字段按对象树位置保留尚未实现。

### caors::variant

`caors::variant` 是序列化 DOM 中间对象。它保存的是已经脱离 C++ 精确类型的运行期数据，可以保存：

- `null`
- `bool`
- 有符号整数
- 无符号整数
- 浮点数
- 字符串
- 数组
- 字典
- 对象

字典条目由 `variant_entry` 表示，key 和 value 都是 `variant`：

```cpp
struct variant_entry {
  caors::variant key;
  caors::variant value;
};
```

对象成员由 `variant_member` 表示，保留输出字段名、稳定字段 id 和字段值：

```cpp
struct variant_member {
  caors::string_name name;
  std::uint32_t field_id;
  caors::variant value;
};
```

可以从已注册反射和序列化元信息的对象构造中间树：

```cpp
caors::serialization::variant_result value = caors::to_value(instance);
```

也可以把中间树写入任意 writer：

```cpp
caors::serialization::text_writer writer;
caors::serialization::write_value_to(writer, *value);
```

DOM 是可选路径，适合工具、编辑器、测试对比和格式转换。性能敏感路径仍可直接使用
`serialize(instance, writer)`，避免构造整棵中间树。

`variant` 提供 `is_*()` 和 `try_as_*()` 安全访问器，避免在类型不匹配时误读默认存储字段。
数组、字典和对象可以通过 `push_back()`、`add_entry()`、`add_member()`、`find_member(name)`
和 `find_member(field_id)` 做基础修改和查询。只需要成员值时，优先使用
`find_member_variant(name)` / `find_member_variant(field_id)`，避免调用方手动解开
`variant_member::value`。

运行期 C++ 值可以显式转换为 `variant`：

```cpp
caors::serialization::variant_result value = caors::serialization::to_variant(caors::any::from(42));
```

`serialization::to_variant(any)` 不要求基础标量或字符串预先注册到 meta；它会先查询 serialization codec，
再回退到内置标量和字符串转换。`serialization::to_variant(any_ref)`
会先尝试复制引用指向的对象；不可拷贝引用会返回 `errc::unsupported`。

### 高级用法：序列化 schema 注册

序列化 schema 作为 extension 挂在对应 `meta::type` 上。业务类型通过统一 registration builder
挂载序列化 schema：

```cpp
caors::error status =
    caors::registration::type_<actor>("game.actor")
        .meta()
        .property("id", &actor::id)
        .property("health", &actor::health)
        .serialization()
        .field("id")
        .field("health")
        .commit();

if (status) {
  // 处理重复注册、缺失类型等错误。
}

caors::meta::type type = caors::meta::find<actor>();
caors::serialization::schema_view schema = caors::serialization::find_schema(type);
```

热点路径查询序列化 schema 时使用 `serialization::find_schema()` 或
`type.get_capability<serialization::schema_capability>()`，这两个入口都不会复制 schema。重复注册同一个
类型会返回 `errc::duplicate_id`，已有注册数据保持不变。显式 `.field()` 必须能在提交时解析到
对应属性；未显式填写字段时会按当前属性列表生成默认 schema。序列化 schema capability 注册推荐在启动期或
模块加载期完成；`meta::init_registry()` 只执行延迟注册并校验注册表，不会阻止后续继续注册。
进入只读运行期后可以调用 `meta::freeze_registry()` 冻结 primary 写入层，之后新的 schema 注册会返回
`errc::frozen`；插件 schema 应通过 registry layer 发布。

### serialization::to_text

`serialization::to_text()` 使用已注册的反射字段读取对象：

```cpp
caors::registration::type_<actor>("game.actor")
    .meta()
    .property("id", &actor::id)
    .property("health", &actor::health)
    .serialization()
    .field("id")
    .field("health")
    .commit();

actor instance{7, 95.5F};
caors::serialization::serialization_text_result result = caors::serialization::to_text(instance);
caors::string text = *result;
```

输出格式是内部简单文本格式，例如：

```text
{"id":7,"health":95.5}
```

当前支持的字段值：

- `bool`
- 字符和整数类型
- 浮点类型
- `caors::string`
- `std::string`
- `std::string_view`
- 基于 traits 的容器、pair、tuple 和 variant 字段
- 已注册反射和序列化元信息的嵌套对象
- 裸指针、普通 wrapper 字段和 `caors::objects::ptr<T>`

遇到不可拷贝字段、未支持字段类型、无 resolver 的对象引用或超出当前类型边界的字段时返回错误。

## 当前已实现能力

- `serialization::type_schema`：序列化 schema capability 的动态数据。
- `serialization::field_schema`：字段级序列化配置。
- `serialization::writer`：格式后端接口。
- `serialization::reader`：格式读入后端接口。
- `serialization::context`：单次序列化/反序列化上下文，支持指定 `type_context`、schema version 字段过滤、多态 envelope 字段名配置、通用外部引用 resolver/collector、对象工厂和运行期资源传递。
- `serialization::string_storage`：`std::string_view` 反序列化使用的多段字符串存储。
- `serialization::external_ref_resolver_ops`：通用外部引用解析函数表，资源模块通过 adapter 接入。
- `serialization::external_ref_collector_ops`：通用外部引用收集函数表，资源模块用它收集资源依赖。
- `serialization::codec_traits<T>`：类型级 codec 特化入口。
- `serialization::codec_of<T>()`：按 C++ 类型查询静态 codec capability；类型不要求已经显式 meta 注册。
- `serialization::find_codec(meta::type)`：按 meta 类型查询静态 codec capability。
- `serialization::codec_capability`：类型级 codec 查询 facade，通过 `bind(any_ref)` 创建实例 view。
- `serialization::codec_view`：绑定实际运行期值，负责 codec 的 write、read 和 make_value。
- `serialization::schema_capability`：类型级 schema 查询 capability，动态数据为 `type_schema`。
- `serialization::schema_view`：序列化 schema 的只读查询视图。
- `serialization::find_schema()` / `serialization::contains()`：基于 `meta::type` 查询是否挂载了序列化 schema。
- `serialization::size(meta::context&)`：统计指定类型上下文中已注册序列化 schema 的数量；无参数版本使用默认类型上下文。
- `serialization::text_writer`：简单文本格式后端。
- `variant`：序列化 DOM 中间对象。
- `serialization::to_variant()`：把运行期 `any` / `any_ref` 转换为 `variant`。
- `serialization::variant_reader`：从 DOM 中间对象读入字段的后端。
- `serialization::json_reader`：从 JSON 文本范围直接读入字段的后端。
- `serialization::messagepack_writer`：把通用 writer 事件编码到拥有式字节缓冲或输出流。
- `serialization::encode_messagepack()` / `serialization::parse_messagepack()`：MessagePack DOM
  编解码入口，支持 byte span、输入流、结果资源和 scratch 资源。
- `serialization::to_messagepack()` / `serialization::read_messagepack_from()`：MessagePack typed
  便捷入口。
- `variant_member`：对象成员，保存字段名、字段 id 和字段值。
- `variant_entry`：字典条目，保存 key 和 value。
- `serialize()`：通过指定 writer 后端序列化对象。
- `deserialize()`：通过指定 reader 后端反序列化对象。
- `to_value()`：通过反射和序列化元信息构造中间对象树。
- `serialization::write_text_to()` / `serialization::write_json_to()`：把对象写入调用方提供的 `caors::string` buffer 或输出流。
- `serialization::write_value_to()`：把中间对象树写入指定 writer。
- `serialization_text_result`：`caors::result<caors::string>` 文本序列化结果。
- `variant_result`：`caors::result<variant>` 中间值构造结果。
- `serialization::to_text()`：基于反射字段的简单文本序列化。
- 基础标量和字符串字段输出。
- 字段别名和跳过配置。
- 反序列化 required/optional 缺失字段策略。
- DOM 反序列化默认忽略未知字段。
- 基础 schema version 字段过滤。
- 内置 string、string_view、guid 和 type_guid codec。
- 裸指针、普通 wrapper、`objects::ptr<T>` 和已适配容器字段。
- 运行期 `object_id` resolver 和持久 `object_guid` resolver。
- 基于稳定 `type_guid` 的 `meta::object` 运行期真实类型序列化。
- 基于稳定 `type_guid`、RTTI、typed parent upcast 和可选 factory 的普通类拥有型智能指针多态序列化。
- `variant` 安全访问器、数组/字典/对象辅助修改和成员查询。
- 通过统一 registration builder 一次性注册 meta 字段和序列化 schema。

## 相关入口

- 公共聚合头：`include/caors/serialization.h`。
- 按职责拆分的公共头位于 `include/caors/serialization/`。
- 行为测试：`tests/serialization/`。
