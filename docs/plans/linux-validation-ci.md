<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Linux 跨平台验证与 CI 开发计划

## 文档状态

- 当前状态：preset、ASan 接入、执行清单和 CI 已实现，等待 Linux runner 完成实跑验证；
- 目标：建立可重复的 Linux Clang/GCC、Static/Shared、Debug/Release 与 ASan 验证矩阵；
- 首批平台：GitHub Actions Ubuntu x86_64 和开发者本地 Linux；
- 非目标：首批不引入交叉编译、真实大端硬件、性能回归门禁或自动发布。

## 背景

当前 Windows Clang Shared Debug/Release 与 Windows MSVC 静态 Debug/Release 已完成全量验证。
以下计划仍有共同的 Linux 验证缺口：

- `messagepack-backend.md`：Clang/GCC 与协议字节序测试向量；
- `scoped-temporary-memory.md`：Clang/GCC 与 ASan；
- `library-plugin-system.md`：POSIX Debug/Release 和 use-after-unload；
- `registry-overlay-safe-unload.md`：POSIX Shared 与 ASan；
- `meta-type-data-redesign.md`：Linux Clang/GCC Debug/Release。

现有 `CMakePresets.json` 已提供 Linux Clang/GCC 静态 Debug/Release，但尚无 Linux Shared 和
ASan preset；仓库也尚未建立 `.github/workflows`。

## 设计结论

### 验证分层

验证拆成三个相互独立的层次，失败时能够直接判断问题边界：

1. Static 基础矩阵：Clang/GCC × Debug/Release；
2. Shared 插件矩阵：Clang Debug/Release，覆盖真实 `.so` 加载、detach 和 unload；
3. ASan 矩阵：Clang Shared Debug，覆盖完整测试并重点检查插件卸载和临时内存。

首批不把 ASan 与普通矩阵合并，也不同时叠加 UBSan，避免不同诊断来源互相干扰。GCC Shared
作为后续补充项，不阻塞首批 POSIX 插件验证。

### 构建并行度

项目 preset 不固定 `jobs`。本地默认不指定 `-j` 或 `--parallel`，让底层构建工具采用默认值；
发生 OOM 时由执行者降低并行度。

GitHub Actions runner 属于已知资源环境，可以在 workflow 中设置
`CMAKE_BUILD_PARALLEL_LEVEL=2`。该值只约束 CI，不传播给项目使用者。

### Sanitizer 接入

新增 CMake 选项：

```cmake
option(CAORS_ENABLE_ASAN "启用 AddressSanitizer" OFF)
```

ASan 使用 target 级 `target_compile_options()` 与 `target_link_options()` 接入，不通过全局
`CMAKE_CXX_FLAGS` 注入。选项需要从 `caors` 目标传播到使用公共模板的测试和插件目标，保证所有跨
边界对象采用一致的 sanitizer ABI。

首批 Clang/GCC 选项：

```text
-fsanitize=address
-fno-omit-frame-pointer
```

运行环境使用：

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
```

如果某个平台的 LeakSanitizer 不可用，应在验证记录中明确说明，不允许静默忽略。

### MessagePack 字节序

首批验证通过固定字节测试向量确认 MessagePack 多字节整数、float64 和容器长度始终使用协议规定的
大端顺序，不依赖主机字节序。真实大端 CPU 或模拟器验证属于后续增强，不作为首批门禁。

## 目标结构

```text
CMakePresets.json
├─ linux-clang-debug/release
├─ linux-gcc-debug/release
├─ linux-clang-shared-debug/release
└─ linux-clang-shared-asan

docs/checklists/linux-validation.md
└─ 本地环境、命令、结果和异常记录模板

