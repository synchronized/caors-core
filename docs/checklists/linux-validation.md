<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Linux 验证执行清单

本清单用于在新的 Linux 环境中复验 CAORS 的静态库、共享库、插件和 AddressSanitizer 行为。
执行前应记录当前提交，执行后应保留失败日志；不要通过排除失败测试使矩阵通过。

## 环境记录

```sh
git rev-parse HEAD
uname -a
cat /etc/os-release
cmake --version
ninja --version
clang --version
gcc --version
ld --version
ldd --version
```

同时记录是否运行在容器或虚拟机中，以及可用内存和逻辑 CPU 数量。若构建发生 OOM，使用
`cmake --build --preset <preset> -j 1` 重新验证，或从逻辑 CPU 数量的一半以下开始调整。

## 基础矩阵

逐项执行配置、构建和测试：

```sh
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug

cmake --preset linux-clang-release
cmake --build --preset linux-clang-release
ctest --preset linux-clang-release

cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug

cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release
```

## Shared 与插件矩阵

```sh
cmake --preset linux-clang-shared-debug
cmake --build --preset linux-clang-shared-debug
ctest --preset linux-clang-shared-debug

cmake --preset linux-clang-shared-release
cmake --build --preset linux-clang-shared-release
ctest --preset linux-clang-shared-release
```

检查：

- plugin protocol 与 meta plugin identity 测试已被发现并执行；
- 插件完成 attach、detach 和 unload；
- 存在 lease 时拒绝卸载，释放 lease 后可以卸载；
- 插件卸载后不再调用来自该 `.so` 的函数或销毁函数。

可使用以下命令检查测试发现结果：

```sh
ctest --test-dir build/linux-clang-shared-debug --show-only
```

## AddressSanitizer

```sh
cmake --preset linux-clang-shared-asan
cmake --build --preset linux-clang-shared-asan
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ctest --preset linux-clang-shared-asan
```

检查：

- 主库、测试 executable 和测试 plugin 均链接 ASan；
- 没有 use-after-free、use-after-unload、越界访问或泄漏报告；
- 临时内存成功路径和失败回滚路径均恢复到调用前状态；
- 如果当前环境不支持 LeakSanitizer，记录原因、影响范围和替代验证方式。

Linux 下可抽查动态依赖：

```sh
ldd build/linux-clang-shared-asan/tests/caors_tests
```

实际测试可执行文件路径以当前构建目录为准。

## MessagePack 固定字节专项

确认现有 MessagePack 测试覆盖并通过：

- 16、32、64 位有符号与无符号整数；
- `float64`；
- 字符串、binary、array 和 map 的多字节长度；
- 正负边界值；
- nested indirect serialization；
- unknown-field path 保存；
- 编码结果使用协议规定的大端字节序，与主机字节序无关。

若修改协议编码器，应在 Clang、GCC 和 ASan 三类矩阵中重新执行相关测试。

## 结果记录模板

```text
提交：
发行版/内核/架构：
容器或虚拟机：
CMake/Ninja：
Clang/GCC：
glibc/libstdc++/linker：

Static Clang Debug：
Static Clang Release：
Static GCC Debug：
Static GCC Release：
Shared Clang Debug：
Shared Clang Release：
Shared Clang ASan：

测试总数：
失败项：
ASan/LeakSanitizer 结果：
OOM 与实际并行度：
已知限制：
日志位置：
```
