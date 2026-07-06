<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# IO

本文档说明 CAORS IO 主题的当前边界。IO 层只提供同步字节访问抽象，目标是为资源系统、配置、
资源包和后续远程来源提供统一底座。

## 当前范围

当前已实现：

- `io::byte_buffer`：使用 CAORS allocator 的字节缓冲区。
- `io::input_stream`：同步输入字节流接口。
- `io::output_stream`：同步输出字节流接口。
- `io::file_system`：同步文件系统抽象。
- `io::native_file_system`：基于宿主操作系统的本地文件系统实现。
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
- `io::input_stream`
- `io::output_stream`
- `io::file_system`
- `io::native_file_system`
- `io::mount_file_system`

根命名空间保留常用别名：

- `caors::byte_buffer`
- `caors::byte_buffer_result`
- `caors::size_result`
- `caors::make_byte_buffer`
- `caors::input_stream`
- `caors::output_stream`
- `caors::file_system`
- `caors::native_file_system`
- `caors::get_native_file_system`
- `caors::mount_file_system`

## file_system

`io::file_system` 表示一类可通过字符串路径访问的字节来源。路径语义由具体实现决定：

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

也可以使用进程内默认实例：

```cpp
caors::native_file_system& fs = caors::get_native_file_system();
```

需要注意：

- 路径当前使用 `std::filesystem::path` 的宿主平台语义解释。
- 写入不会自动创建父目录。
- 当前实现是同步阻塞 IO。
- 跨线程并发访问同一个文件路径时，调用方需要自行定义同步和覆盖策略。

## mount_file_system

`io::mount_file_system` 把虚拟路径前缀转发到其他 `file_system`。它适合把本地目录、后续 HTTP
fileserver、资源包等组合成统一入口：

```cpp
caors::native_file_system native;
caors::mount_file_system fs;

fs.mount("assets", native, "D:/game/assets");

auto bytes = fs.read_all("assets/textures/albedo.png");
```

规则：

- 挂载表不拥有目标 `file_system`，调用方需要保证目标生命周期。
- 虚拟路径使用 `/` 作为分隔符，开头的 `/` 会被忽略。
- `mount()` 会拒绝重复前缀，返回 `errc::already_exists`。
- 读写时使用最长前缀匹配，允许更具体的挂载覆盖通用挂载。
- 未命中挂载返回 `errc::not_found`。
- 目标路径语义仍由被挂载的具体文件系统决定。

相关测试位于 `tests/io/`，测试组织规则见 [tests/README.md](../tests/README.md)。
