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

## 工程装配入口迁移

同分支后续将 build/build_detailed、配置路由图校验和失败结果组装移至 project_builder.cpp，
manager 删除这两个公共入口，测试和 alpha/beta 示例迁移至 project_builder{manager}。
project_builder 非拥有，配置对象继续由 manager 保活；builder 析构不影响已装配环境。
各阶段创建函数、注册表、工厂及对象所有权尚在 manager，这是独立编排入口的第一步，未完成整个装配拆分。
失败语义沿用原逐阶段构建，不增加事务回滚保证。

迁移后重新构建相关资源目标，CTest 9/9 通过；宿主加载及 VFS 两种读取示例通过。
另外构建运行 beta/config_schema、beta/asset_pipeline、alpha/alpha_pipeline：
配置诊断示例按预期阻止无效 options 进入 factory，两个工程管线示例完成加载、依赖处理与保存，退出码均为 0。
这些是本机原生验证，未替代 Web、MSVC 或 Linux 门槛。

## 工程状态封装

注册表、工厂、配置拥有对象表、手工类型绑定表及构建标记集中到私有 project_state，
定义在 asset_manager.cpp；公共头仅保留不完整类型和带内存资源的专用 deleter。
状态通过 manager 的 memory_resource 分配释放，生命周期仍由 manager 持有；
析构时保持清空 database/provider、来源/注册表，再销毁拥有对象的既有顺序。
本步不是把注册 API 或整个状态所有权转交给 builder；相关公共接口仍待迁移。

create_owned 先用 RAII 持有新对象，登记到拥有表成功后才放弃临时所有权。
新增定向失败测试：对象构造后拒绝下一次表扩容分配，确认抛出 bad_alloc、对象析构且内存回到登记前，
manager 析构后自定义资源的在用字节归零。状态封装及头文件隐藏后重新构建，相关 CTest 9/9、\n宿主/VFS 与三个工程示例全部通过；未执行完整跨平台矩阵。

## 公开注册入口迁移

文件系统/source 绑定、查询、清空、计数，三类配置工厂及强类型注册，create_owned 已由 project_builder
公开；manager 的同名实现及工厂类型转为私有，工厂扩展类型通过 project_builder 别名公开。
工厂上下文提供 context.project()，JSON/通用类型工厂 helper 改接收 builder；现有测试和示例同步迁移。
新增编译期检查限制 manager 不公开文件绑定/对象创建，而 builder 提供这些入口。
具体容器和各阶段实现仍保留私有工程状态，builder 借用 manager，不改变创建对象的生命周期。
迁移公开入口并新增边界检查后重新构建，原生相关 CTest 9/9，宿主/VFS、配置诊断与 alpha/beta 工程示例通过。

## 安装消费端收口与平台验证

81c26ba0 的消费端审计发现 resources_coroutine.cpp 仍使用 context->sources，先前点号搜索遗漏了此指针路径。
迁移为 snapshot_as<provider_snapshot>()，并补安装包侧 builder 的工程构建、拥有对象创建与后端绑定调用。
本机使用新建的 build/api-installed-* 输出目录执行 caors_verify_installed_package.cmake，
Windows x64 Clang 共享 Debug 六类 consumer 在原安装前缀 6/6、迁移前缀 6/6 通过。

该遗漏发现后主动停止旧 SHA 的 Linux、Windows、Web 和 Stress 运行，四轮均 cancelled，不作为通过证据：
[Linux](https://github.com/synchronized/caors-core/actions/runs/39704210730)、
[Windows](https://github.com/synchronized/caors-core/actions/runs/39704214742)、
[Web](https://github.com/synchronized/caors-core/actions/runs/39704219255)、
[Stress](https://github.com/synchronized/caors-core/actions/runs/39704257535)。
修正后固定 `9fb1c5879c87d5e6a16e400a0f3ac4c695e2179e` 执行以下验证；已取消运行不作为验收。

| 最终代码提交验证 | 结果 |
| --- | --- |
| [Linux5](https://github.com/synchronized/caors-core/actions/runs/39704555744) | 5/5 成功；动态/ASan/UBSan 各 100/100，静态 97/97；普通 Release 安装与迁移 consumer 各 6/6 |
| [Windows3](https://github.com/synchronized/caors-core/actions/runs/39704559523) | Clang/MSVC 动态各 100/100，Clang 静态 97/97；每组原始与迁移 consumer 各 6/6 |
| [Web](https://github.com/synchronized/caors-core/actions/runs/39704563053) | 完整库/公共头/Node CTest 2/2；安装与迁移各 1/1；真实浏览器迟到完成、取消、队列丢弃、句柄寿命通过 |
| [Linux ASan Stress](https://github.com/synchronized/caors-core/actions/runs/39704567540) | 插件卸载 4 项重复 20 次、内存/MessagePack 9 项重复 10 次、VFS/异步加载 4 项重复 20 次，全部通过 |

逐一核对四个运行 headSha 均为上述提交，全部 job 成功。日志未发现编译 warning/error；来源快照基准 smoke 仅验证可运行，不作为性能改善证据。
本记录及索引后续回写仅修改文档，不改变已验证代码。集成入口为 [PR #57](https://github.com/synchronized/caors-core/pull/57)。

## 限制与下一步

首批代码验收已完成，不代表版本发布或整个资源核心已解耦。database 仍绑定 provider，
配置工厂公开入口和装配编排已迁移至 project_builder；状态及私有执行函数仍由 manager 持有，
宿主注入可通过 database 捕获器配置。
快照基类与 const 共享所有权不自动冻结派生对象引用的外部可变数据，也不自动 pin 动态模块。
类型查询依赖 RTTI，沿用项目已有 RTTI 使用，不承诺稳定跨编译器 C++ ABI。

公开注册入口迁移后，本地 Emscripten 5.0.6 wasm32 静态单线程完整核心与协议目标重新构建通过；
ctest --test-dir build/emscripten-static-debug -R caors.web.protocol --output-on-failure：Node 协议目标 1/1 通过。

上述 CI 已补齐 Linux/Windows 常规矩阵、真实浏览器、安装迁移和既有插件生命周期压力验证。
尚不代表任意宿主自定义快照的跨编译器 ABI、完整无异常运行、Gneiss 联调或生产性能已经验收。
下一步继续审查 provider 的公共归属及装配状态与 manager 的持有边界，保持依赖批次与清理顺序。
