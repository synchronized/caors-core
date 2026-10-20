<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# CI 与验证策略

本页维护当前工作流职责、触发方式和按改动选择检查的规则；具体构建命令见
[构建指南](build.md)，开发顺序见[开发规范](development.md)，发布门槛见[发布清单](../checklists/release-readiness.md)。
工作流 YAML 是实际触发和矩阵的实现来源，历史通过记录不能替代当前发布提交的验证。

## 当前工具能力

| 入口 | 触发 | 实际覆盖 |
| --- | --- | --- |
| [Linux](../../.github/workflows/linux.yml) | 手动；每周一北京时间 02:00 | 下列 5 组；普通 Release 验证安装迁移，动态 Release 审计符号与依赖 |
| [Windows](../../.github/workflows/windows.yml) | 手动；每周一北京时间 02:00 | 下列 3 组，均验证安装与迁移 consumer |
| [Linux Stress](../../.github/workflows/linux-stress.yml) | 手动 | Clang Shared ASan；插件卸载 20 轮；内存与 MessagePack 10 轮 |
| [MSVC 包装脚本](../../scripts/msvc.ps1) | 本地调用 | 初始化 Visual Studio 环境，按 preset 配置、构建与测试 |
| [安装包验证](../../cmake/caors_verify_installed_package.cmake) | 本地或 Linux 工作流调用 | 基础、Tasks、Tasks coroutine、Resources coroutine、共享宿主、静态宿主六类 consumer，原始与迁移前缀 |

当前没有 Documentation 或 Release workflow，没有版本准备和正式发布脚本。
普通分支 push、PR 更新及 main push 不自动运行 Linux/Windows CI；本页不引入新的自动触发。
无异常公共头探针不等于完整无异常运行矩阵，ASan 不等于 UBSan，consumer 检查不等于成品 SDK 发布。

## 常规矩阵

Linux 共 5 组：`linux-clang-release`、`linux-gcc-release`、`linux-clang-asan`、
`linux-clang-ubsan`、`linux-clang-static-release`。
Windows 共 3 组：`windows-clang-release`、`windows-msvc-release`、`windows-clang-static-release`。
没有 static 的名称均为动态库。ASan 与 UBSan 保持独立 Debug 配置，分别检查内存和未定义行为。
普通 Release 覆盖两种编译器与两种链接方式，不再排列全部 Debug/Release 笛卡尔积。

Debug、GCC static、MSVC static、clang-cl、Visual Studio generator presets 仍可本地按需验证，
不宣称这些配置每次都由常规 CI 覆盖。涉及调试宏、调试 CRT、断言或特定编译器问题时补相应 Debug/工具链组合。

## 按改动选择检查

| 改动 | 最低相关验证 | 扩大验证的条件 |
| --- | --- | --- |
| 纯 Markdown | 文件链接、锚点、分类可达性、命名、UTF-8/LF、`git diff --check` | 改了操作命令时核对实际脚本/目标；不触发完整 Linux CI |
| 局部实现或缺陷 | 相关构建与行为测试，必要时补失败路径 | 跨模块影响或本地无法覆盖的平台进入 Linux CI |
| 公共头、ABI、插件、内存、并发、序列化或构建 | 按实际影响运行相关配置、失败路径及必要 consumer；仅改注释不扩大测试 | 核心机制改变、跨平台行为或关键集成批次完成时，合并前运行受影响平台的常规 CI；卸载等风险按需增加 Stress |
| 仅版本号或包元数据 | 版本常量/运行时查询、包版本及受影响安装 consumer | 改变链接、导出或依赖契约时扩大到常规 CI |
| 发布候选 | 发布清单规定的最终提交验证，默认使用常规矩阵 | 宣布支持但无法验证的平台阻塞该发布范围；不得用历史结果充数 |

## 手动运行与记录

先推送需要验证的同一分支，再运行实际存在的 workflow：

```sh
gh workflow run linux.yml --ref <branch>
gh workflow run windows.yml --ref <branch>
gh workflow run linux-stress.yml --ref <branch>
gh run list --workflow linux.yml --branch <branch>
```

命令中的 `<branch>` 需替换为实际分支。记录运行 URL、head SHA、环境、通过/失败和未验证范围。
同一 workflow/ref 的新运行会取消未完成的旧运行；旧运行的取消或部分通过不代表新提交通过。
需要等待时使用已确认的 run ID 查看结果，不通过空提交反复触发观察 CI。

中间实现先做专项验证，在完整批次上集中运行常规 CI。没有代码、配置或依赖变化时不重复启动同一矩阵；
纯文档合并不再补跑，普通合并优先核对与已验证提交的实际差异。

本机验证的 preset 与命令以构建指南为准；普通 PowerShell 中使用 MSVC 包装脚本。
发生 OOM 时降低并行度，失败后保留日志，不排除失败测试或降低检查等级换取通过。

## 合并与发布的提交身份

日常合并记录实际测试过的提交；若之后修改了代码、配置或依赖，应按影响补验证。
纯文档收口可以说明与已验证提交的差异，不把它描述为重新运行过测试。
发布要求更严格：最终 main SHA 必须有对应验收证据；功能分支通过后仍不能自动声称合并 SHA 已验证。
当前没有发布工作流，发布者需在最终 SHA 的干净工作树完成验证和制品检查。

自动化目标与未实施项见[发布自动化计划](../plans/release-automation.md)。

## 快照基准可运行性

既有 linux-clang-release 和 windows-msvc-release 任务复用已构建库，额外编译
caors_provider_snapshot_benchmarks，并运行 1000 绑定、3 样本的最小 smoke，上传汇总与逐次 CSV。
不增加常规矩阵组合，不设耗时阈值；这些数据只证明程序可运行，不能代替固定环境性能验收。
