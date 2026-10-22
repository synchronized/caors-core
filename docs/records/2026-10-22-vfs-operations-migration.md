<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# VFS 整体操作与消费迁移记录

## 范围

承接 [VFS 收敛计划](../plans/vfs-api-consolidation.md)和[只读记录](2026-10-22-vfs-read-context.md)。
本批基于 fed639ad，分支 feature/resource-source-boundary，PR #58。

io_context 新增整文件 write、stat 和 list；新 backend 按能力实现，缺省返回 unsupported。
写入拥有缓冲区，创建或截断，检查实际关闭错误；取消不回滚已发生写入。
目录结果映射回逻辑路径，拒绝非法子名称，覆盖带冒号的文件名。
新增 resources::vfs_source，默认 JSON 工厂捕获新 VFS 快照；原来源切换不改变在途加载。
普通 io.h 不再暴露旧文件系统与原始异步文件定义，旧消费者显式使用 io/legacy.h。

## 本机验证

Windows Clang Debug：资源/插件等选定测试 16/16，IO 1/1，相关示例运行成功。
安装及搬迁安装目录后的消费测试分别 6/6；Node Web 协议 1/1；真实 Chrome 155.0.8059.85 通过。
九个相关公共头独立编译通过；两个基准目标编译且来源快照基准烟测通过，不作为性能结论。
修改文件 clang-format 检查及文档链接检查通过。

本机日志：build/vfs-migration-acceptance-native.log、vfs-migration-acceptance-web.log、
vfs-migration-installed.log、vfs-migration-headers.log、vfs-migration-bench.log。

## 远端失败与修复

只读提交 fed639ad：Windows 39725319544 和 Emscripten 39725323198 成功；
Linux 39725315414 和 ASan 压力 39725327801 失败，报告宿主测试对象泄漏 1472 字节/19 个分配。
测试 backend 保留已经完成的 completion，形成凭据与 backend 自身的引用环。
本批清除完成后的测试凭据，并用 weak_ptr 断言卸载；宿主协议文档同步说明清理义务。
没有屏蔽泄漏检测。新提交必须重新验证受影响平台，本记录不把尚未完成的远端运行记为通过。

## 剩余范围

整体计划仍实施中。可写 open/流式写入、旧配置工厂退出与兼容层删除未完成；
保留旧接口直到替代验证，不能标记整体迁移完成。新 vfs_source 仅用于异步来源快照，
不支持旧同步 saver。原生 /dev/full 关闭失败用例仅在 Linux 执行，Windows 本机未验证该用例。
本批不修改公开标签，也不宣称发布完成。