.github/workflows/linux.yml
├─ linux-static
├─ linux-shared
└─ linux-asan
```

## 分阶段实施

### 阶段 0：环境与基线确认

- [ ] 记录 Linux 发行版、内核、架构和容器/虚拟机环境；
- [ ] 记录 CMake、Ninja、Clang、GCC、glibc、libstdc++ 和 linker 版本；
- [ ] 确认 `dlopen`、`dlsym`、`dlclose` 可用于真实插件测试；
- [ ] 在未修改代码前运行现有 Linux 静态 preset，保存首次失败基线。

完成标准：能够区分环境缺失、构建配置问题和代码问题。

### 阶段 1：补齐 Linux Shared preset

- [x] 增加 `linux-clang-shared-debug`；
- [x] 增加 `linux-clang-shared-release`；
- [x] 增加对应 build/test preset；
- [x] 确认 Shared preset 启用 `BUILD_SHARED_LIBS=ON`；
- [x] 从 CMake 目标关系确认 plugin protocol 与 meta plugin identity 测试被纳入；
- [x] 保持平台 condition 和现有 preset 命名层次一致。

完成标准：Linux Clang Shared Debug/Release 可配置、构建并发现完整插件测试。

### 阶段 2：接入 ASan

- [x] 增加 `CAORS_ENABLE_ASAN`；
- [x] 使用 target 级编译和链接选项；
- [x] 增加 `linux-clang-shared-asan` configure/build/test preset；
- [ ] 验证主库、测试 executable 和测试 plugin 都带有 ASan；
- [x] 明确不支持的编译器或平台诊断；
- [x] 在正式构建文档中说明用法和限制。

完成标准：ASan 能检测测试进程和动态插件中的内存错误，运行期不存在 sanitizer ABI 混用。

### 阶段 3：编写人工执行清单

- [x] 新增 `docs/checklists/linux-validation.md`；
- [x] 记录环境检查命令；
- [x] 列出 Static、Shared 和 ASan 的配置、构建、测试命令；
- [x] 提供提交号、测试总数、失败项、ASan 结果和已知限制模板；
- [x] 增加 MessagePack 固定字节向量专项；
- [x] 增加插件 detach/unload 和 lease 生命周期专项；
- [x] 增加临时内存成功/失败回滚专项。

完成标准：开发者不依赖对话上下文即可在新 Linux 环境完整复验。

### 阶段 4：建立 GitHub Actions

- [x] 新增 `.github/workflows/linux.yml`；
- [x] 支持 `pull_request`、`push` 到 `main` 和手动触发；
- [x] `linux-static` 使用 Clang/GCC × Debug/Release 矩阵；
- [x] `linux-shared` 使用 Clang Shared Debug/Release 矩阵；
- [x] `linux-asan` 独立运行 Clang Shared Debug；
- [x] 矩阵设置 `fail-fast: false`；
- [x] 设置合理的 job 超时；
- [x] 失败时上传 `Testing/Temporary/LastTest.log`；
- [x] 初版不引入 ccache，先建立稳定基线。

完成标准：普通构建、插件边界和 sanitizer 失败可以在不同 job 中独立定位。

### 阶段 5：全量验证与计划回写

- [ ] Linux Clang Static Debug/Release 全量通过；
- [ ] Linux GCC Static Debug/Release 全量通过；
- [ ] Linux Clang Shared Debug/Release 全量通过；
- [ ] Linux Clang Shared ASan 全量通过；
- [ ] plugin detach/unload 专项连续运行，未发现 use-after-unload；
- [ ] 临时内存与 MessagePack 压力测试在 ASan 下通过；
- [ ] 将编译器、测试数量和提交号写入执行清单；
- [ ] 回写并关闭关联计划中的 Linux/POSIX 待验证项。

完成标准：CI 和本地清单均能复现相同结果，关联计划不再保留未验证声明。

## CI 验收门禁

- 配置、构建或测试任一阶段失败时 job 必须失败；
- `ctest` 无测试可运行时必须失败；
- ASan 发现错误或泄漏时必须返回非零；
- Shared job 必须实际构建并运行插件协议测试；
- 不允许通过排除失败测试让矩阵变绿；
- 临时平台豁免必须写明原因、影响范围和移除条件。

## 风险与缓解

### GitHub runner 内存不足

模板密集测试可能在默认并行度下 OOM。CI 首批使用较低的
`CMAKE_BUILD_PARALLEL_LEVEL`；本地仍由使用者按可用内存决定，不写入 preset。

### ASan 与动态卸载交互

插件必须与 host 使用一致的 sanitizer 选项和运行库。若只有一侧启用，结果不具备诊断价值，应在
CMake 配置阶段或 CI 构建结构中避免这种组合。

### LeakSanitizer 平台差异

不同 libc、容器权限和 runner 配置可能影响 leak 检测。首次实施应保存完整环境信息和 ASan 输出，
不得只记录“测试通过”。

### CI 时间增长

初版优先保证矩阵完整。稳定后再评估 ccache、路径过滤和按变更范围拆分，不在建立基线前提前优化。

## 暂缓内容

- GCC Shared 与 GCC ASan；
- UBSan、TSan 和 MSan；
- macOS、ARM64 和真实大端平台；
- benchmark 性能回归门禁；
- 自动打包与发布；
- 跨发行版矩阵；
- 增量 typed MessagePack reader。

这些内容在基础 Linux 矩阵稳定后按实际缺陷和发布需求单独规划。
