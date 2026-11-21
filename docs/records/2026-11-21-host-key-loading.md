<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 宿主资源键加载与独立运行原型

## 范围

对应[宿主资源计划](../plans/host-driven-resource-loading.md)P1 及 P2 的运行原型，
分支 feature/resource-source-boundary，基于 0f7b272b。代码提交与平台结果在验收收尾时补充。
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
- 平台矩阵待本批验收，旧提交的绿色检查不能覆盖本批。

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

P3 动态依赖仍未实现，包括解析后子请求、共享等待图、并发环检测和父订阅取消传播。
当前 manager 调用与完成发布须统一串行化，loader 可跨线程交付完成；尚不提供可从任意线程
直接发起依赖请求的上下文服务。宿主循环、借用 loader、分配器及模块必须覆盖真实清理。
旧 GUID 外部引用协议不能表达宿主键，收集或强制解析场景明确返回 unsupported，不静默伪造 GUID。
旧 database、assets 工程装配及 IO 目录仍在，P4/P5 尚未开始，不能宣称计划整体完成。
