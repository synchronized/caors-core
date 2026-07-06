<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Resources

本文档说明 CAORS 资源系统的当前设计边界。资源系统仍处于基础阶段，当前提供资源身份、轻量引用模型、
非拥有资源索引、拥有式缓存、资源清单、字节来源 provider、加载/保存接口和最小序列化读写 helper；异步加载、
资源包和序列化 resolver 后续再补。

## 设计目标

资源系统第一阶段只解决几件事：

- 资源有跨进程、跨存档稳定的 `resource_guid`。
- 资源对象可以保存自己的稳定身份。
- 资源引用可以先保存 GUID，在资源加载后再解析成实际资源指针。
- 资源管理器可以保存非拥有索引，用 GUID 定位已经存在的资源对象。
- 资源缓存可以按 GUID 拥有资源对象，并复用 manager 做查找和引用解析。
- 资源清单可以把稳定 GUID 映射到资源位置、类型、版本和 hash。
- 资源 provider 可以把稳定 GUID 映射到字节来源和位置，作为资源数据库或调试 fileserver 的运行期读取入口。
- 资源加载器和保存器先只定义类型化接口，不绑定路径、文件系统或异步框架。
- 底层资源模型不绑定具体引擎、文件系统、异步加载框架或编辑器管线。

当前不会实现：

- 资源包或网络加载后端。
- 拥有式全局资源缓存。
- 自动引用计数。
- 热重载。
- 资源导入管线。
- 与对象系统的场景树生命周期绑定。

这些能力后续应建立在 `io::file_system`、`resources::provider`、`resources::manager`、
`resources::loader` / `resources::saver` 和上层资源数据库之上。

## 命名空间

资源系统主定义位于 `caors::resources` 命名空间，例如：

- `resources::resource_guid`
- `resources::resource`
- `resources::ref<T>`
- `resources::manager`
- `resources::cache<T>`
- `resources::manifest`
- `resources::manifest_entry`
- `resources::source`
- `resources::file_source`
- `resources::provider`
- `resources::loader<T>`
- `resources::json_loader<T>`
- `resources::saver<T>`
- `resources::json_saver<T>`

根命名空间保留常用别名：

- `caors::resource_guid`
- `caors::resource_guid_hash`
- `caors::resource`
- `caors::resource_ref<T>`
- `caors::resource_manager`
- `caors::resource_cache<T>`
- `caors::resource_manifest`
- `caors::resource_manifest_entry`
- `caors::resource_source`
- `caors::resource_file_source`
- `caors::resource_provider`
- `caors::resource_load_result<T>`
- `caors::resource_loader<T>`
- `caors::resource_json_loader<T>`
- `caors::resource_saver<T>`
- `caors::resource_json_saver<T>`
- `caors::bind_manifest()`

## resource_guid

`resource_guid` 是基于 `core::guid` 的强类型稳定资源标识：

```cpp
auto parsed = caors::resource_guid::parse("6cd2da3d-b549-4756-8b8b-0c84293af410");
if (parsed) {
  caors::resource_guid id = *parsed;
}
```

需要注意：

- nil GUID 表示资源尚未绑定持久身份。
- `resource_guid` 不由资源路径、资源名称或运行期 hash 自动派生。
- 序列化资源引用时应写出 `resource_guid`，而不是运行期指针。

## resource

`resource` 是资源基类，只保存稳定资源 GUID：

```cpp
class texture : public caors::resource {};

texture value;
value.set_guid(id);
```

需要注意：

- `resource` 不负责加载、缓存、引用计数或线程同步。
- 修改 GUID 时，调用方需要保证资源索引、缓存和外部引用保持一致。
- `resource` 和 `objects::object` 的边界不同：资源表示可持久化、可缓存、可复用资产；对象表示当前运行期实例。

## ref

`resources::ref<T>` 保存一个 `resource_guid` 和一个可选已解析资源指针：

```cpp
caors::resource_ref<texture> reference{id};

if (!reference.resolved()) {
  // 可以由 resources::manager 按 GUID 查找已存在资源。
}
```

