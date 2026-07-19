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
- `io::directory_entry` / `io::directory_entry_list`：非递归目录枚举结果。
- `io::file_status`：文件或目录的基础属性查询结果。
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
- URI 解析。

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
- `io::directory_entry`
- `io::directory_entry_list`
- `io::file_status`
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

相关测试位于 `tests/io/`，测试组织规则见 [tests/README.md](../tests/README.md)。
