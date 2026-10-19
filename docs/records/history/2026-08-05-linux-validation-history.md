<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Linux 历史验收记录

以下为原 Linux 执行清单记录的 2026-07-30 至 2026-08-05 验收证据，保留当时环境、链接、数量和限制。
这些记录不代表当前提交已验证；现行操作见[Linux 清单](../../checklists/linux-validation.md)。

## 已完成记录

### 2026-07-30 GitHub Actions

- 提交：`26246758a249ad241586af5602a410c67a03580c`；
- 运行：[Linux #30535633441](https://github.com/synchronized/caors/actions/runs/30535633441)；
- 环境：GitHub-hosted Ubuntu 24.04 LTS x86_64 虚拟机，Linux
  `6.17.0-1020-azure`；
- 工具：CMake 3.31.6、Ninja 1.13.2、Clang 18.1.3、GCC 13.3.0、glibc 2.39；
- Static Clang Debug/Release：各 81 项测试全部通过；
- Static GCC Debug/Release：各 81 项测试全部通过；
- Shared Clang Debug/Release：各 83 项测试全部通过；
- Shared Clang ASan：83 项测试全部通过；
- ASan 环境：`detect_leaks=1:halt_on_error=1`，未报告 AddressSanitizer 或
  LeakSanitizer 错误；
- OOM：未发生，CI 使用 `CMAKE_BUILD_PARALLEL_LEVEL=2`；
- 已知限制：本次日志未单独记录 libstdc++ 与 linker 版本；尚未执行独立的多轮插件卸载和临时内存/
  MessagePack ASan 压力 job。

### 2026-07-31 GitHub Actions 压力验证

- 提交：`bb993d9f5075645496370337db412ceed6102557`；
- 基础矩阵运行：[Linux #30596369100](https://github.com/synchronized/caors/actions/runs/30596369100)，
  7 个 Static、Shared 与 ASan job 全部通过；
- 压力验证运行：
  [Linux Stress #30597404673](https://github.com/synchronized/caors/actions/runs/30597404673)；
- 环境：GitHub-hosted Ubuntu 24.04 LTS x86_64 虚拟机；
- 工具：Clang 18.1.3、GNU ld 2.42、glibc/ldd 2.39，
  libstdc++ 路径为 `/usr/lib/gcc/x86_64-linux-gnu/13/libstdc++.so`；
- ASan 覆盖检查：主库、测试程序和 C++ 测试插件的编译、链接规则全部通过；
- 插件卸载压力：plugin scope、meta plugin identity 和 plugin protocol 各连续 20 次通过；
- 临时内存与 MessagePack 压力：6 项 memory 测试与 1 项 MessagePack 测试各连续 10 次通过；
- ASan 环境：`detect_leaks=1:halt_on_error=1`，未报告 AddressSanitizer 或
  LeakSanitizer 错误；
- OOM：未发生，CI 使用 `CMAKE_BUILD_PARALLEL_LEVEL=2`；
- 已知限制：尚未覆盖 GCC Shared/ASan、UBSan、TSan、MSan、ARM64 或真实大端平台。

### 2026-08-05 内存域重构集成验证

- 提交：`4cf1c3c2e8ba558ec0bc30b8711eb71cb5cb6a19`；
- 运行：[Linux #30996692832](https://github.com/synchronized/caors-core/actions/runs/30996692832)；
- 环境：GitHub-hosted Ubuntu 24.04 LTS x86_64 虚拟机，Linux
  `6.17.0-1020-azure`；
- 工具：CMake 3.31.6、Clang 18.1.3、GCC 13.3.0；
- Static Clang Debug/Release：各 83 项测试全部通过；
- Static GCC Debug/Release：各 83 项测试全部通过；
- Shared Clang Debug/Release：各 85 项测试全部通过；
- Shared Clang ASan：85 项测试全部通过，未报告 AddressSanitizer 或 LeakSanitizer 错误；
- Static/Shared Release 安装包消费测试全部通过；
- 已知限制：固定 Linux 环境 UBSan 仍待环境部署后执行。
