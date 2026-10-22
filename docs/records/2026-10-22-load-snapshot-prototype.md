<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 加载快照解耦原型验证

## 范围与环境

2026-10-22，feature/load-snapshot-prototype，基于 main 8dbe6f6c；
对应[API 收敛计划](../plans/io-assets-api-consolidation.md)的第一步代码原型。
Windows x64，Clang 23.1.1、MSVC ABI、共享 Debug；VsDevCmd 初始化，离线依赖缓存。

## 改动与结果

load_snapshot 仅提供多态析构；async_load_context 保存共享只读基类，snapshot_as<T>()
安全查询适配器的具体类型。provider_snapshot 继承该基类；请求内部参数支持基类。
资源 loader 协议头不再包含 source.h；async/database 仍为既有来源捕获显式依赖该头。
VFS 示例和当前参考已迁移；context.sources 删除属于源码与公共布局变更，使用方须统一重编译。

新增独立翻译单元检查 loader 头不能传递包含 source/file_system；编译通过。
新增运行测试验证宿主自定义快照经内部管线传递、外部引用释放后的保活、完成后的释放、
空快照及类型不匹配。保留既有重挂载、预加载共享来源与刷新测试，未修改捕获及请求合并算法。

```powershell
cmake --build build/v030-clang-debug --target caors_assets_asset_manager_test caors_assets_io_test caors_assets_integration_test caors_resources_async_test caors_resources_database_test caors_resources_pipeline_test caors_resources_provider_test caors_resources_reload_test caors_resources_coroutine_test -j 2
ctest --test-dir build/v030-clang-debug -R '^caors\.(assets\.(asset_manager|io|integration)|resources\.(async|database|pipeline|provider|reload|coroutine))$' --output-on-failure
```

上述相关 CTest 目标 9/9 通过。宿主异步示例输出 width=256；VFS 示例原生与 worker 两种路径均输出
hello VFS，重新构建和运行成功。宿主示例仍是模拟事件，不是真实 libuv 读取。

## 后续捕获策略原型

同一 feature 分支继续引入 snapshot_capture 和 database::set_snapshot_capture：
异步管线移除 source.h 与 provider 类型，默认 provider 捕获保留在 database 适配中。
自定义捕获器优先，空返回不回退，清空捕获器恢复默认适配；活动操作期间禁止更换。
边界翻译单元进一步改为包含 resources/async.h，验证整个异步管线头不包含 VFS。

新增公开 database 路径测试覆盖两种预加载闭包一次捕获、根/依赖共享、合并及缓存命中不重复捕获、
refresh 捕获新版本、活动期修改拒绝、显式空结果和恢复 provider 默认行为。
本次修改后重新构建并运行同样的 9 个相关 CTest 目标，9/9 通过；两条宿主/VFS 示例也重新构建运行通过。

## 限制与下一步

这是 feature 分支原型，不代表发布或整个资源核心已解耦。database 仍绑定 provider，
配置工厂和来源装配尚未迁出；宿主注入已可通过 database 捕获器配置，没有新增公共 manager 配置外观。
快照基类与 const 共享所有权不自动冻结派生对象引用的外部可变数据，也不自动 pin 动态模块。
类型查询依赖 RTTI，沿用项目已有 RTTI 使用，不承诺稳定跨编译器 C++ ABI。

本批未运行 Linux、Windows MSVC、Web、插件跨模块专项或完整矩阵。原型完成评审并准备合并前，
须执行受影响平台 CI 与必要生命周期专项；不得用本机 9 个目标替代这些门槛。
下一步继续将 database 的默认 provider 及工程装配职责迁往可选适配，并保持依赖批次语义。
