<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 资源后端快照迁移记录

## 范围与环境

2026-10-23，feature/resource-source-boundary，PR #58。
被验证代码提交为 20a1a68cfeb7d8d407aadcb16afe97c8df93d82c；后续文档补充不改实现。
环境为 Windows Clang Debug 共享库、Emscripten 静态构建与 Chrome 155.0.8059.85。
范围见[VFS 计划](../plans/vfs-api-consolidation.md)，当前 API 由[资源参考](../reference/resources.md#加载来源快照)维护。

provider_snapshot 在捕获时保存 backend_location，逐 source 捕获一次、逐 GUID 解析位置，
后续查询不再访问解析函数。source::capture 返回拥有式函数，没有新增快照继承层。
vfs_source 删除两个 file_system 传输类，直接捕获新 VFS；兼容 file_source 保留单向后端适配。
byte_reader 和整体读取编排仅接收 backend_location，JSON 加载层不再选择 file_read_mode。
同步配置、保存、文件工具以及本地/内存实际读写实现保留。

## 本机验证

- build/vfs-final-native.ps1：16/16 资源、配置及序列化相关回归，IO 1/1，相关示例通过。
- build/io-removal-special.ps1：插件实际卸载 1/1，分块示例通过。
- build/io-removal-installed.ps1：原安装与迁移安装各 6/6；安装消费者使用新 VFS 并检查后端身份。
- build/vfs-migration-headers.ps1：9 个相关公共头独立编译通过。
- Emscripten 构建 caors_web_protocol_test/caors_web_browser_smoke，Node 协议 1/1。
- tests/web/browser_test.cjs：真实浏览器延迟完成、取消、队列丢弃和生命周期通过；JSON 路径已改新 VFS 来源。
- build/api-three-requests-benchmark.ps1：两个基准目标编译通过，来源快照基准 3 次、1000 GUID 可运行。
- 文档本地链接、clang-format 与 git diff --check 通过。

新增专项检查精确后端身份、两个 GUID 的来源标识一致、配置销毁后的后端与宿主凭据保活、
每批只捕获一次且每位置只解析一次、逐 GUID 错误隔离和畸形来源失败。
旧来源拒绝、空成功诊断、借用叶拒绝和分配失败回归继续保留；原生 JSON 验证后端绑定 worker，
同时验证不隐式退回旧同步来源。
源码边界检查确保 source/vfs_source/byte_reader 不带入旧 file_system 或 route_snapshot 定义。

开发中暴露的旧测试与浏览器消费直接构造 resolved_file 的编译错误已迁移并重跑通过；
不通过恢复旧类型别名或新增同步回退修补。
日志位于本机 build/resource-backend-{native,special,installed,headers,web,browser,bench}.log。

## 限制与剩余范围

本批平台 CI 在代码提交 20a1a68cfeb7d8d407aadcb16afe97c8df93d82c 上全部成功：

- [Linux](https://github.com/synchronized/caors-core/actions/runs/39898207752)：Clang 动态/静态、GCC、ASan、UBSan 五组成功。
- [Windows](https://github.com/synchronized/caors-core/actions/runs/39898211890)：Clang 动态/静态、MSVC 三组成功。
- [Emscripten](https://github.com/synchronized/caors-core/actions/runs/39898217145)：公共头、Node 与真实浏览器成功。

此结果只验收资源后端快照批次；未跳过失败检查，不代表整个 IO 整理完成。
旧配置和工具路由仍存在；file_source 仍为旧文件来源保留适配，不宣称整体 IO 收敛完成。
捕获工作由查询阶段移至批次建立阶段；旧来源适配会产生额外后端包装，基准仅证明可运行，
未进行固定环境性能对照，不声明性能提升。
不以删除 file_system 为验收条件，不修改已发布标签，不合并或发布本批草稿。
