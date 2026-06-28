<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 开发规范

本文档说明 CAORS 的目录结构、代码风格、格式化、静态检查和新增模块规则。

## 目录结构

```text
.
├── 3rd/                    # 第三方依赖，当前包含 Catch2
├── cmake/                  # 项目 CMake 工具函数
├── docs/                   # 专题设计文档
├── examples/               # 可运行示例
├── include/caors/          # 公共头文件和版本头模板
├── src/                    # 库目标和实现
│   └── caors/
├── tests/                  # 测试入口
├── CMakeLists.txt
├── CMakePresets.json
└── README.md
```

## 代码规范

- 使用 C++20，不引入低于项目标准的兼容写法。
- 采用偏 STL / RTTR 的库风格：公共 API 使用小写下划线命名，例如 `get_version()`。
- 仅在单个 `.cpp` 文件内部使用的自由函数、变量和辅助类型应放入匿名 namespace，避免暴露翻译单元外符号。
- CMake 使用 target-based 写法，优先使用 `target_sources`、`target_include_directories`、`target_link_libraries` 和 `target_compile_features`。
- 公共头文件模板放在 `include/caors/`，生成头文件放在构建目录下。
- 跨库边界的公共类和非内联函数使用 `CAORS_API` 标记。
- 库实现放在 `src/caors/`。
- 版本号以根 `project(... VERSION ...)` 为唯一来源，不手写重复版本常量。
- 新增模块时同步补充测试目录。
- 第三方源码不默认启用项目级 `-Werror`，避免外部警告影响本项目构建。

## 代码格式与静态检查

项目提供以下配置文件：

- `.clang-format`：基于 LLVM 风格微调，整体偏 STL / RTTR 这类库代码排版。
- `.clang-tidy`：启用 bugprone、performance、portability、modernize 和 readability 相关检查，并排除部分噪声较大的规则。

格式化单个文件：

```sh
clang-format -i src/caors/version.cpp
```

检查单个源文件：

```sh
clang-tidy src/caors/version.cpp -p build
```

`clang-tidy` 依赖 `compile_commands.json`，当前 preset 已开启 `CMAKE_EXPORT_COMPILE_COMMANDS`。第三方目录 `3rd/` 不作为项目代码检查目标。

`include/caors/version.h.in` 包含 CMake 占位符，不直接使用 `clang-format` 格式化；需要检查时以生成后的 `version.h` 为准。

## 新增模块

新增主题模块时建议同步处理：

- 公共 API 放入 `include/caors/` 或对应主题子目录。
- 实现放入 `src/caors/`。
- 行为测试放入 `tests/<topic>/`。
- 示例放入 `examples/<topic>/`，规则见 [examples/README.md](../examples/README.md)。
- 专题文档放入 `docs/<topic>.md`，并在 [docs/README.md](README.md) 中挂入口。
