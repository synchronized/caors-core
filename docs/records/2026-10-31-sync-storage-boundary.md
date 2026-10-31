<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 同步存储职责收缩

## 范围与提交

分支 `feature/resource-source-boundary`，代码提交 `af20a05dcea65dd5f6656e60bc6b6b2d1594ed56`，
并入现有 [PR #58](https://github.com/synchronized/caors-core/pull/58)。
本批不合并 main、不调整版本或创建标签；PR 实时状态以 GitHub 为准。
当前契约由[IO 后端参考](../reference/io-backends.md#复用同步存储)维护，
任务范围见[VFS 收敛计划](../plans/vfs-api-consolidation.md#同步存储职责收缩已完成)。

file_system 保留同步读写、流、属性、目录和随机访问；异步打开统一使用已有 backend。
旧路由的解析、冻结与快照隔离到 legacy::route_provider，不复制同步 IO 接口。
原生工厂复用通用 worker 适配，内存适配支持共享已有存储；没有新增存储或路径路由算法。
旧来源、测试和插件迁移到已有 backend 协议，删除重复旧异步桥。

## 本机验证

环境为 Windows、Clang Debug 动态库，构建目录 build/v030-clang-debug，加载 Visual Studio 开发环境，
Ninja 并发 2；格式与文档链接检查通过。

- IO 测试组及 16 组资产、资源、序列化回归全部通过，相关加载、配置、保存与服务示例通过。
- 新增同步头边界与旧成员缺失检查；共享内存和原生实例的双向读写、显式 worker 与无效配置回归通过。
- 15 个公共头独立编译通过，包括新诊断、同步随机句柄、同步存储和旧路由头。
- 插件 resource_lifetime 专项通过，覆盖跨 DLL 后端解析和模块寿命；分块示例成功消费 100000 字节。
- 原安装与重定位安装各 6 项 consumer 测试通过，新公共头随安装包完整交付。

## 平台验证

以下运行均固定上述代码 SHA，不能使用上一批结果代替：

| 平台 | 运行 | 状态 |
| --- | --- | --- |
| Linux 五组 | [41160115902](https://github.com/synchronized/caors-core/actions/runs/41160115902) | 全部通过：Clang 动态/静态、GCC、ASan、UBSan，含安装迁移及基准可运行性检查 |
| Windows 三组 | [41160115814](https://github.com/synchronized/caors-core/actions/runs/41160115814) | 全部通过：Clang 动态/静态、MSVC，含安装迁移与 MSVC 基准可运行性检查 |
| Emscripten | [41160115843](https://github.com/synchronized/caors-core/actions/runs/41160115843) | 全部通过：静态库、公共头、Node、安装迁移及真实浏览器生命周期 |

三类平台验收全部完成；UBSan 完整日志确认 100 个测试组通过，未报告运行时错误。
最终收尾只修改文档，已核对 include、src、tests、examples、benchmarks 与被验证代码提交一致。
基准检查仅确认可运行性，不作为性能验收或性能改进结论。

## 兼容影响与边界

移除旧公共虚函数及调整类布局属于破坏性接口变更；库、宿主和插件须统一重编译。
file_source 的同步读写保留；无异步能力的同步磁盘来源在异步解析时返回 unsupported，
需显式 worker 适配后接入 file_service，不隐式阻塞读取。
共享内存适配器只串行化自身调用；直接同步访问及多个适配器之间的访问仍须宿主协调。
旧配置及工具路由保留，未宣称整个兼容体系已删除；外部 Gneiss 工程未在本批修改或联调。
