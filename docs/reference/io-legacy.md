<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# IO 兼容接口

本页维护旧文件系统、同步工具和显式组合函数。使用时包含 `caors/io/legacy.h` 或对应单头，
普通调用者从 [io_context/file](io.md) 开始；请求状态与错误约束也以主参考为准。
同步文件能力仍用于配置、保存、工具与内置异步后端；不意味着这些实现都将删除。
分层原则见[职责说明](../concepts/io-boundaries.md)，剩余迁移范围由 [VFS 计划](../plans/vfs-api-consolidation.md#兼容退出的剩余范围)维护。

## 当前范围

以下兼容接口仍可使用；新文件访问使用 [IO 主参考](io.md)。

- `io::input_stream`：同步输入字节流接口。
- `io::output_stream`：同步输出字节流接口。
- `io::input_stream_ptr` / `io::output_stream_ptr`：拥有式流指针。
- `io::file_system`：同步文件系统访问与后端异步打开接入。
- `io::file_system_capabilities`：文件系统后端能力声明。
- `io::file_system_resolver`：按 URI scheme / authority / 路径前缀路由到具体文件系统。
- `io::walk()`：基于 `file_system::list()` 的通用递归遍历 helper。
- `io::native_file_system`：基于宿主操作系统的本地文件系统实现。
- `io::memory_file_system`：基于内存哈希表的虚拟文件系统实现。
- `io::mount_file_system`：基于虚拟路径前缀转发到其他文件系统。

当前未提供：

- 内置 HTTP/libuv 等具体宿主异步后端。
- 文件监听。
- 后端原生递归枚举、完整文件属性查询和权限模型。
- 资源包格式。
- HTTP、对象存储或其他远程协议。
- 完整 RFC3986 URI 语义、百分号解码、查询参数和片段语义。

宿主可实现异步读取后端；现有协议不等于已内置网络或文件监听实现。资源 loader 也可以直接使用宿主服务。

## file_system

`io::file_system` 表示一类可通过 `path_view` 访问的字节来源。路径语义由具体实现决定：

```cpp
caors::io::file_system& fs = caors::io::get_native_file_system();

if (fs.exists("config.json")) {
  caors::io::byte_buffer_result bytes = fs.read_all("config.json");
}
```

当前接口覆盖最小文件系统能力：

- `exists(path)`：判断路径是否存在。
- `status(path)`：查询文件或目录的基础属性，空路径表示文件系统根目录。
- `list(directory, resource)`：列出目录下的直接子项，空路径表示文件系统根目录。
- `read_all(path, resource)`：读取完整内容，返回 `byte_buffer`。
- `open_read(path, resource)`：打开同步输入流。
- `open_write(path, resource)`：打开同步输出流。
- `write_all(path, bytes)`：覆盖写出完整字节内容。
- `write_all(path, text)`：覆盖写出字符串内容。

`open_read()` 的默认实现会通过 `read_all()` 建立内存输入流，因此所有已有后端都能被需要
`input_stream` 的上层接口直接使用。需要真正分块读取的后端可以覆盖该函数。

`open_write()` 的默认实现会建立内存缓冲输出流。`write()` / `write_all()` 只追加到缓冲区，只有显式调用
`close()` 才会通过 `write_all()` 提交到底层后端；析构函数不会隐式提交，避免把写入失败隐藏到析构阶段。
`flush()` 默认是 no-op。需要真正分块写入、追加写入或事务提交的后端可以覆盖该函数。

需要注意：

- `read_all()` 返回的 `byte_buffer` 可使用调用方传入的 `memory_resource`。
- 空路径返回 `errc::invalid_argument`。
- 不存在路径读取返回 `errc::not_found`。
- 后端打开、读取或写入失败返回 `errc::backend_failure`。
- 输出流写完后应显式检查 `close()` 返回值；没有 `close()` 的缓冲输出流不会提交内容。
- `file_system` 不负责资源缓存、依赖解析或对象生命周期。

`capabilities()` 返回后端整体能力声明。它用于资源系统、编辑器和工具层做路由与 UI 决策：

- `read`：支持读取字节或打开输入流。
- `write`：支持写入字节或打开输出流。
- `status`：支持基础属性查询。
- `list`：支持目录枚举。
- `watch`：支持变化监听；当前内置后端尚未实现。
- `seek`：支持随机访问或 seek 语义；当前通用流接口尚未暴露完整 seek API。

能力声明不是权限检查。具体路径仍可能因为不存在、权限、后端状态或格式限制返回错误。上层不应根据
scheme 猜测能力，应优先读取目标 `file_system` 的 `capabilities()`。

字段语义见[主参考的元数据约定](io.md#路径与元数据)。

`list()` 是非递归目录枚举接口，返回的 `directory_entry::name` 只包含直接子项名称，
`directory_entry::path` 是可再次传给同一个 `file_system` 的路径。只支持读写、不支持枚举的后端可以沿用
默认实现并返回 `errc::unsupported`。

需要递归扫描时优先使用 `io::walk()`：

```cpp
caors::io::walk(fs, "assets", [](const caors::io::directory_entry& entry, std::size_t depth) {
  // entry.path 可继续传给同一个 file_system。
  // depth 从 root 的直接子项开始计为 0。
});
```

`walk()` 基于 `list()` 实现，因此它能自动适配 native、memory、mount 和后续资源包 / 远程后端。
visitor 返回 `io::walk_action::skip_children` 可跳过当前目录的子项，返回 `io::walk_action::stop` 或
`false` 可正常停止遍历；返回 `caors::error` 可把错误传出。`walk_options::max_depth` 可限制遍历深度，
`include_root` 可让 root 自身作为第一条记录进入 visitor。循环挂载或会反复产生同一路径的后端需要由调用方
通过最大深度或上层访问记录避免无限递归。

## uri / location / file_system_resolver

通用 URI/location 规则见[主参考](io.md#uri--location)。

`io::file_system_resolver` 本身实现 `io::file_system`，可以把 URI 路由到具体后端：

```cpp
caors::io::memory_file_system assets;
caors::io::file_system_resolver fs;

fs.mount("caors://assets", assets);
fs.write_all("caors://assets/config.json", R"({"debug":true})");

auto bytes = fs.read_all("caors://assets/config.json");
```

也可以给目标文件系统设置内部路径前缀：

```cpp
caors::io::native_file_system project{"D:/game_project"};
caors::io::file_system_resolver fs;

fs.mount("caors://assets", project, "assets");
```

规则：

- `mount(prefix, file_system&, target_prefix)` 借用目标；拥有式重载见下文[拥有式挂载](#拥有式挂载)。
- 路由按 scheme、authority 和路径前缀匹配，使用最长匹配。
- 未命中路由返回 `errc::not_found`。
- `list()` 返回的 `directory_entry::path` 会回到调用方使用的 URI 空间。
- `capabilities()` 聚合已挂载目标能力；上层仍应把它当作整体能力提示，不作为单个路径的最终保证。
- `file_system_resolver` 是 URI 层 VFS；它可以把 `caors://`、`file://`、`pack://`、后续 `http://`
  等位置转发到具体后端。
- `mount_file_system` 是低层路径前缀 VFS；它只处理虚拟路径到目标文件系统和可选子路径的映射，不解析
  URI scheme。
- resolver 的目标可以是 native、memory、mount、manifest-backed overlay，或后续资源包、远程后端。
  调用方不应根据 scheme 推断能力，应查询实际入口的 `capabilities()`。

## native_file_system

`io::native_file_system` 是本地文件系统实现：

```cpp
caors::io::native_file_system fs;

caors::error status = fs.write_all("hello.txt", "hello");
caors::io::byte_buffer_result bytes = fs.read_all("hello.txt");
```

也可以在构造时指定本地根目录。此时相对路径会在 root 下解析，绝对路径仍按宿主平台语义直接使用：

```cpp
caors::io::native_file_system assets{"D:/game/assets"};

assets.write_all("config.json", R"({"debug":true})");
// 实际写入 D:/game/assets/config.json
```

也可以使用进程内默认实例：

```cpp
caors::io::native_file_system& fs = caors::io::get_native_file_system();
```

需要注意：

- 路径当前使用 `std::filesystem::path` 的宿主平台语义解释。
- root 不按虚拟路径规则规范化，避免破坏 Windows 盘符、UNC 路径或平台相关路径。
- root 只影响相对路径；传入绝对路径时不会再拼接 root。
- 写入不会自动创建父目录。
- `open_read()` / `open_write()` 覆盖为宿主文件流，不再先把完整文件读入内存或写到临时内存缓冲。
- `open_write()` 直接打开目标文件并覆盖写入；如果写入中途失败，调用方应按自身策略处理半写文件。
- `status()` 使用宿主文件系统属性，文件大小、最后修改时间和只读状态在可查询时返回。
- `list()` 使用宿主文件系统的目录语义，只列出直接子项，不递归。
- 当前实现是同步阻塞 IO。
- 跨线程并发访问同一个文件路径时，调用方需要自行定义同步和覆盖策略。

## memory_file_system

`io::memory_file_system` 是内存中的虚拟文件系统。它按规范化虚拟路径保存字节内容：

```cpp
caors::io::memory_file_system fs;

fs.write_all("assets/config.json", R"({"debug":true})");
auto bytes = fs.read_all("/assets\\config.json");
```

需要注意：

- 写入会覆盖同一路径已有内容。
- 路径会通过 `normalize_path()` 规范化，因此 `/assets\\a.txt` 和 `assets/a.txt` 命中同一条记录。
- `status()` 对真实内存文件返回大小，对从路径推导出的虚拟目录只返回目录类型。
- `list()` 会从已写入文件路径推导虚拟目录；它不会保存独立目录对象。
- 读取返回的 `byte_buffer` 可以使用调用方传入的 `memory_resource`。
- 该实现不模拟目录、权限、时间戳或宿主平台路径语义。
- 跨线程访问时调用方需要自行同步。

## mount_file_system

`io::mount_file_system` 把虚拟路径前缀转发到其他 `file_system`。它适合把本地目录、后续 HTTP
fileserver、资源包等组合成统一入口：

```cpp
caors::io::native_file_system assets{"D:/game/assets"};
caors::io::mount_file_system fs;

fs.mount("assets", assets);

auto bytes = fs.read_all("assets/textures/albedo.png");
```

如果需要把同一个目标文件系统里的不同子目录挂到不同虚拟路径，可以使用 `target_prefix`：

```cpp
caors::io::native_file_system project{"D:/game_project"};
caors::io::mount_file_system fs;

fs.mount("textures", project, "assets/textures");
fs.mount("materials", project, "assets/materials");
```

`mount_file_system` 不关心 URI scheme，也不区分 `caors://`、`file://` 或 `pack://`。如果需要按
scheme、authority 或 URI 前缀分发，应在更高层使用 `file_system_resolver`，再把某个 resolver
目标指向一个 `mount_file_system`。

规则：

- 借用重载不拥有目标 `file_system`，调用方需要保证目标生命周期；另提供[拥有式挂载](#拥有式挂载)。
- 虚拟路径使用 `/` 作为分隔符，开头的 `/` 会被忽略。
- 常规物理根目录应放在目标 `file_system` 内，例如 `native_file_system{"D:/game/assets"}`。
- `target_prefix` 只用于目标文件系统内部的子路径重映射，不应承担后端物理根目录职责。
- `mount()` 会拒绝重复前缀，返回 `errc::already_exists`。
- 读写时使用最长前缀匹配，允许更具体的挂载覆盖通用挂载。
- `status()` 会把路径转发到最长匹配的目标文件系统；挂载点和子挂载点会按虚拟目录返回。
- `list()` 会合并目标文件系统的直接子项和挂载表中的子挂载点；子挂载点按目录返回。
- 未命中挂载返回 `errc::not_found`。
- 目标路径语义仍由被挂载的具体文件系统决定。

相关测试位于 `tests/io/`，测试组织规则见 [tests/README.md](../../tests/README.md)。

## 拥有式挂载

resolver 和 mount 均增加 `mount(prefix, shared_ptr<file_system>, target_prefix, lifetime)` 重载。
挂载持有后端；`lifetime` 是可选的拥有式宿主凭据，例如模块 pin，其释放晚于本挂载持有的后端所有权。
路由表扩容、删除、清空和销毁均遵循该顺序；空后端返回 invalid_argument，重复前缀返回 already_exists，
不替换已有路由，也不继续持有被拒绝的候选。分配失败可抛出 bad_alloc，并保留已有挂载。

调用方若在挂载之外独立持有后端，必须自行保证相应实现代码和 allocator 的寿命；
路由器自身的 memory_resource 必须覆盖路由器、已捕获快照及所有相关控制块；并发使用要求 allocator 支持并发调用。
不要用拥有式嵌套挂载形成 shared_ptr 强引用环；快照捕获会诊断循环，但不会替宿主拆除已有所有权环。
配置创建的挂载图在发布拥有式边之前拒绝循环，手工挂载仍须遵守无强引用环的前置条件。
原 `mount(prefix, file_system&, target_prefix)` 的借用行为保持不变。

## 不可变路由快照

`router.snapshot()` 捕获 resolver/mount 及嵌套路由的稳定副本；`route_snapshot::capture(shared_ptr<file_system>, lifetime)`
也可捕获拥有式叶根。调用 `snapshot.resolve(location)` 得到 `resolved_file`，其中 `backend` 持有后端与宿主凭据，
`path` 拥有字符串，`source_id` 是进程内挂载实例标记；无挂载的叶根使用后端实例标记。
解析本身不证明文件存在，也不冻结外部文件的内容或修订。

- 叶后端的配置（例如 native root）应在操作期间保持稳定；切换来源应新建后端并 remount。
- 同步调用和快照读取来自同一权威挂载表。更新以不可变表发布；读操作持有当时的表，在内部同步之外调用后端和析构旧所有者。
- mount/unmount/clear、数量查询、快照捕获及同步路由查询可并发；后端数据、allocator 和路由器析构的并发约束独立成立。
- unmount/remount 只影响后续路由。旧快照和解析结果可在路由器销毁后使用；不会自动清资源缓存或取消旧读取。
- 捕获时遍历全部可达路由，任一借用边返回 unsupported；不能只验证当前命中的路径而忽略后续依赖。
- 同一次捕获中共用嵌套节点，不在后续解析时查询实时嵌套路由。各节点捕获是短时观察，不提供多路由器更新的全局事务。
- 循环返回 cycle_detected，超过 256 层返回 recursion_limit。快照只读解析不修改挂载表。
- 自定义转发后端必须声明 `routes_paths()` 并覆盖 `freeze_routes()`，通过上下文递归；默认路由扩展返回 unsupported。
  普通叶后端可使用默认捕获实现，但必须有完整拥有者，不能把内部可变路由伪装成叶后端。

宿主 loop 适配步骤见[接入指南](../guides/async-vfs-integration.md)。

## 串行分块读取

`chunked_file.h` 仍为底层适配器提供 read_all_chunked 与 consume_chunks，复用内部精确范围读取，
支持后端单次读取上限、进度通知及不会递归增长的逐块消费；这些能力尚未完整迁入普通 file 接口，暂保留。
普通调用者按 file.read 逐块处理，见 [io_chunked_read.cpp](../../examples/beta/io_chunked_read.cpp)。
`chunk_read_options` 显式提供 max_bytes（总处理预算）与非零 chunk_bytes（单块上限），
实际块还受后端 max_read_size、剩余长度及本机可表示范围限制。

- read_all_chunked 返回完整 byte_buffer；结果需 size_t/PTRDIFF_MAX 可表示，驻留内存为完整结果加常数个临时块。
- consume_chunks 返回 chunk_summary.bytes；消费者在指定 completion executor 上同步执行，
  仅在调用期间借用 offset/const span，返回 io_error 表示停止；不能保存该视图。
- 读完并消费/合并当前块后才启动下一块，最多一个未完成范围；慢消费者不会导致无界预读。
  当前通知退出前可能仍持有上一块，因此辅助结果缓冲为常数个块，不宣称严格只有一份分配。
- 总处理预算不等于峰值内存预算，allocator、请求/通知控制块及后端内部缓冲需另计；不将其宣称为进程总内存上限。
- chunk_progress 报告已消费/合并字节与固定总量，不统计排队字节；空文件可直接成功，不调用消费者。
- 取消阻止新块启动，并协作停止在途读取；不能抢占正在执行的同步消费者，消费副作用不会回滚。
- 消费者或进度回调抛出 bad_alloc 归类 out_of_memory，其他异常归类 backend_failure；
  消费者返回的后端错误码原样保留。读取、取消和通知拒绝/丢弃都有唯一可查询终态。
- helper 持有文件以覆盖实际清理，但不隐式关闭共享文件；底层适配器仍负责通过 async_file::start_close 关闭及维持 loop 寿命；外部关闭可使尚未启动的后续块返回 closed。
  allocator 可能在发起、后端或完成线程使用，宿主须满足对应线程安全要求。
- inline executor 与即时后端使用迭代推进，不随块数递归增长；原生后端及 executor 的线程约束仍成立。