资源加载完成后可以显式解析：

```cpp
texture value{id};

if (reference.resolve(&value)) {
  texture* current = reference.get();
}
```

需要注意：

- `ref<T>` 不拥有资源，也不延长资源生命周期。
- `resolve()` 会拒绝已保存 GUID 和资源对象 GUID 不一致的情况。
- `unresolve()` 只清除资源指针，保留 GUID，适合资源卸载或缓存失效后继续保留引用身份。
- 当前 `resources::manager` 只负责 GUID 到已存在资源对象的解析；拥有式缓存和生命周期策略后续再扩展。

## 序列化

`caors/serialization.h` 包含 `resource_guid` 和 `resource_ref<T>` 的 codec。资源引用序列化只保存稳定
GUID，不展开资源对象，也不保存运行期资源指针：

```cpp
struct material {
  caors::resource_ref<texture> albedo;
};

caors::meta::type_<caors::resource_ref<texture>>("game.resource_ref<game.texture>");
caors::serialization::register_codec<caors::resource_ref<texture>>();
```

写出规则：

- `resource_ref<T>` 为空或 GUID 为 nil 时写出 `null`。
- 非空引用写出 `resource_guid` 字符串。
- 即使引用已经解析到资源对象，也只写 GUID，不写资源对象内容。

读回规则：

- `null` 会清空引用。
- GUID 字符串会读回为未解析的 `resource_ref<T>`。
- 读回不会访问全局 manager、cache 或 locator，避免序列化层隐式决定资源生命周期。

资源加载完成后再显式解析：

```cpp
material decoded{};
caors::deserialize_json(decoded, R"({"albedo":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

caors::resource_cache<texture> cache;
// cache 创建或加载同 GUID 的资源后：
cache.resolve(decoded.albedo);
```

## manager

`resources::manager` 是非拥有资源索引。它保存 `resource_guid -> resource*` 映射，可以绑定、查找、
解绑和清空资源，也可以把 `resource_ref<T>` 解析成实际资源指针：

```cpp
caors::resource_manager manager;

texture value{id};
manager.bind(&value);

caors::resource_ref<texture> reference{id};
if (manager.resolve(reference)) {
  texture* current = reference.get();
}
```

需要注意：

- `manager` 不创建、不销毁资源，也不延长资源生命周期。
- `bind(guid, resource)` 在资源对象 GUID 为 nil 时会写入传入 GUID；如果资源对象已经有不同 GUID，则绑定失败。
- 同一对 GUID 和资源重复绑定视为成功；同一个 GUID 或同一个资源指针绑定到不同对象会失败。
- `unbind()` 和 `clear()` 只清理索引，不修改资源对象自身 GUID。
- 跨线程访问时调用方需要自行同步。
- 后续拥有式缓存、加载、卸载、失效通知和异步策略应在 manager 之上继续扩展，而不是由当前非拥有索引隐式完成。

## cache

`resources::cache<T>` 是拥有式资源缓存。它按 `resource_guid` 创建并持有 `T` 资源对象，同时维护一个
内部 `resources::manager` 作为非拥有索引：

```cpp
caors::resource_cache<texture> cache;

texture* value = cache.create(id);
caors::resource_ref<texture> reference{id};

if (cache.resolve(reference)) {
  texture* current = reference.get();
}
```

需要注意：

- `cache<T>` 拥有它创建的资源，`erase()` 和 `clear()` 会销毁资源。
- `insert()` 可以接收已经构造好的 `memory::unique_ptr<T>`，适合 loader 把资源所有权转入缓存。
- `release()` 会从缓存和索引中移除资源，并把所有权交给调用方。
- `create(guid, args...)` 会使用缓存的 `memory_resource` 构造资源；nil GUID、重复 GUID 或构造后资源
  GUID 与传入 GUID 不一致都会失败。
- `cache<T>` 当前不负责从磁盘、网络或资源包加载内容；loader 层会决定如何构造资源。
- `cache<T>` 不提供内部线程同步；跨线程访问时调用方需要自行加锁。

