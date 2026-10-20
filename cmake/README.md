<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# CMake 模块

项目私有模块使用 `caors_*.cmake`，辅助函数使用 `caors_` 前缀；开发约定见
[开发规范](../docs/guides/development.md)，使用与验证命令见[构建指南](../docs/guides/build.md)。

| 文件 | 职责 |
| --- | --- |
| [caors_version.cmake](caors_version.cmake) | 唯一维护工程版本；根 `project()` 读取 `CAORS_PROJECT_VERSION`，派生版本组件、生成头和包版本 |
| [caors_dependency_catalog.cmake](caors_dependency_catalog.cmake) | 唯一维护开发依赖版本、固定提交、下载地址、摘要及许可证 |
| [caors_dependency_cache.cmake](caors_dependency_cache.cmake) | 共享只读源码、离线、摘要校验与按项并发锁 |
| [caors_warnings.cmake](caors_warnings.cmake) | 警告选项与 `caors_target_compile_warnings()`，选项只作用于目标自身 |
| [caors_verify_memory_domains.cmake](caors_verify_memory_domains.cmake) | 配置期间检查内存域声明 |
| [caors_verify_installed_package.cmake](caors_verify_installed_package.cmake) | 检查安装包及迁移后的独立 consumer |
| [caorsConfig.cmake.in](caorsConfig.cmake.in) | `find_package(caors)` 包配置模板，遵循 CMake 包命名约定 |

`CMakeLists.txt`、生成的 `caorsConfig.cmake`、`caorsConfigVersion.cmake` 和目标导出文件保留标准名称。
第三方模块与 CMake 自带命令不套用项目私有命名规则。
