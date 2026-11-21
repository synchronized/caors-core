<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 宿主资源键加载与独立运行原型

## 范围

对应[宿主资源计划](../plans/host-driven-resource-loading.md)P1 及 P2 的运行原型，
分支 feature/resource-source-boundary，基于 0f7b272b，功能提交 f015ac90cd6c7467c6d18b74c75cfa44f8eaccb2，
独立探针 ASan 配置修复为 058955fe。当前契约保持实验性，PR #58 为开发草稿。
当前 API 由[资源参考](../reference/resources.md#宿主资源键加载)维护，不复制完整声明。

缓存、引用、loader、加载上下文和请求表支持宿主 Key，路径键不转换为 GUID，普通对象不保存内嵌身份。
原 GUID 派生资源通过受约束兼容特化继续工作；后续调用迁移前不删除这部分行为。
加载、订阅、取消、重载仍复用原请求状态机，类型存储通过新的 resource_manager 组合，
不创建项目状态、provider、文件来源或线程池，不引入第二套异步完成协议。
宿主 Hash/Equal 状态同步复制到缓存、发布凭据表和请求表；持久引用只保存键。
独立缓存身份弱凭据与代次共同校验引用，避免同对象跨缓存误认绑定。

## 本机证据

Windows、Clang 23.1.1、Debug 动态库、Ninja 并发 2：

- handle/cache/async/serialization.resource 四组专项通过，包含分配失败注入及解析限制检查。
- IO 及 16 组资产、资源、序列化回归通过，加载、配置、保存及文件服务示例通过。
- 无 IO 的独立编译、链接及运行原型通过；源码冻结复验通过。
- 插件 resource_lifetime 专项、分块示例、原安装与重定位安装各 6 项消费测试通过。
- 安装消费新增普通对象的宿主路径键管理器、引用解析和缓存归属验证。
- 本批平台结果及 ASan 修复专项见下表，旧提交的绿色检查不计入本批证据。

## 平台证据与 ASan 配置修复

| 检查 | 提交与运行 | 结果 |
| --- | --- | --- |
| Linux Clang 动态/静态、GCC、UBSan | f015ac90，[44904717075](https://github.com/synchronized/caors-core/actions/runs/44904717075) | 四组通过；该工作流整体因下述 ASan 覆盖检查失败而为失败状态 |
| Windows Clang 动态/静态、MSVC | f015ac90，[44904717106](https://github.com/synchronized/caors-core/actions/runs/44904717106) | 三组全部通过 |
| Emscripten | f015ac90，[44904717288](https://github.com/synchronized/caors-core/actions/runs/44904717288) | 通过，含安装消费及浏览器检查 |
| Linux ASan 修复后专项 | 3a25c0db，[44905558958](https://github.com/synchronized/caors-core/actions/runs/44905558958) | 通过，包含覆盖检查及运行验证 |

首次 ASan 在编译前的覆盖检查失败：独立目标没有链接单体 caors，也没有继承其 sanitizer 使用要求。
058955fe 对对象探针、支持实现和运行程序显式设置 ASan 编译及链接参数，没有绕过覆盖检查。
本机 Windows ASan 独立运行通过，已核对 8 条编译命令与最终链接命令的 sanitizer 参数；
本机结果不代替 Linux 的泄漏检查。

为避免重复其他四组矩阵，chore/resource-core-asan-validation 临时分支仅保留 ASan 矩阵项。
3a25c0db 与 058955fe 的差异只有该工作流删去其他四组任务，已核对 include/src/tests/examples/benchmarks 完全一致。
058955fe 相比 f015ac90 只增加独立探针的条件 ASan 配置；非 ASan 平台结果仍明确关联原功能提交。

新增场景覆盖拥有式路径、GUID、组合键、哈希碰撞、可配置相等策略、跨类型隔离、
引用替换和清空后的代次失效、跨缓存身份隔离、键持久化与未解析读回。
网络由可控延迟 loader 模拟；两个订阅只启动一次实际操作，取消一个订阅不影响另一个。
重载乱序完成只发布最新结果，失败保留旧缓存，旧句柄继续有效。
管理器在 worker 启动前或宿主交付前销毁均保活类型存储，结果及控制块清理后分配器计数归零。

## 独立原型复现

在初始化的开发环境中执行：

```sh
cmake -S tests/resource_boundary -B build/resource-boundary -G Ninja
cmake --build build/resource-boundary
ctest --test-dir build/resource-boundary --output-on-failure
```

该工程明确只编译 memory/resource、memory/detail/internal、tasks/executor、
tasks/execution_context、core/string_name 和 core/guid 六个基础实现，
不链接 caors 单体，不编译 IO、assets 或旧 resources 来源实现。
运行目标同样优先搜索禁止 IO 的头目录，验证宿主路径加载、共享取消、缓存、重载、引用及管理器寿命。
正式资源模块的目标拆分、导出及独立安装尚未完成；不能把测试支持库当作正式模块交付。

## 剩余工作

2026-11-22 插件边界补查：新增 platform/shared_library.cpp 的禁止 IO 编译目标，
并确认插件清单使用的 serialization/operations.h 仍命中禁止 IO 诊断。
Windows Clang 独立工程重新配置、构建及 caors.resource_boundary.runtime 通过。
这只证明平台加载实现的编译边界；本批没有更改插件运行行为，也未新增插件运行或平台矩阵验收。

P3 动态依赖仍未实现，包括解析后子请求、共享等待图、并发环检测和父订阅取消传播。
当前 manager 调用与完成发布须统一串行化，loader 可跨线程交付完成；尚不提供可从任意线程
直接发起依赖请求的上下文服务。宿主循环、借用 loader、分配器及模块必须覆盖真实清理。
旧 GUID 外部引用协议不能表达宿主键，收集或强制解析场景明确返回 unsupported，不静默伪造 GUID。
旧 database、assets 工程装配及 IO 目录仍在，P4/P5 尚未开始，不能宣称计划整体完成。