## manifest

`resources::manifest` 是资源 GUID 到资源位置和轻量元数据的清单。它适合承载本地资源数据库、资源包索引
或调试 fileserver 下发的 manifest：

```cpp
caors::resource_manifest manifest;

manifest.bind(texture_id,
              "/assets//textures/albedo.json",
              caors::string_name{"game.texture"},
              "content-hash",
              12);

manifest.set(texture_id,
             "assets/textures/albedo-v2.json",
             caors::string_name{"game.texture"},
             "new-content-hash",
             13);
```

清单条目包含：

- `guid`：稳定资源 GUID。
- `location`：规范化后的虚拟资源路径。
- `type`：可选资源类型名，主要用于调试、导入和上层资源数据库。
- `hash`：可选内容 hash、etag 或版本指纹。
- `version`：可选递增版本号，适合热更新和调试 fileserver 判断资源是否变化。

可以把清单批量绑定到 provider：

```cpp
caors::memory_file_system files;
caors::resource_file_source source{files};
caors::resource_provider provider;

caors::bind_manifest(provider, source, manifest);
caors::sync_manifest(provider, source, manifest);
caors::replace_manifest(provider, source, manifest);
```

也可以从 JSON 文本或 source 读取清单：

```cpp
caors::resource_manifest manifest;

caors::load_manifest_json(manifest, R"({
  "resources": [
    {
      "guid": "6cd2da3d-b549-4756-8b8b-0c84293af410",
      "location": "assets/textures/albedo.json",
      "type": "game.texture",
      "hash": "content-hash",
      "version": 12
    }
  ]
})");

caors::load_manifest(manifest, source, "manifest.json");
```

JSON 清单支持两种根格式：

```json
[
  {
    "guid": "6cd2da3d-b549-4756-8b8b-0c84293af410",
    "location": "assets/textures/albedo.json"
  }
]
```

```json
{
  "resources": [
    {
      "guid": "6cd2da3d-b549-4756-8b8b-0c84293af410",
      "location": "assets/textures/albedo.json"
    }
  ]
}
```

需要注意：

- `manifest` 只保存清单数据，不读取字节、不构造资源对象，也不拥有 source。
- `bind()` 会拒绝 nil GUID、空路径和重复 GUID，并会按虚拟路径规则规范化 location。
- `set()` 会新增或替换同 GUID 条目，适合资源数据库刷新和调试 manifest 热更新。
- JSON 导入要求每个条目至少包含字符串 `guid` 和字符串 `location`；`type`、`hash`、`version` 可选。
- `load_manifest_json()` 和 `load_manifest()` 会向现有 manifest 新增或替换条目，不会隐式清空已有内容。
- `bind_manifest()` 不回滚已经成功绑定到 provider 的条目；失败时调用方需要按自身策略处理。
- `sync_manifest()` 会更新 provider 中已有 GUID 的 source/location，也会新增缺失 GUID；它不会删除清单外的旧绑定。
- `replace_manifest()` 会先清空 provider，再绑定完整清单，适合完整清单刷新；失败时不会恢复旧 provider 状态。
- 清单的 `type`、`hash`、`version` 只是轻量元数据，不替代资源对象本身的序列化内容。
- 跨线程更新 manifest 或 provider 时，调用方需要自行同步。

## source / provider

`resources::source` 是“按 `io::path_view` 位置读取字节”的接口；`resources::provider` 是
`resource_guid -> (source, location)` 的运行期索引：

```cpp
caors::native_file_system native;
caors::resource_file_source source{native};
caors::resource_provider provider;

provider.bind(texture_id, source, "assets/textures/albedo.png");

caors::byte_buffer_result bytes = provider.read(texture_id);
caors::error written = provider.write(texture_id, std::string_view{"new-bytes"});
```

`resources::file_source` 把任意 `io::file_system` 适配成资源字节来源。因此本地目录、挂载文件系统、
后续 HTTP fileserver 或资源包都可以通过同一套 provider 暴露给资源 loader。

