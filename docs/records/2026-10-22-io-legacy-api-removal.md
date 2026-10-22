<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# IO 重复公共请求入口删除记录

## 范围

基于 b89a6a42，分支 feature/resource-source-boundary，PR #58。
删除五个重复公共请求入口，不提供旧名别名；逐项替代以 [IO 迁移参考](../reference/io.md#已移除入口的迁移)为准。
原有有界读取、打开和关闭算法由 src/caors/io/file_requests.h 私有声明复用，命名移入 detail，
不安装给调用方。原公开的 detail/read_operation.h、detail/vfs_operations.h 也移入 src。
async_file 宿主协议不再因旧请求函数而包含路由快照；resources::byte_reader 自行显式包含其来源类型。

安装消费和普通分块示例迁到 io_context/file；捕获资源来源的调用用 request_bytes 组合读取与关闭，
不重新解析活跃路由。内部协议及插件保活回归直接验证私有实现，不把测试入口作为公共 API。

## 本机验证

Windows Clang Debug：相关资源等测试 16/16，IO 1/1，插件真实卸载/队列丢弃保活 1/1；
相关示例及新的分块示例运行成功。安装原位置及搬迁后消费各 6/6。
Node 协议 1/1，真实 Chrome 155.0.8059.85 通过；九个相关公共头独立编译通过。
两个基准目标编译及来源快照烟测通过，不作为性能验收。

源码/安装消费/Web 都编译负向约束：即使包含 legacy.h，也不能调用已删除的五个名字。
新安装包及搬迁包不含两个内部编排头；Windows DLL 导出检查不含旧公共函数符号。
本批是破坏性公共 API 清理，库、宿主和插件须按当前接口统一重编译。

日志：build/io-removal-native.log、io-removal-special.log、io-removal-installed.log、
io-removal-web.log、io-removal-browser.log、io-removal-headers.log、io-removal-bench.log。

## 平台验收

本批最终代码尚待 Linux、Windows、Emscripten 常规 CI，不以前批通过替代。
本批不改变在途计数、取消和关闭算法；完成本机插件专项并运行含 ASan/UBSan 的常规 CI，
不为仅接口删除重复另开 Linux Stress 矩阵。

## 保留范围

backend/async_file 的 start_* 是宿主实现协议，继续保留。
同步保存、旧配置及同步工具流仍有实际调用，未删除；高级逐块消费的进度、后端单次上限与迭代推进
尚未完整迁入 file，暂留独立兼容能力。它们的退出条件继续由 [VFS 计划](../plans/vfs-api-consolidation.md)维护。
本批不合并 main、不改已发布版本和标签，也不记为全部兼容层删除。
