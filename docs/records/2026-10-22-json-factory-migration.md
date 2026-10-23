<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 默认 JSON 工厂异步迁移记录

## 范围与环境

2026-10-22，`feature/resource-source-boundary` 分支，以 `c578d041` 为基线的本批提交工作树。
关联[API 收敛计划](../plans/io-assets-api-consolidation.md)、[PR #58](https://github.com/synchronized/caors-core/pull/58)。
Windows x64 Clang 23.1.1 / MSVC ABI，共享 Debug；Emscripten 5.0.6 静态 Debug；Chrome 155.0.8059.85。

## 迁移内容

- `json::loader<T>` 统一为请求快照 + byte_reader 的异步实现，允许借用外部 reader 或拥有内部 file_byte_reader。
- 原同步实现更名 `json::sync_loader<T>`；短期 byte_loader 名称退出当前 API，不保留另一套异步类。
- 默认 JSON 工厂直接创建异步 loader 和 cache，默认 native_async；注册时可显式选择 worker_adapter，遇到 unsupported 不隐式回退。
- 工程工厂测试、alpha 依赖管线、beta 加载与重载示例迁到 request 入口；`assets_sync_load.cpp` 更名 `assets_load.cpp`，构建目标同步更名。
- 外部文件系统工厂测试从借用指针绑定迁到共享所有权绑定，使其可以捕获拥有式快照；同步序列化、资源核心及基准用例明确使用 sync_loader。

当前接口与迁移步骤维护于[assets 参考](../reference/assets.md#json-异步字节接入)，不由本记录维护第二套 API。

## 验证

- `build/api-json-default-complete.ps1`：原生相关 CTest **16/16**，包括默认工厂来源冻结、在途 rebuild 拒绝、refresh 切换来源及 native 文件必须显式选择 worker 适配。
- 实际运行加载队列、线程池加载、重载、宿主异步、VFS 原生/worker、配置 schema、alpha/beta 管线示例，退出码均为 0。
- `cmake -P cmake/caors_verify_installed_package.cmake`：安装与重定位各 **6/6**，消费端实例化默认 JSON 工厂模板，并继续调用统一 reader。
- Emscripten 构建后 Node 协议 CTest **1/1**；新增默认 JSON 工厂经配置队列加载的用例。
- 真实 Chrome 浏览器协议通过，新增默认 JSON 工厂跨 tick 完成；保留原有迟到完成、取消、丢弃和寿命检查。
- `caors_benchmarks` 目标编译通过，补迁移其同步自定义 loader；仅检查源码兼容性，未运行性能测量。
- 文档 117 文件、596 个本地链接检查、clang-format 和 git diff --check 通过。
- 原始日志在忽略的 `build/api-json-default-final.log`、`api-json-default-web.log`、`api-json-default-installed.log`、`api-json-default-browser.log` 及 `api-json-default-benchmark.log`。

## 剩余范围

资源核心和 manager 的旧同步 load/reload/preload 入口尚未删除，仍只服务同步 adapter 或缓存命中；
不通过阻塞等待调用异步 loader。需要继续迁移剩余兼容用例后决定退出范围，本批不宣布整个收敛完成。
provider/source 同步读写、配置文件读取、枚举及元数据未作异步替换，已有写入能力保留。
自定义 serialization context 支持和全局字节预算也未纳入本批。

本分支远端矩阵、sanitizer 和压力验收尚未执行，仍阻塞合并验收；PR 保持草稿。
未修改版本号、已发布标签、上游 Gneiss；没有性能结果或稳定 C++ ABI 承诺。
