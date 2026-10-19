<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 构建与安装

本文档说明 CAORS 的环境要求、常用构建命令、测试入口、CMake 选项和安装方式。

## 环境要求

- CMake 3.23 或更新版本。
- Ninja（使用 Ninja preset 时需要）。
- 支持 C++20 的编译器，任选其一：
  - MSVC `cl`
  - `clang-cl`
  - Clang
  - GCC

Windows 下推荐直接使用 Visual Studio 2022 generator preset，它会通过 Visual Studio 安装信息定位 MSVC，不要求当前终端的 `PATH` 中已有 `cl`：

```powershell
cmake --preset windows-vs2022-debug
cmake --build --preset windows-vs2022-debug
ctest --preset windows-vs2022-debug
```

如果需要 Ninja 构建速度或与 CI 保持一致，可通过包装脚本自动定位 Visual Studio 并初始化 MSVC 环境：

```powershell
.\scripts\msvc.ps1
```

`scripts/msvc.ps1` 是 Ninja + MSVC 的标准入口。只有当前终端已经是 Visual Studio Developer
PowerShell/Command Prompt 时，才直接调用 `windows-msvc-*` preset。

## 快速开始

查看可用 preset：

```sh
cmake --list-presets
```

配置、构建并测试：

```sh
cmake --preset windows-clang-static-debug
cmake --build --preset windows-clang-static-debug
ctest --preset windows-clang-static-debug
```

使用 GCC preset：

```sh
cmake --preset linux-gcc-static-debug
cmake --build --preset linux-gcc-static-debug
ctest --preset linux-gcc-static-debug
```

preset 名称表示预期工具链组合，实际是否可用取决于当前系统和 PATH 中的编译器。

## 常用 Preset

默认使用动态库：`<平台>-<工具链>-debug/release`；静态库明确使用
`<平台>-<工具链>-static-debug/release`。preset 通过 `CAORS_LIBRARY_TYPE` 固定 CAORS 的类型，
不修改宿主的 `BUILD_SHARED_LIBS`。不使用 preset 的父工程仍可选择 `AUTO/STATIC/SHARED`。

| Preset | 用途 |
| --- | --- |
| `windows-msvc-debug/release` | Ninja + MSVC 动态库，要求开发环境 |
| `windows-clang-debug/release` | Clang 动态库 |
| `windows-clang-cl-debug/release` | clang-cl 动态库，要求开发环境 |
| `windows-vs2022-debug/release` | Visual Studio generator 动态库，普通终端可用 |
| `linux-clang-debug/release` | Linux Clang 动态库 |
| `linux-gcc-debug/release` | Linux GCC 动态库 |
| `*-static-debug/release` | 上述平台/工具链对应的静态库配置 |
| `linux-clang-asan` | 动态 Debug + AddressSanitizer |
| `linux-clang-ubsan` | 动态 Debug + UndefinedBehaviorSanitizer |

旧 `*-shared-*` 名称已移除；原来无 `static` 的名称现在表示动态库。
需要保留旧静态行为时改用 `*-static-*`。旧构建目录建议重新配置到全新目录，避免混用旧缓存和产物。
Debug、clang-cl 与 Visual Studio presets 继续保留，CI 常规组合由[CI 策略](ci.md)统一维护。

每个 configure preset 使用独立构建目录 `build/<preset-name>/`，不同编译器和配置不会复用同一份
CMake 缓存。

包装脚本默认依次配置、构建和测试动态库 `windows-msvc-debug`。也可以只执行一个阶段或指定其他 Ninja + MSVC preset：

```powershell
.\scripts\msvc.ps1 -Action configure
.\scripts\msvc.ps1 -Preset windows-msvc-static-release
.\scripts\msvc.ps1 -Preset windows-msvc-debug
.\scripts\msvc.ps1 -Action build -Preset windows-msvc-debug -Jobs 1
```

若 MSVC/Ninja 输出包含大量未被过滤的 showIncludes 行，可用
`ninja -C <build> -t deps <object>` 检查头依赖。对象依赖为 0 时，不能相信修改头文件后的增量结果；
应改用 Visual Studio generator，或完整清理后按当前源码构建。语言资源缺失时仅设置 VSLANG 不保证改变诊断语言。

`-Fresh` 只重建 CMake 缓存，不等于清理全部对象文件。恢复旧构建目录、变更编译器或发现头文件依赖记录不完整时，
应在初始化工具链后用 `cmake --build --preset <preset> --clean-first` 执行干净重建，不能把旧对象当作本轮验收。
普通 PowerShell 先执行 MSVC 包装脚本的 configure 动作，再在同一进程内调用 clean-first 构建。

切换 worktree、移动源码目录或需要丢弃旧 CMake cache 时，使用 `-Fresh` 让 CMake 重新生成缓存：

```powershell
.\scripts\msvc.ps1 -Preset windows-msvc-static-debug -Fresh
```

