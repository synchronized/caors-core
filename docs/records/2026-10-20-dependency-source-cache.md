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

## 来源核对与首批验证

Catch2 解析注释 tag 后固定源码 commit 6ee0826dcae55ed1e06b2c5701981221e979e1e6；
ctrack 固定 v1.1.0 对应 commit 6dfa9b0eef26507cfa9bc17ae4d817ab011a28f5。
精确归档 URL 和 SHA-256 由清单维护。下载后按 LF 归一化比对：Catch2 聚合头相同，
cpp 仅将原有 catch_all.hpp include 还原为上游 catch_amalgamated.hpp 后相同；ctrack 单头完全相同。
未携带额外第三方补丁，也未执行版本字段滞后的 ctrack 上游 CMake。

本机代理路径到 codeload 返回 TLS EOF；只在本次验证进程中为该主机设置 NO_PROXY 后，
CMake 仍以 TLS_VERIFY ON 完成两个真实冷下载、SHA 校验、解压及发布；随后 OFFLINE=ON 命中通过。
未关闭 TLS 校验，未修改系统代理，也未依赖 Granit 的缓存作为真实下载证据。
日志 build/dependency-migration/prepare-real.log；归档和比对元数据在同目录。

独立缓存契约脚本通过，覆盖本地归档冷/暖/离线、坏摘要、不完整源码、并发发布、锁超时、
进程释放及按项隔离。全仓构建、两个基准和安装/跨平台验收尚在收口，未声明通过。

## 本机集成验收

Clang 23.1.1 / MSVC ABI / Shared Debug，build/v030-clang-debug：全仓及两个基准目标构建通过，
最终 CTest 99/99 通过，日志 clang-build.log、clang-final-test.log 位于 build/dependency-migration。
原始和迁移安装 consumer 各 6/6 通过，clang-package.log 保存结果；不安装或导出测试/基准依赖。

本机 MSVC 14.41 / Shared Release：两类基准构建通过，11/11 CMake 契约通过；
日志 msvc-benchmark-build.log、msvc-final-cmake-test.log。Clang/MSVC 下聚合基准 1 迭代、
快照基准 3 样本/1000 绑定均正常运行，仅作可运行性验证，不是性能对比。

首轮集成的缓存契约在 Windows 深层构建目录中遇到解压子进程启动失败；
缩短 staging 名称，保留完整锁键与发布摘要后，在相同深层目录复验通过。未通过缩短测试路径绕过。
归档和源码重命名均使用 NO_REPLACE，不覆盖已有目标；非归档目录不会递归删除。

跨平台验收尚未完成；计划中的正式 v0.6.0 发布仍未执行。
