<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Resources

本文档说明 CAORS 资源系统的当前设计边界。资源系统仍处于基础阶段，当前提供资源身份、轻量引用模型、
非拥有资源索引和拥有式缓存；加载器、保存器和序列化 resolver 后续再补。

## 设计目标

资源系统第一阶段只解决几件事：

- 资源有跨进程、跨存档稳定的 `resource_guid`。
- 资源对象可以保存自己的稳定身份。
- 资源引用可以先保存 GUID，在资源加载后再解析成实际资源指针。
- 资源管理器可以保存非拥有索引，用 GUID 定位已经存在的资源对象。
- 资源缓存可以按 GUID 拥有资源对象，并复用 manager 做查找和引用解析。
- 底层资源模型不绑定具体引擎、文件系统、异步加载框架或编辑器管线。

当前不会实现：

- 资源加载和保存。
- 拥有式全局资源缓存。
- 自动引用计数。
- 热重载。
- 资源导入管线。
- 与对象系统的场景树生命周期绑定。

这些能力后续应建立在 `resources::manager`、`resources::loader` / `resources::saver` 和上层资源数据库之上。

## 命名空间

资源系统主定义位于 `caors::resources` 命名空间，例如：

- `resources::resource_guid`
- `resources::resource`
- `resources::ref<T>`
- `resources::manager`
- `resources::cache<T>`

根命名空间保留常用别名：

- `caors::resource_guid`
- `caors::resource_guid_hash`
- `caors::resource`
- `caors::resource_ref<T>`
- `caors::resource_manager`
- `caors::resource_cache<T>`

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
- `release()` 会从缓存和索引中移除资源，并把所有权交给调用方。
- `create(guid, args...)` 会使用缓存的 `memory_resource` 构造资源；nil GUID、重复 GUID 或构造后资源
  GUID 与传入 GUID 不一致都会失败。
- `cache<T>` 当前不负责从磁盘、网络或资源包加载内容；后续 loader 层会决定如何构造资源。
- `cache<T>` 不提供内部线程同步；跨线程访问时调用方需要自行加锁。

相关测试位于 `tests/resources/`，测试组织规则见 [tests/README.md](../tests/README.md)。
