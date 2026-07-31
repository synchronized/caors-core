<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 安装包与外部 Consumer 验证计划

## 文档状态

- 当前状态：设计完成，等待实施；
- 目标：验证安装后的 CAORS 能被独立 CMake 项目查找、链接、运行并随安装前缀移动；
- 首批范围：Windows Clang 与 Linux Clang 的 Static/Shared Release；
- 非目标：不在本计划中引入 CPack、系统包管理器、FetchContent 封装或新的公共 API。

## 背景

当前工程已经安装公共头文件、生成的 `caors/version.h`、库文件、许可证，以及
`caorsConfig.cmake`、版本文件和 `caorsTargets.cmake`。文档也给出了：

```cmake
find_package(caors CONFIG REQUIRED)
target_link_libraries(app PRIVATE caors::caors)
```

但现有 examples 和 tests 都在 CAORS 源码树内构建，无法证明安装接口完整。源码树构建可能掩盖：

- 公共头文件或生成头文件漏装；
- 导出目标携带源码目录、构建目录或缺失的传递属性；
- Static/Shared 编译定义不正确；
- 外部 consumer 无法解析符号或在运行时找到共享库；
- package config 只能在原安装路径工作。

因此需要一个不引用 CAORS 源码目录的独立 consumer，并由安装验证流程完成 producer、install、
consumer configure/build/run 的完整闭环。

## 首批安装契约

- 包名为 `caors`，首批只导出 `caors::caors`；
- consumer 通过 Config 模式和显式安装前缀查找，不回退到源码树或用户 package registry；
- `cxx_std_20`、Static/Shared 定义和必要的公共编译属性由导出目标传递；
- 公共头文件与生成的 `caors/version.h` 必须来自安装前缀；
- Static consumer 不依赖 CAORS DLL；
- Shared consumer 必须链接安装包中的 import/shared library，并能在受控运行时路径中找到 DLL/SO；
- 安装树移动后，package config、头文件和库路径仍相对于新前缀解析。

首批不定义 CMake components。未来只有出现多个可独立安装、独立链接的目标时，才增加 component
语义。

## Consumer 样本

新增独立小工程，建议放在 `tests/installed_package/consumer/`：

```text
tests/installed_package/consumer
├─ CMakeLists.txt
└─ main.cpp
```

`CMakeLists.txt` 只允许：

```cmake
find_package(caors CONFIG REQUIRED)
add_executable(caors_installed_consumer main.cpp)
target_link_libraries(caors_installed_consumer PRIVATE caors::caors)
```

样本程序至少验证：

- 包版本头可以包含并读取；
- 基础公共 umbrella header 可以独立使用；
- 注册一个小型反射类型；
- 使用 meta 查询属性；
- 完成一次 JSON 写出和读回；
- 进程正常退出。

这组路径同时覆盖模板实例化、导出符号、默认 runtime 状态和 serialization，不把全部 examples
复制进安装验证。

## 隔离与防误通过

- producer、安装前缀和 consumer 使用彼此独立的构建目录；
- consumer configure 显式设置 `CMAKE_PREFIX_PATH` 或 `caors_DIR`；
- 禁用 CMake user/system package registry，避免找到机器上旧版本 CAORS；
- consumer 不添加 CAORS include directory，不使用 `add_subdirectory()`，也不链接 build-tree target；
- 验证日志记录实际使用的 `caors_DIR` 和 imported target 路径；
- 各验证目录按 preset、链接类型和配置隔离，避免 Static/Shared 产物互相污染。

## Static 与 Shared

### Static

- 使用 Static Release producer；
- 安装后配置独立 consumer；
- 确认 `CAORS_STATIC_DEFINE` 通过 exported target 生效；
- 构建并运行 consumer。

### Shared

