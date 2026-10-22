<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 资源核心与来源适配边界验证

## 范围与环境

2026-10-22，feature/resource-source-boundary，基于 main b786c203。
这是[API 收敛计划](../plans/io-assets-api-consolidation.md)在首批集成后的第二批实现，尚未发布。
Windows x64、Clang 23.1.1、MSVC ABI、共享 Debug；Web 使用 Emscripten 5.0.6、wasm32 静态单线程。

## 实现与迁移

- database 移除 provider 指针、set_provider/provider 和 bind/sync/replace_manifest 成员；仅通过通用捕获器取得批次数据。
- manifest.h 不再包含来源协议；三个清单绑定自由函数归入 manifest_binding.h。resources.h 聚合头仍包含该适配，直接使用函数的调用方应包含新头。
- manifest_file_system 显式包含其使用的 file_system，修复原先依赖 manifest 间接包含的问题。
- 工程 provider 移入私有 project_state，公开访问改用 project_builder::provider；类型工厂的 provider 引用及已有来源快照行为保持。
- manager 为现有工程默认安装 provider 捕获回调；独立 database 默认没有捕获策略。清空捕获器关闭捕获，builder::use_provider_snapshot 显式恢复工程策略，活动操作期间仍返回 frozen。
- 测试、示例和安装消费端迁移；用户需统一重编译库与调用方。

## 方法与结果

原生构建并执行 assets.asset_manager/io/integration，以及 resources.async/database/manifest/pipeline/provider/reload/coroutine，
CTest 10/10 通过。独立 database 测试不设置 provider 或捕获器，成功完成异步加载并清理实际操作。
空捕获器和返回空结果的捕获器均不回退；显式工程策略恢复及原有依赖快照共享/重挂载测试通过。
独立翻译单元包含 database.h，检查它不传递包含 source.h 或 file_system.h；manager/database 的 provider 公开入口缺失、builder 提供该入口的编译期检查通过。

宿主、VFS 原生/worker、配置诊断、alpha/beta 工程管线示例构建运行通过。
使用新建 build/api-source-installed-* 目录执行 caors_verify_installed_package.cmake，原始和迁移前缀六类 consumer 各 6/6 通过。
Web 核心及 caors_web_protocol_test 重新构建，Node 协议 CTest 1/1 通过，包含新增独立 database 测试。
原始日志保留在忽略的 build/api-source-complete.log、api-source-installed.log、api-source-bare-web.log，不作为仓库交付文件。

## 限制与后续

本批尚未运行远端 Linux/Windows 完整矩阵、真实浏览器和压力验证；第一批的成功 CI 不替代本批验收，合并前执行受影响平台检查。
provider 仍包含定位和读写协议，本批是让资源核心不再持有该适配，不是完成 provider 内部职责拆分。
manifest 仍使用 location 字符串，不承诺任意数据库标识语义；manager 仍兼容安装工程捕获策略，未完成完全按需创建工程状态。
loader 同步/异步协议统一、同步资源入口退出与独立文件 API 整理仍待后续批次；不删除既有写入能力。
