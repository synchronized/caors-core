<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 第三方依赖入口

本目录维护 CAORS 自有的依赖目标组装，不提交第三方源码副本。
获取、离线、并发锁、恢复及许可证规则见[第三方依赖指南](../docs/guides/third-party-dependencies.md)。
固定身份与摘要由[依赖清单](../cmake/caors_dependency_catalog.cmake)维护，缓存实现为
[caors_dependency_cache.cmake](../cmake/caors_dependency_cache.cmake)。

cache 为可重新填充的本地目录，已由 .gitignore 排除；不得提交缓存或把编译产物放入其中。