需要注意：

- `provider` 不拥有 source，调用方需要保证 source 生命周期。
- `source_location::location` 是非拥有 `io::path_view`，provider 修改或销毁后失效。
- `provider` 只负责定位、读取和可选写入字节，不负责解码资源对象。
- `source::write()` 默认返回 `errc::unsupported`；`file_source` 会转发到后端 `io::file_system::write_all()`。
- `bind()` 会拒绝 nil GUID、空 location 和重复 GUID。
- provider 未命中 GUID 返回 `error_domain::resources / errc::not_found`。
- 具体 source 读取失败时，错误域保持来源模块语义，例如 `file_source` 可能返回 `error_domain::io`。
- 跨线程访问 provider 或 source 时，调用方需要自行同步。

## loader / saver

`resources::loader<T>` 和 `resources::saver<T>` 是类型化资源 IO 接口。它们只描述“按 GUID 加载一个
资源”和“保存一个已有资源”，不规定路径、文件格式、资源包、缓存策略或异步调度：

```cpp
class texture_loader : public caors::resource_loader<texture> {
public:
  caors::resource_load_result<texture>
  load(caors::resource_guid id, caors::memory_resource* resource) override;
};
```

加载结果使用 `resource_load_result<T>`，内部是 `result<memory::unique_ptr<T>>`。加载器返回所有权后，
可以通过 `load_into()` 转入 `resources::cache<T>`：

```cpp
caors::resource_cache<texture> cache;
texture_loader loader;

caors::result<texture*> value = caors::load_into(cache, loader, id);
```

`load_into()` 的规则：

- cache 已经有同 GUID 资源时直接返回已有对象，不再次调用 loader。
- loader 返回的资源如果没有 GUID，会写入请求 GUID。
- loader 返回的资源如果携带不同 GUID，会返回 `errc::inconsistent_type`。
- 成功后资源所有权转入 cache，后续由 cache 销毁或 `release()` 转移。

`load_serialized<T>()` / `save_serialized<T>()` 是最小序列化读写 helper，复用现有
`serialization::reader` / `serialization::writer`：

```cpp
caors::serialization::variant_reader reader{value};
auto loaded = caors::load_serialized<texture>(id, reader);

caors::serialization::json_writer writer;
caors::save_serialized(*loaded.value, writer);
```

`resources::json_loader<T>` 是 provider 到资源对象的第一版桥接。它按 GUID 从 provider 读取 JSON
字节，再通过 `load_serialized<T>()` 构造资源，可直接配合 `load_into()` 使用：

```cpp
caors::resource_file_source source{file_system};
caors::resource_provider provider;
provider.bind(texture_id, source, "assets/texture.json");

caors::resource_json_loader<texture> loader{provider};
caors::resource_cache<texture> cache;

auto loaded = caors::load_into(cache, loader, texture_id);
```

`resources::json_saver<T>` 是资源对象到 provider 的同步 JSON 保存桥接。它会把资源对象字段序列化为
JSON 文本，再按资源 GUID 写回 provider 已绑定的位置：

```cpp
caors::resource_json_saver<texture> saver{provider};
caors::error saved = saver.save(*loaded.value);
```

需要注意：

- `load_serialized<T>()` 要求 `T` 派生自 `resources::resource` 且可默认构造。
- 读写的资源类型仍需要完成 reflection 和 serialization 注册。
- helper 只读写资源对象字段；资源 GUID 由外部资源数据库、路径或包索引维护，不写入资源对象本体。
- `json_loader<T>` 和 `json_saver<T>` 不拥有 provider；provider、source 和底层 file_system 的生命周期
  由调用方保证。
- `json_saver<T>` 要求资源携带非 nil GUID，且 provider 中已有对应 GUID 绑定；绑定到只读 source 时会返回
  `errc::unsupported`。
- 非默认构造、需要外部依赖或需要异步调度的资源应实现自定义 `loader<T>`。

相关测试位于 `tests/resources/`，测试组织规则见 [tests/README.md](../tests/README.md)。