## 构建并行度与内存

默认建议不指定 `-j` 或 `--parallel`，让 Ninja、Visual Studio 等底层构建工具采用自身默认并行度：

```powershell
.\scripts\msvc.ps1 -Action build -Preset windows-msvc-static-debug
```

如果模板密集目标并行编译时发生内存不足，应根据可用内存主动降低并行度。最稳妥的诊断和低内存
构建方式是单任务：

```powershell
.\scripts\msvc.ps1 -Action build -Preset windows-msvc-static-debug -Jobs 1
```

需要兼顾速度时，可从逻辑 CPU 数量的一半或更低值开始尝试，例如 `-j 4`，再根据峰值内存调整；
CPU 数量不是硬性依据，内存容量和单个编译任务的占用更重要。也可以通过
`CMAKE_BUILD_PARALLEL_LEVEL` 为当前开发环境设置默认并行度，而不把机器相关数值写入项目 preset。

## 测试

测试说明见 [tests/README.md](../../tests/README.md)。这里仅保留常用入口：

```sh
ctest --preset windows-clang-static-debug
ctest --preset linux-gcc-static-debug
```

## CMake 选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `CAORS_LIBRARY_TYPE` | `AUTO` | CAORS 独立链接类型：AUTO、STATIC、SHARED |
| `BUILD_SHARED_LIBS` | 未设置时等效 `OFF` | 宿主全局选项，仅在 CAORS 链接类型为 AUTO 时跟随 |
| `CAORS_BUILD_TESTING` | 顶层 `ON`，子项目 `OFF` | 是否构建测试目标；还需 BUILD_TESTING 启用 |
| `CAORS_BUILD_EXAMPLES` | 顶层项目为 `ON` | 是否构建示例目标 |
| `CAORS_BUILD_EXAMPLE_TESTING` | `ON` | 测试启用时是否同时构建示例 |
| `CAORS_BUILD_BENCHMARKS` | `OFF` | 是否构建 benchmark 目标 |
| `CAORS_ENABLE_ASAN` | `OFF` | 是否为 CAORS 及其链接目标启用 AddressSanitizer；当前支持 Clang/GCC |
| `CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE` | `OFF` | 是否让库内部分配跟随默认资源；仅用于诊断和分配 benchmark |
| `CAORS_ENABLE_WARNINGS` | `ON` | 是否为 CAORS 自有目标开启编译警告 |
| `CAORS_ENABLE_PEDANTIC_WARNINGS` | `OFF` | 是否启用严格标准扩展警告，例如 `-Wpedantic` |
| `CAORS_WARNINGS_AS_ERRORS` | `OFF` | 是否将 CAORS 自有目标的编译警告视为错误 |
| `CAORS_USE_DEBUG_OUTPUT` | `OFF`，Debug preset 为 `ON` | 是否启用调试输出宏 |
| `CAORS_USE_LOG` | `OFF`，Debug preset 为 `ON` | 是否启用日志宏 |
| `CAORS_USE_LOG_SEVERITY` | `0` | 日志最低级别宏 |

开启警告即错误：

```sh
cmake --preset windows-clang-static-debug -DCAORS_WARNINGS_AS_ERRORS=ON
```

开启严格标准扩展警告：

```sh
cmake --preset windows-clang-static-debug -DCAORS_ENABLE_PEDANTIC_WARNINGS=ON
```

构建 benchmark：

```sh
cmake --preset windows-clang-static-debug -DCAORS_BUILD_BENCHMARKS=ON
cmake --build --preset windows-clang-static-debug --target caors_benchmarks
```

Linux 下验证共享库与插件：

```sh
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug
```

Linux 下启用 AddressSanitizer：

```sh
cmake --preset linux-clang-asan
cmake --build --preset linux-clang-asan
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ctest --preset linux-clang-asan
```

`CAORS_ENABLE_ASAN` 使用目标级编译和链接选项，并通过 `caors::caors` 传播到测试及插件目标，避免
共享边界两侧混用 sanitizer ABI。当前只接受 Clang 或 GCC；其他编译器会在配置阶段报错。完整的
人工验证步骤见 [Linux 验证执行清单](../checklists/linux-validation.md)。

## 示例

示例说明见 [examples/README.md](../../examples/README.md)。

在本项目 CMake 内部链接库目标：

```cmake
target_link_libraries(your_target PRIVATE caors::caors)
```

## 安装

构建后可安装到指定目录：

```sh
cmake --install build/<preset-name> --prefix <install-prefix>
```

当前安装内容包括：

- Static 或 Shared 库文件；Windows Shared 安装同时包含 import library 和 DLL。
- `include/caors/` 公共头文件，其中 `version.h` 由 `version.h.in` 生成。
- `lib/cmake/caors/caorsConfig.cmake` 包配置文件。
- `lib/cmake/caors/caorsConfigVersion.cmake` 版本匹配文件。
- `lib/cmake/caors/caorsTargets.cmake` 导出目标文件。
- `share/doc/caors/LICENSE` 许可证文件。

