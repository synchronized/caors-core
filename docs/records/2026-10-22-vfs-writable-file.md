<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# VFS 可写文件句柄实施记录

## 范围

承接 [VFS 计划](../plans/vfs-api-consolidation.md)，基于 a8de4509，仍使用 feature/resource-source-boundary / PR #58。
统一 open 增加 write/read_write 权限与 create_always，默认 read/open_existing 不变；
file.write 使用显式 offset 和拥有式缓冲，返回单层 request<uint64_t>。
当前契约与后端限制以 [IO 参考](../reference/io.md#vfs-与统一文件访问)为准。

复用 worker 文件的在途计数与关闭流程，累计短写，清理任务丢弃也释放计数；
关闭拒绝新操作并等待已接受读写，取消不回滚副作用。旧宿主默认不支持写入，不作同步回退。
内存写句柄关闭时发布修改，未写入的已有文件句柄不覆盖外部更新；原生后端串行随机读写。

## 本机验证

Windows Clang Debug：选定资源/插件等 16/16、IO 1/1，相关示例通过；
补充边界用例并调整测试分类后 IO 1/1 再验收通过。
覆盖默认不创建/截断、显式截断、精确范围写后读、权限（含空范围）、预算、偏移溢出、
空写不扩展、未修改句柄不覆盖外部更新、短写累计、取消和队列丢弃时的关闭。

Node 协议 1/1，Chrome 155.0.8059.85 的独立事件循环打开/写入/读回/关闭通过。
安装包原位置与搬迁消费分别 6/6，通过新增可写句柄消费；九个相关公共头独立编译通过。
两个基准目标编译及来源快照烟测通过，不代表性能验收。

日志：build/vfs-writable-final-native.log、vfs-writable-edge-native.log、vfs-writable-final-web.log、
vfs-writable-edge-web.log、vfs-writable-browser.log、vfs-writable-installed.log、vfs-writable-headers.log、vfs-writable-bench.log。
a04e2eff 已启动远端验收；前批 baf593ad 的成功不能替代本批可写协议验收。

后续仅迁移工程配置消费示例与测试，不修改库实现：手工 bind_source 注册命名 vfs_source，
project_resource_config 指向来源及逻辑路径，config.file_systems 留空。
Windows asset_manager 1/1 与示例通过，Node 1/1 通过，日志为 build/vfs-config-consumer.log、vfs-config-web.log。

## 保留与限制

未实现 append、独占创建、原子替换、持久化保证或冲突检测；每次预算不是文件总长/全局在途限制。
旧配置工厂、同步 saver 与工具流仍保留兼容；删除前必须有保存和配置迁移替代，不能宣称整体兼容层已退出。
不修改已发布版本与标签，不代表 Gneiss 宿主已经联调通过。
