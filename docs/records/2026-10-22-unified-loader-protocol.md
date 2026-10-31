<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 统一 loader 协议验证

## 范围与环境

2026-10-22，feature/resource-source-boundary，在 b217ff33 的来源边界改造之上继续推进。
关联[API 收敛计划](../plans/io-assets-api-consolidation.md)，本批尚未合并或发布。
原生为 Windows x64、Clang 23.1.1、MSVC ABI、共享 Debug；Web 为 Emscripten 5.0.6 wasm32 静态单线程。

## 实现与迁移

loader<T> 仅提供纯虚 start(context, completion)，不再要求实现同步 load；删除独立 async_loader 类型。
原生实现改为直接继承 loader。既有同步实现改继承 sync_loader_adapter，覆盖 load 钩子；
适配器 final start 在请求管线指定的 worker 执行钩子并用相同完成凭证交付，不创建线程、不等待异步操作。

旧同步辅助函数暂保留：缓存未命中时检查显式适配能力，纯异步实现返回 unsupported，不阻塞 loop。
同步适配和原生实现并存的注册槽暂为旧接口迁移保留，异步请求仍优先原生实现，不因失败重试另一来源。
类型擦除注册存储统一 loader 基类指针，手工绑定重建时再按同步适配类型正确转换，避免多继承偏移错误。
JSON loader、示例、测试、安装 consumer 及 Web 浏览器 smoke 均已迁移。

## 验证结果

原生构建并运行 assets 三个目标、resources 十二个目标和 serialization.api，CTest 16/16 通过。
包含原生延迟完成/拒绝/取消、同步进度、JSON 序列化、重载及来源批次等既有回归。
新增多继承适配器测试覆盖类型擦除指针转换、工程重建手工绑定、worker 执行和 completion 提交，
以及旧同步辅助入口；编译期检查 loader 不提供 load，而显式适配器提供该钩子。

宿主/VFS、配置诊断及 alpha/beta 工程管线示例重新构建运行通过。
安装验证使用新建 build/api-loader-installed-* 输出目录，原始及迁移前缀六类 consumer 各 6/6 通过。
Web 完整核心、协议及浏览器目标重建，Node 协议 CTest 1/1 通过。
使用本机 Chrome 155.0.8059.85 运行 browser_test.cjs，迟到完成、取消、队列丢弃、句柄寿命全部通过。
相关本机日志为 build/api-loader-complete.log、api-loader-installed.log、api-loader-web.log、api-loader-browser.log，均为忽略的执行输出。

## 未完成边界

本批尚未执行远端 Linux/Windows 常规矩阵和压力验证，合并前补齐，不沿用第一批或来源边界的旧结果。
provider 的定位/快照仍同步，字节读写也仍为既有同步适配；其定位和字节访问拆分未实施。
旧同步资源入口与兼容注册槽尚未删除；本批统一的是 loader 交付协议，不代表所有资源/文件 API 均已异步化。
saver、写入提交语义与独立文件 API 整理仍待后续，不删除已有功能；无稳定 C++ ABI 或性能保证。
