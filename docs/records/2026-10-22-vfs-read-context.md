<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# VFS 统一只读入口实施记录

## 范围与环境

2026-10-22，`feature/resource-source-boundary`，基于 `88f8a531` 的本批代码工作树。
关联[VFS 收敛计划](../plans/vfs-api-consolidation.md)和 [PR #58](https://github.com/synchronized/caors-core/pull/58)。
Windows x64 Clang 23.1.1 / MSVC ABI 共享 Debug；Emscripten 5.0.6 静态 Debug；Chrome 155.0.8059.85。

## 实施范围

- 新增独立 backend 与 vfs 配置，不让路由继承文件后端；URI/虚拟路径算法继续复用旧 resolver 的私有传输适配。
- io_context 初始化完成调度和 allocator，open 默认只读已有文件，当前不暴露未实现的写入枚举。
- file 提供范围 read 与显式 close；上下文和文件操作返回单层 request，发起拒绝也经完成调度或句柄查询。
- native 后端构造绑定 worker，memory 后端支持内存数据 store；文件及快照持有后端和模块 lifetime。
- 资源 byte_reader 与新 io_context 复用 IO 内部的整体读取编排，未复制第二个打开/读取/关闭状态机。
- 添加 vfs_basic 示例、安装消费、真实浏览器及新 API 边界测试。
- vfs_snapshot 不暴露旧 resolve_owned 等路由协议；活跃上下文不能借用临时 vfs。

当前用法见[IO 参考](../reference/io.md#vfs-与统一文件访问)，不在记录复制声明。

## 本机验证

- `build/vfs-final-native.ps1` / `vfs-acceptance-native.log`：资源与 assets 相关 CTest **16/16**，IO **1/1**；旧示例与新 vfs_basic 示例均退出 0。
- `build/vfs-library-final.ps1`：最终库编译及 IO 回归 **1/1**。
- 新用例覆盖每次发起固定路由、显式批次越过配置寿命、范围预算、关闭共享状态、后端拒绝保留诊断、错误单次通知、取消转发、完成队列丢弃、打开文件越过卸载仍保留模块凭据。
- 新 API 头与旧 file_system/resolver 包含隔离、临时 vfs 借用拒绝使用编译期断言验证；六个新公开头各自独立语法编译通过。
- 安装原前缀与重定位各 **6/6**；消费端实际调用 vfs/io_context.read，日志 `build/vfs-installed-final.log`。
- Emscripten Node 协议 **1/1**，包含可移植的新 VFS 用例；最终日志 `build/vfs-acceptance-web.log`。
- 真实 Chrome 通过原生命周期测试及新增 io_context 跨 tick 读取，日志 `build/vfs-browser-final.log`。

上述 build 日志属于本机忽略目录，不作为发布制品；不是性能验收。

## 未完成范围

首批只读主线已实现，本批最终 SHA 的远端矩阵、sanitizer、压力验证尚待完成，仍阻塞合并验收。
VFS 整体计划仍实施中：旧配置工厂和嵌套路由接入、写入打开模式、目录/元数据异步替代、旧公共接口退出及目录清理未完成。
旧写入/配置能力保留，不能把它们标记为已异步化；需要在相应批次实现、迁移并验收后退出。

结果缓冲仍可能复制，单次预算不等于全局在途预算；allocator/宿主 loop 仍须覆盖实际清理。
本批不发布版本、不改已有标签、不修改上游 Gneiss。

后续整体写入、元数据和兼容消费迁移见[后续实施记录](2026-10-22-vfs-operations-migration.md)。