- 使用 Shared Release producer；
- consumer 必须链接安装前缀中的共享库或 Windows import library；
- Windows 运行时只将该安装前缀的 `bin` 临时加入子进程 `PATH`；
- Linux 使用构建期 RPATH 或只对验证子进程设置安装前缀 `lib` 的运行时搜索路径；
- 不把 DLL/SO 复制回源码树或 consumer 源码目录。

运行时路径调整只作用于 consumer 子进程，不污染开发者全局环境。

## 可重定位验证

首次 install + consumer 通过后，将完整安装树复制或移动到另一个无关前缀，并使用全新的 consumer
构建目录再次 configure/build/run。第二次配置必须只提供新前缀，且缓存和日志中不得引用旧前缀。

该验证检查 CMake package 的路径可重定位性，不承诺二进制可以跨机器、跨编译器、跨 CRT 或跨不兼容
系统版本移动。

## 自动化入口

增加单一验证脚本，接收以下显式参数：

- producer build directory；
- install prefix；
- consumer build directory；
- build configuration；
- Static/Shared 模式；
- 可选 relocation prefix。

脚本按顺序执行 install、consumer configure、build、run 和 relocation 验证；任一步失败立即返回
非零。脚本不得自行选择或删除宽泛目录，所有输出目录必须由调用方明确传入并位于工作区构建目录。

本地验证首先使用现有：

- `windows-clang-release`；
- `windows-clang-shared-release`。

不为 consumer 复制一套新的 producer preset。

## CI 策略

完整 Linux workflow 已降低触发频率，因此安装验证并入既有 Release job，而不新增长期独立矩阵：

- `Static · linux-clang-release` 构建测试后验证 Static install/consumer；
- `Shared · linux-clang-shared-release` 构建测试后验证 Shared install/consumer；
- Debug、GCC 和 ASan 首批不重复该流程。

这样每次完整 CI 增加两个短程 consumer 验证，但不增加 runner 数量。GitHub Actions 只验证功能正确性，
不记录性能数据。

## 实施阶段

### 阶段 0：冻结契约

- [x] 明确包名、导出目标、首批平台和非目标；
- [x] 明确隔离、运行时路径和可重定位规则；
- [x] 明确 CI 不新增矩阵。

### 阶段 1：独立 consumer

- [ ] 增加最小外部 CMake 工程；
- [ ] 覆盖版本、基础 API、meta 和 JSON round trip；
- [ ] 确认工程不依赖源码树。

### 阶段 2：本地安装验证

- [ ] 增加参数明确的验证脚本；
- [ ] 完成 Windows Clang Static Release；
- [ ] 完成 Windows Clang Shared Release；
- [ ] 完成安装前缀移动后的重新消费验证。

### 阶段 3：Linux CI

- [ ] 接入现有 Static Clang Release job；
- [ ] 接入现有 Shared Clang Release job；
- [ ] 完成一次手动完整 CI。

### 阶段 4：文档收尾

- [ ] 回写 `docs/build.md` 的验证保证与故障排查；
- [ ] 记录 Static/Shared、运行时库查找和可重定位边界；
- [ ] 将计划状态改为完成。

## 完成标准

- consumer 只凭安装前缀和 `find_package(caors CONFIG REQUIRED)` 即可配置；
- Static/Shared consumer 均能编译、链接并运行；
- 安装树移动后全新 consumer 仍能成功；
- 导出配置不泄漏 CAORS 源码目录或 producer 构建目录；
- Windows 本地验证与 Linux Clang Release CI 均通过；
- 正式构建文档与实际验证流程一致。

## 风险

- Windows Shared consumer 可能因 `PATH` 中存在其他 CAORS DLL 而误通过，必须限制并记录运行时路径；
- CMake user package registry 可能掩盖错误安装前缀，必须在 consumer configure 时禁用；
- relocation 验证如果复用旧 consumer cache 会失去意义，必须使用全新构建目录；
- 递归地从普通 CTest 内再次构建 producer 容易造成并发和生成器问题，因此安装验证采用显式脚本与 CI
  步骤，不嵌入每个普通单元测试进程。
