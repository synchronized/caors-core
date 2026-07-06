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
- `io::file_system`：同步文件系统抽象。
- `io::native_file_system`：基于宿主操作系统的本地文件系统实现。
- `io::memory_file_system`：基于内存哈希表的虚拟文件系统实现。
- `io::mount_file_system`：基于虚拟路径前缀转发到其他文件系统。

当前不会实现：

- 异步 IO。
- 文件监听。
- 目录遍历和权限模型。
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
- `io::file_system`
- `io::native_file_system`
- `io::memory_file_system`
- `io::mount_file_system`

根命名空间保留常用别名：

- `caors::byte_buffer`
- `caors::byte_buffer_result`
- `caors::size_result`
- `caors::make_byte_buffer`
- `caors::path_view`
- `caors::path`
- `caors::normalize_path`
- `caors::join_path`
- `caors::input_stream`
- `caors::output_stream`
- `caors::file_system`
- `caors::native_file_system`
- `caors::get_native_file_system`
- `caors::memory_file_system`
- `caors::mount_file_system`

## path / path_view

`io::path_view` 是非拥有字符串视图，`io::path` 是拥有式字符串。它们用于表达 IO 和资源层的路径语义，
但不等同于 `std::filesystem::path`：

```cpp
caors::path path = caors::normalize_path("\\assets//textures\\albedo.png");
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
caors::file_system& fs = caors::get_native_file_system();

if (fs.exists("config.json")) {
  caors::byte_buffer_result bytes = fs.read_all("config.json");
}
```

当前接口只覆盖最小能力：

- `exists(path)`：判断路径是否存在。
- `read_all(path, resource)`：读取完整内容，返回 `byte_buffer`。
- `write_all(path, bytes)`：覆盖写出完整字节内容。
- `write_all(path, text)`：覆盖写出字符串内容。

需要注意：

- `read_all()` 返回的 `byte_buffer` 可使用调用方传入的 `memory_resource`。
- 空路径返回 `errc::invalid_argument`。
- 不存在路径读取返回 `errc::not_found`。
- 后端打开、读取或写入失败返回 `errc::backend_failure`。
- `file_system` 不负责资源缓存、依赖解析或对象生命周期。

## native_file_system

`io::native_file_system` 是本地文件系统实现：

```cpp
caors::native_file_system fs;

caors::error status = fs.write_all("hello.txt", "hello");
caors::byte_buffer_result bytes = fs.read_all("hello.txt");
```

也可以在构造时指定本地根目录。此时相对路径会在 root 下解析，绝对路径仍按宿主平台语义直接使用：

```cpp
caors::native_file_system assets{"D:/game/assets"};

assets.write_all("config.json", R"({"debug":true})");
// 实际写入 D:/game/assets/config.json
```

也可以使用进程内默认实例：

```cpp
caors::native_file_system& fs = caors::get_native_file_system();
```

需要注意：

- 路径当前使用 `std::filesystem::path` 的宿主平台语义解释。
- root 不按虚拟路径规则规范化，避免破坏 Windows 盘符、UNC 路径或平台相关路径。
- root 只影响相对路径；传入绝对路径时不会再拼接 root。
- 写入不会自动创建父目录。
- 当前实现是同步阻塞 IO。
- 跨线程并发访问同一个文件路径时，调用方需要自行定义同步和覆盖策略。

## memory_file_system

`io::memory_file_system` 是内存中的虚拟文件系统。它按规范化虚拟路径保存字节内容：

```cpp
caors::memory_file_system fs;

fs.write_all("assets/config.json", R"({"debug":true})");
auto bytes = fs.read_all("/assets\\config.json");
```

需要注意：

- 写入会覆盖同一路径已有内容。
- 路径会通过 `normalize_path()` 规范化，因此 `/assets\\a.txt` 和 `assets/a.txt` 命中同一条记录。
- 读取返回的 `byte_buffer` 可以使用调用方传入的 `memory_resource`。
- 该实现不模拟目录、权限、时间戳或宿主平台路径语义。
- 跨线程访问时调用方需要自行同步。

## mount_file_system

`io::mount_file_system` 把虚拟路径前缀转发到其他 `file_system`。它适合把本地目录、后续 HTTP
fileserver、资源包等组合成统一入口：

```cpp
caors::native_file_system assets{"D:/game/assets"};
caors::mount_file_system fs;

fs.mount("assets", assets);

auto bytes = fs.read_all("assets/textures/albedo.png");
```

如果需要把同一个目标文件系统里的不同子目录挂到不同虚拟路径，可以使用 `target_prefix`：

```cpp
caors::native_file_system project{"D:/game_project"};
caors::mount_file_system fs;

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
- 未命中挂载返回 `errc::not_found`。
- 目标路径语义仍由被挂载的具体文件系统决定。

相关测试位于 `tests/io/`，测试组织规则见 [tests/README.md](../tests/README.md)。
