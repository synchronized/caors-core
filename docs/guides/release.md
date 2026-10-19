<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 版本准备与发布流程

本页借鉴 Granit 的阶段边界，规定 CAORS 的版本准备、人工发布与制品复核流程。
当前已有构建、安装、consumer 和 Linux CI，尚无 `release`/`publish` 脚本或 Release workflow。
本页不是自动发布已实现的声明，也不选择下一版本号或执行一次发布。

## 职责和事实来源

| 内容 | 维护入口 |
| --- | --- |
| 工程版本 | 根 [CMakeLists.txt](../../CMakeLists.txt) 的 `project(... VERSION ...)`；不新增第二个版本常量 |
| 版本范围、任务依赖、发布出口 | [versions](../versions/README.md) 中对应版本文档 |
| 单项实现与待办 | 对应 Plan，不复制版本清单 |
| 用户可见变化、破坏性影响和迁移入口 | [CHANGELOG](../../CHANGELOG.md) |
| 版本验证条件 | [发布清单](../checklists/release-readiness.md) |
| 最终提交、环境、测试与制品结果 | 按[验收模板](../templates/release-acceptance.md)建立的 Record |
| 当前兼容和稳定性政策 | [API 稳定性](../reference/api-stability.md) |

生成头 `caors/version.h` 由 `include/caors/version.h.in` 从工程版本生成；运行时版本查询来自同一组常量。
不要手改生成头或在 README 另维护版本号。CMake 包版本匹配策略不等同于 API/ABI 稳定承诺。

## 分支与阶段

```text
feature / fix ──PR──> main ──创建 release/X.Y.Z
                                  │
                         版本范围、版本号、变更和验收收口
                                  │
                             版本准备 PR
                                  ↓
                                 main
                                  │
                       固定最终 SHA，干净构建与完整验收
                                  │
                         制品验证 → tag / Release → 下载复核
```

main 是唯一集成分支。功能和修复先通过各自 PR 合入 main；release 分支从最新 origin/main 创建，
只承载版本号、变更记录、版本文档和验收收口，不作为第二个功能集成分支。
若准备期间发现必须修复的缺陷，先按 fix 流程合入 main，再把 main 合入 release 分支。
默认不重写共享分支历史，不自动执行强推。版本准备 PR 合并后，短期 release 分支即可结束。

## 1. 确定版本范围

从[版本模板](../templates/version-plan.md)建立 `docs/versions/vX.Y.Z.md`，明确目标、非目标、
关联 Plan、依赖、承诺平台、源码或二进制交付形式，以及阻塞项。
不把整个库笼统声明为稳定，也不把已经完成的旧功能自动纳入本次发布范围。
CAORS 仍处于早期阶段，破坏性调整必须记录影响和升级操作，但不新增稳定 C++ ABI 承诺。

## 2. 准备版本提交

在 release 分支中完成：

1. 修改 CMake 中唯一的工程版本，确认生成常量、版本函数及包版本一致。
2. 把 CHANGELOG 的相应 Unreleased 内容移入带版本、日期的章节，保留新的 Unreleased 入口。
3. 版本文档固定范围与门槛；关联 Plan 标明真实完成范围与延期，不保留虚假的“已通过”。
4. 建立候选 Record，记录当前提交与验证证据；发布状态写“候选验收中/已验收，尚未发布”。
5. 相关文档、示例、迁移说明和已知限制完成收口，检查 diff 后提交、推送并创建版本准备 PR。

源码、CMake、依赖或版本配置有变化时按[CI 策略](ci.md)验证，不能把版本准备当作纯 Markdown 改动。
发布前必须已说明范围和风险，不能等 tag 创建后再补关键兼容事实。

## 3. 验收最终 main 提交

版本准备 PR 合并后，获取并固定最终 main SHA。核对本地 main 与 origin/main 一致且工作区干净；
干净工作树中的源版本、生成版本与目标标签 `vX.Y.Z` 一致，标签尚未使用。
main 在验证期间继续前进时，仍使用已冻结的已验收 SHA，不在最后一步重新解析分支名改变发布对象。

执行[发布清单](../checklists/release-readiness.md)，最终 SHA 的证据先保存在按 SHA 标识的 CI 运行、
制品 manifest 或发布附件；发布后把这些链接写回 Record，不为写入自身 SHA 而改变已冻结的发布提交。
预发布标记不能替代检查；不能覆盖的平台应从承诺交付范围中移除并明确说明，或等待环境补齐。
自动创建 tag 前的门禁尚未实现，目前由维护者按清单逐项执行。

## 4. 制品与人工发布

当前可使用经验证提交对应的源码发布；二进制 SDK 只有在完成该批实际文件的干净构建、安装、
consumer 和部署复验后才能交付。`cmake --install` 可用不代表通用 SDK 打包已经完成。

交付二进制时，说明系统、架构、编译器/工具集、CRT、Debug/Release、Static/Shared、异常/RTTI、
CAORS 配置与私有依赖边界；这类 C++ 库不能仅凭“同为 Windows x64”声称可通用链接。
每批制品附 SHA-256 和提交/环境 manifest；安装 consumer 必须针对将要交付的同一批文件执行。

确认清单通过后，维护者为固定 SHA 创建 `vX.Y.Z` 标签和 GitHub Release，预发布阶段明确标记。
Release 说明引用对应 CHANGELOG、版本范围和验收记录，包含交付形式、已知限制与迁移方式。
当前没有“自动构建后才建 tag”的工具保证，不能直接调用 Granit 的脚本名代替人工步骤。

## 5. 发布后复核与失败处理

从实际 Release 下载入口重新获取制品，检查文件集、SHA-256、manifest、标签 SHA 和下载状态。
源码制品核对归档内容与标签；二进制制品核对已验收字节，不能只重用本地原文件证明下载成功。
确认后将版本文档和 Record 更新为“已发布”，附公开链接及复核结果；发布前不可提前写已发布。
这一更新只记录公开事实，不用来补做发布前未完成的设计或验收。

构建或发布前检查失败时停止，修复后重新固定提交与验证。
若已经公开的标签或制品存在问题，记录失败证据并发布修订版本，不移动公开标签或静默替换制品。
发布操作中断或网络超时时先查询真实 tag/Release 状态，不盲目重试造成提交或制品不一致。

脚本门禁、Windows/Release workflow、可复现 SDK 打包和公开下载校验的实现范围见
[发布自动化计划](../plans/release-automation.md)。
