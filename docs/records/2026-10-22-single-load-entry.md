<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 统一资源请求入口收口记录

## 范围与环境

2026-10-22，分支 `feature/resource-source-boundary`，基于 `ef363349` 后的本批代码。
关联[API 收敛计划](../plans/io-assets-api-consolidation.md)、[PR #58](https://github.com/synchronized/caors-core/pull/58)。
本机为 Windows x64 Clang 23.1.1 / MSVC ABI 共享 Debug；Emscripten 5.0.6 静态 Debug；Chrome 155.0.8059.85。

## 完成范围

- 删除 asset_manager/database 的同步 load、load_handle、reload、reload_handle、preload 执行入口，日常加载只保留 request_*。
- find、resolve、plan_preload 等不执行 I/O 的同步查询保留；低层同步组合只接受 sync_loader_adapter，不能误传原生异步 loader。
- database 和手动重建绑定删除同步/原生异步双槽与能力优先选择，收敛到同一 loader 指针。重复注册同对象幂等，不同对象返回 already_exists。
- 删除同步类型分派函数表，预加载规划检查统一 request_load 能力。保留类型别名和稳定 type_guid 分派。
- 迁移剩余 manager/database 调用测试；保留低层显式同步适配测试、序列化与写入能力。

当前行为维护于[资源参考](../reference/resources.md)和[assets 参考](../reference/assets.md)。

## 本机验证

- `build/api-json-default-complete.ps1`：原生相关 CTest **16/16**，加载、重载、预加载、来源一致性、队列丢弃和多继承 loader 指针等回归通过；相关 alpha/beta 示例退出码均为 0。
- `build/api-single-load-extra.ps1`：补充编译期 API 边界检查并复验 asset_manager **1/1**；基准目标编译通过，未执行性能测量。
- 安装与重定位前缀各 **6/6**，使用 `cmake/caors_verify_installed_package.cmake` 与独立 `build/api-single-load-installed-*` 目录。
- Emscripten Node 协议 CTest **1/1**；真实 Chrome 迟到完成、取消、丢弃、字节读取和默认 JSON 工厂跨 tick 加载通过。
- 本机日志在忽略的 `build/api-single-load-{native,extra,installed,web,browser}.log`。

## 远端验收与剩余范围

本分支远端平台矩阵、sanitizer 和压力验收仍待完成；不能把此前批次的 CI 结果当成本批验证，PR 保持草稿。
计划在本批代码提交后发起一次受影响平台与压力工作流，失败不绕过。

provider/source 的同步读写、配置读取、枚举和元数据未异步替换；这是保留的独立能力，并非已验证的异步接口。
自定义序列化 context、全局字节预算与读取中间复制仍保留此前记录的限制。本批未发布版本、改标签或修改上游 Gneiss。
