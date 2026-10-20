<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 第三方依赖源码缓存迁移记录

## 范围与状态

2026-10-20，用户授权合并待合入的已验收改动，并迁移第三方依赖。
PR #47 在确认 e6c1f487 的 Linux 5/Windows 3 全部通过后合入 main，合并提交 d175fe58。
从该 main 创建 feature/v0.6.0-dependency-cache；工程版本仍为 0.5.0，目标范围见版本页。
本记录尚为实施证据，不代表 v0.6.0 已发布。

## 环境与执行

Windows x64，PowerShell；CMake、Clang/MSVC，相关验证结果完成后补齐。
Granit/Gneiss 仅作本地实现参考，未修改相邻项目。
