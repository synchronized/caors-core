<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# VFS 路由独立实施记录

## 范围与环境

2026-10-22，分支 feature/resource-source-boundary，Windows Clang Debug 共享库、Emscripten 静态构建。
本记录对应[收敛计划](../plans/vfs-api-consolidation.md)的旧体系退出首批，不代表 file_system 已删除。

VFS 拥有不可变挂载表，快照直接共享固定表；解析结果持有 backend、拥有式路径与来源标识。
普通 open/read/write/stat/list 不再经过 file_system、旧 resolver 或动态类型转换。
更新串行发布，读取共享不可变表；旧表保留至更新锁外释放，避免宿主清理重入造成死锁。
来源标识序列提取为独立实现，新旧来源仍共享进程内唯一序列。

资源 source 尚依赖旧快照协议，其兼容桥移到 resources/vfs_source.cpp，不再由 VFS 提供旧类型。
新旧来源复用原打开/读取/关闭状态机，没有复制取消和清理逻辑。

## 本机验证

- build/vfs-final-native.ps1：16/16 资源、配置及序列化相关测试，IO 1/1，相关示例通过。
- build/io-removal-special.ps1：真实插件资源卸载 1/1，分块读取示例通过。
- build/io-removal-installed.ps1：原安装与迁移安装各 6/6。
- build/vfs-migration-headers.ps1：9 个相关公共头独立编译通过。
- Emscripten caors_web_protocol_test 与 caors_web_browser_smoke 构建通过，Node 协议 1/1。
- tests/web/browser_test.cjs：Chrome 155.0.8059.85 真实浏览器通过。
- 文档本地链接检查、clang-format 检查及 git diff --check 通过。

新增测试覆盖 scheme/authority、最长完整前缀、斜杠规范化、重复/空挂载、快照隔离、
原生并发更新以及后端先于宿主凭据释放、清理重入配置更新。Web 不包含原生线程并发用例。
首次新增路径测试错误假定 normalize_path 折叠点段；核对其契约后改为验证原有斜杠规范化语义，重跑通过。
本批不增加点段折叠或路径安全隔离承诺。

日志保存在本机 build/vfs-independent-{native,special,installed,headers,web,browser}.log，不作为公开制品。

## 剩余范围

平台 CI 待本批提交后运行，未完成前不声明跨平台验收通过。
资源快照、同步保存、配置工厂、工具及内置后端存储仍依赖旧体系，阻塞 file_system 整体删除。
PR 保持草稿，不合并、不发布；后续迁移完成后再整体验收。