安装后，外部项目可通过 Config 模式查找并链接：

```cmake
find_package(caors CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE caors::caors)
```

外部工程应将安装前缀加入 `CMAKE_PREFIX_PATH`，不应直接引用 CAORS 源码目录或 producer 构建目录：

```sh
cmake -S <consumer-source> -B <consumer-build> \
  -DCMAKE_PREFIX_PATH=<install-prefix>
cmake --build <consumer-build>
```

`caors::caors` 会传递 C++20、头文件目录和 Static/Shared 所需编译定义。当前安装契约通过六类独立
consumer 覆盖基础 API 与序列化、核心 tasks、tasks coroutine、resources coroutine、共享宿主及静态宿主，并验证安装前缀移动后的
重新配置。普通 consumer 不需要包含 `caors/tasks/coroutine.h`，内置调度后端仅依赖 C++ 标准库和平台线程支持，不再引入 Asio。
具体已执行的配置与结果见[验收记录索引](../records/README.md)。

### Shared 运行时库

链接 Shared 安装包后，运行进程还必须能找到动态库：

- Windows：将 `<install-prefix>/bin` 加入该进程的 `PATH`；
- Linux：使用部署 RPATH，或只为目标进程设置指向 `<install-prefix>/lib` 的
  `LD_LIBRARY_PATH`；
- macOS：使用应用的部署规则或目标进程的动态库搜索路径。

不要把 CAORS DLL/SO 随意复制回源码树，也不要永久修改开发机器的全局搜索路径。host、plugin 和
其他跨边界模块仍需遵守相同编译器、标准库、架构和运行时兼容性要求。

### 安装验证脚本

维护者可以在 producer 已配置并构建后运行完整验证：

```sh
cmake \
  -DCAORS_PRODUCER_BUILD_DIR=build/<producer> \
  -DCAORS_INSTALL_PREFIX=build/installed-package/<mode>-prefix \
  -DCAORS_CONSUMER_BUILD_DIR=build/installed-package/<mode>-consumer \
  -DCAORS_RELOCATION_PREFIX=build/installed-package/<mode>-relocated-prefix \
  -DCAORS_RELOCATED_CONSUMER_BUILD_DIR=build/installed-package/<mode>-relocated-consumer \
  -DCAORS_BUILD_CONFIG=Release \
  -DCAORS_SHARED=<ON-or-OFF> \
  -P cmake/caors_verify_installed_package.cmake
```

脚本会重新创建四个显式输出目录，执行 install、两轮独立 consumer configure/build/run，并检查
迁移后的配置不引用旧前缀。为避免误删，所有输出目录必须是当前仓库 `build/` 下彼此不嵌套的具体
子目录。consumer 当前覆盖基础库、tasks executor、tasks coroutine，以及通过公开 database 和双
executor 实际加载资源的 resources coroutine API。

常见问题：

- `find_package` 找不到 CAORS：确认传入的是安装前缀，而不是 producer 构建目录；
- 找到错误版本：清理 consumer cache，并检查 `caors_DIR` 是否位于预期前缀；
- Shared consumer 启动失败：检查目标进程的动态库搜索路径，不要用另一个构建目录中的 DLL/SO；
- 移动安装树后仍引用旧路径：必须使用全新的 consumer 构建目录重新配置。

## 嵌入宿主与依赖复用

`CAORS_LIBRARY_TYPE` 为 `AUTO`（默认）、`STATIC` 或 `SHARED`，AUTO 跟随 `BUILD_SHARED_LIBS`，
显式模式不改变宿主全局选项。导出宏与测试使用 CAORS 实际模式；子项目默认关闭自身测试。
测试启用还需 `BUILD_TESTING`；关闭测试不创建 Catch2，关闭 benchmark 不创建 ctrack。
外部 Catch2 需要同时提供 `Catch2::Catch2` 和 `Catch2::Catch2WithMain`，已验证版本为 3.15.0；
通过包元数据或 `Catch2_VERSION` 声明实际版本。外部 ctrack 使用 `ctrack_VERSION=1.1.0`。
未知或不同版本会给出明确配置错误；不静默组合内外两套测试框架。

日志使用 `CAORS_USE_DEBUG_OUTPUT`、`CAORS_USE_LOG`、`CAORS_USE_LOG_SEVERITY`。
旧无前缀选项仅提供首次配置默认值并发出弃用提示，新名称优先；旧缓存可用 CMake `-U` 删除。
静态 CAORS 默认启用 PIC，宿主显式设置 `CMAKE_POSITION_INDEPENDENT_CODE` 时按其设置。
动态库默认隐藏内部符号，通过公共导出声明暴露 API；SONAME 使用 major.minor，
CMake 包兼容匹配限制在同一 minor，不保证任意工具链 C++ ABI 或多版本对象混用。
