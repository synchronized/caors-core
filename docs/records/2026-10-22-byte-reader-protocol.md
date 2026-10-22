<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 异步字节读取协议实施记录

## 范围与环境

- 日期：2026-10-22；分支 `feature/resource-source-boundary`，基于 `114b51ca` 后的本批提交工作树。
- 关联[API 收敛计划](../plans/io-assets-api-consolidation.md)，PR [#58](https://github.com/synchronized/caors-core/pull/58)。
- Windows x64，Clang 23.1.1 / MSVC ABI，共享 Debug；Emscripten 5.0.6 静态 Debug；Chrome 155.0.8059.85。

## 已实施

新增 `resources::byte_reader`、`read_context` 和 `request_bytes`，复用 IO 完成凭证、请求句柄和诊断，
不另建一套完成状态机。`file_byte_reader` 封装打开、限量读取、关闭，明确选择原生或同步 worker 适配。
取消按阶段协作观察，不强制打断宿主读取；正常通知路径在交付前关闭，队列丢弃使用 RAII 兜底。

新增 `assets::json::byte_loader<T>`，借用 reader，来源取自请求的 provider 快照，读取后在 worker 解析。
简化 VFS 示例；安装 consumer 和真实浏览器接入新增 reader。
当前协议与接入维护于[资源参考](../reference/resources.md#统一字节读取适配)和[assets 参考](../reference/assets.md#json-异步字节接入)。

## 方法与结果

- 本机包装脚本 `build/api-loader-complete.ps1`：相关原生 CTest **16/16**，宿主异步、VFS 原生/worker、配置及资源管线示例退出码均为 0。
- 新测试覆盖快照来源切换、refresh 选择新来源、JSON 解析失败保留旧缓存、预算超限、取消、宿主延迟完成、拒绝保留原始诊断、重复完成、凭证销毁、完成队列丢弃、内联回调和后端释放。
- `cmake -P cmake/caors_verify_installed_package.cmake`：原安装前缀与重定位前缀 **6/6 各通过**，消费端实际调用新 reader。使用独立 `build/api-reader-installed-*` 目录。
- `caors_web_protocol_test`：新增 reader 用例并通过 Node CTest **1/1**。
- `node tests/web/browser_test.cjs build/emscripten-static-debug/tests/web`：真实 Chrome 通过；除了原迟到完成、取消、队列丢弃和句柄寿命，还覆盖 VFS 字节读取跨浏览器 tick 推进。
- 原始本机日志位于忽略的 `build/api-reader-native.log`、`api-reader-web.log`、`api-reader-browser.log`、`api-reader-installed.log`，不作为发布制品。

## 限制与后续

- 未执行本分支远端 Linux/Windows/Web 矩阵、sanitizer 和压力验证；合并验收仍受此阻塞，PR 保持草稿。
- 默认 JSON 配置工厂仍使用同步兼容 loader；本批新路径通过显式注册 byte_loader 接入。工厂切换和旧同步入口退出需后续迁移与验收，不记为完成。
- provider/source 现有同步读写未删除；写入、枚举和元数据尚未异步化。
- 新 JSON byte_loader 目前使用默认 serialization context，自定义 resolver 需自定义 loader。
- IO 中间结果只读，适配器交付时复制字节，峰值结果内存约两份预算；未做全局预算或性能验收。
- 本批不修改版本号、已发布标签和上游 Gneiss。
