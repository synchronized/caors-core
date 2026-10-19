<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# IO

本文档说明 CAORS IO 主题的当前边界。IO 层只提供同步字节访问抽象，目标是为资源系统、配置、
资源包和后续远程来源提供统一底座。

## 当前范围

当前已实现：

- `io::byte_buffer`：使用 CAORS allocator 的字节缓冲区。
- `io::path_view`：非拥有路径视图。
- `io::path`：使用 CAORS allocator 的拥有式路径字符串。
- `io::input_stream`：同步输入字节流接口。
- `io::output_stream`：同步输出字节流接口。
- `io::input_stream_ptr` / `io::output_stream_ptr`：拥有式流指针。
- `io::file_system`：同步文件系统抽象。
- `io::file_system_capabilities`：文件系统后端能力声明。
- `io::directory_entry` / `io::directory_entry_list`：非递归目录枚举结果。
- `io::file_status`：文件或目录的基础属性查询结果。
- `io::uri_view` / `io::uri`：轻量 URI 视图和拥有式 URI。
- `io::file_system_resolver`：按 URI scheme / authority / 路径前缀路由到具体文件系统。
- `io::walk()`：基于 `file_system::list()` 的通用递归遍历 helper。
- `io::native_file_system`：基于宿主操作系统的本地文件系统实现。
- `io::memory_file_system`：基于内存哈希表的虚拟文件系统实现。
- `io::mount_file_system`：基于虚拟路径前缀转发到其他文件系统。

当前不会实现：

- 异步 IO。
- 文件监听。
- 后端原生递归枚举、完整文件属性查询和权限模型。
- 资源包格式。
- HTTP、对象存储或其他远程协议。
- 完整 RFC3986 URI 语义、百分号解码、查询参数和片段语义。

这些能力后续应建立在 `io::file_system` 和资源层 `resources::source` / `resources::provider` 之上。

## 命名空间

IO 模块主定义位于 `caors::io` 命名空间，例如：

- `io::byte_buffer`
- `io::path_view`
- `io::path`
- `io::input_stream`
- `io::output_stream`
- `io::input_stream_ptr`
- `io::output_stream_ptr`
- `io::file_system`
- `io::file_system_capabilities`
- `io::directory_entry`
- `io::directory_entry_list`
- `io::file_status`
- `io::uri_view`
- `io::uri`
- `io::file_system_resolver`
- `io::walk`
- `io::native_file_system`
- `io::memory_file_system`
- `io::mount_file_system`

IO 模块不再向根命名空间导出低层别名。路径、流和文件系统类型统一通过
`caors::io::...` 使用，避免根命名空间和资源、任务、对象等模块的低层组合件混在一起。

## path / path_view

`io::path_view` 是非拥有字符串视图，`io::path` 是拥有式字符串。它们用于表达 IO 和资源层的路径语义，
但不等同于 `std::filesystem::path`：

```cpp
caors::io::path path = caors::io::normalize_path("\\assets//textures\\albedo.png");
// path.string_view() == "assets/textures/albedo.png"
```

当前规则：

- `path_view` 不拥有内容，调用方需要保证底层字符串生命周期。
- `path` 使用 CAORS allocator 保存内容。
- `path` 普通构造不自动规范化，避免破坏本地绝对路径。
- `normalize_path()` 和 `join_path()` 面向虚拟路径：统一为 `/`，去掉开头分隔符，折叠连续分隔符。
- `native_file_system` 仍由后端把 `path_view` 转为宿主平台路径。

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

`status()` 返回 `file_status`，第一版只定义通用基础字段：

- `kind`：文件或目录。
- `size` / `has_size`：文件字节大小。目录、虚拟目录或无法查询大小的后端可以不提供。
- `modified_time` / `has_modified_time`：最后修改时间。内存后端和部分虚拟后端可以不提供。
- `read_only` / `has_read_only`：是否只读。只有能查询权限语义的后端才需要提供。

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

`io::uri_view` 和 `io::uri` 用于解析逻辑资源位置和后端路由边界。它们不是 MIME 或 content-type：
URI 的 scheme 决定“去哪一类后端取”，MIME / content-type 只描述“取到的内容是什么格式”。
`io::location_view` 和 `io::location` 则是资源层对“普通虚拟路径或 URI”的拥有/非拥有表示。

```cpp
caors::io::uri_view location{"caors://assets/textures/albedo.png"};

// scheme == "caors"
// authority == "assets"
// path == "textures/albedo.png"
```

第一版 URI 解析规则：

- 支持 `scheme://authority/path`、`scheme:path` 和普通相对路径。
- `path()` 会去掉查询参数和片段文本，但不会做百分号解码。
- `caors://assets/a.png` 的 scheme 是 `caors`，authority 是 `assets`，path 是 `a.png`。
- 普通路径 `assets/a.png` 没有 scheme 和 authority，path 是完整输入路径。
- `io::normalize_location()` 返回 `io::location`，用于资源位置规范化：普通路径按
  `normalize_path()` 处理；URI 会保留 scheme / authority，只规范化 path 部分。
- `io::join_location()` 返回 `io::location`，用于拼接资源位置：普通路径等价于
  `join_path()`；URI 会在 path 部分拼接，并保留原来的 scheme / authority。
- 底层 `io::file_system` 仍接收 `io::path_view`；资源 manifest/provider/source 等面向资源位置的
  API 使用 `io::location_view`。

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

- resolver 不拥有被挂载的文件系统。
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

- 挂载表不拥有目标 `file_system`，调用方需要保证目标生命周期。
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
