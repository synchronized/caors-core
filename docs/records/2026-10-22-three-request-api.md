<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 三类日常资源请求接口迁移记录

## 范围与环境

2026-10-22；`feature/resource-source-boundary` 分支，基于 `87e54a4c` 的本批代码工作树。
关联[API 收敛计划](../plans/io-assets-api-consolidation.md)、[PR #58](https://github.com/synchronized/caors-core/pull/58)。
本机 Windows x64 Clang 23.1.1 / MSVC ABI 共享 Debug；Emscripten 5.0.6 静态 Debug；Chrome 155.0.8059.85。

## 迁移范围

- manager/group 只提供 request_load<T>(guid)、request_reload<T>(guid)、request_preload(root) 三类日常请求。
- 原 refresh 并入 reload，每次独立捕获来源与启动，无缓存也可创建；最新重载拥有默认绑定发布资格，失败保留旧资源且不恢复旧意图。
- manager 不再提供逐次 worker/completion 参数，特殊调度经 create_group；模板 preload 仅保留在低层 database。
- database 删除 reload 专用请求表，统一加载表与重载发布选择；低层 request_reload_into 采用相同语义，删除 request_refresh_into。
- 无效 group 在启动前拒绝，因此没有新重载意图，不取消既有请求发布资格；有效请求启动后取消或投递失败则按最新意图屏障处理。
- 迁移示例、测试、安装 consumer、来源快照基准及当前参考。编译期断言检查已删除的高层入口不再可调用。

## 先前远端失败

`87e54a4c` 的 Web [39715044032](https://github.com/synchronized/caors-core/actions/runs/39715044032) 和 ASan 压力
[39715048028](https://github.com/synchronized/caors-core/actions/runs/39715048028) 通过。
Linux [39715036479](https://github.com/synchronized/caors-core/actions/runs/39715036479) 与 Windows
[39715040173](https://github.com/synchronized/caors-core/actions/runs/39715040173) 在来源快照基准编译时失败：
仍引用已删除的 async_loader 和 database.set_provider。本批改为统一 loader 和 snapshot_capture，
并将基准的模板 preload 迁到低层 database；没有绕过失败或将旧 CI 计为新代码通过。

## 本机验收

- 相关原生 CTest **16/16**，加载、重载、预加载、取消、失败保留、来源一致性及跨组调度测试通过。
- 独立重载与最新发布测试覆盖无缓存创建、多个重载独立结果、旧提交不得覆盖、最新失败/取消后旧意图不恢复。
- 安装原前缀及重定位消费测试各 **6/6**；通用基准及来源快照基准编译通过，后者以 3 样本/1000 绑定完成可运行性检查，不作为性能验收。
- Emscripten Node 协议 **1/1**；真实 Chrome 协议通过，保留默认 JSON 工厂及 reader 跨事件循环验证。
- 本机脚本及日志位于忽略的 `build/api-three-requests-*`，不是发布制品；当前 API 维护于[assets 参考](../reference/assets.md#三个日常请求入口)。

## 未完成范围

本批最终 SHA 的远端矩阵和压力验证尚待完成，阻塞合并验收；PR 继续保持草稿，失败不得跳过。
同步写入、配置读取、元数据、全局字节预算和自定义序列化 context 未纳入本批，未声明异步替代或性能验收。
未发布版本、改动公开标签或修改上游 Gneiss。
