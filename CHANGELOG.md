<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 变更记录

本文件记录面向使用者的变化。未发布内容进入 Unreleased，发布准备时移入带版本和日期的章节。
本次建立变更记录，不追补未经核实的历史版本；工程版本不等同于已经发布。
准备与发布步骤见[发布流程](docs/guides/release.md)。

## Unreleased

暂无新增变更。

## v0.4.0 — 2026-10-20（alpha，已发布）

- resolver/mount 新增拥有式挂载、不可变并发路由及嵌套快照，旧快照在 unmount/remount 后保留来源；原同步借用接口保留。
- 新增拥有式异步文件、精确范围读取、明确预算、取消/完成通知/关闭协议；原生可选能力默认 unsupported，显式 worker 适配无整文件回退，不引入 libuv 依赖。
- provider/source 向使用 `context.sources` 的 loader 传递同次加载来源快照，依赖预加载共用来源，同 GUID 继续复用首次操作；配置工厂提供拥有式后端和循环检查。
- 新增异步 VFS 端到端示例；公共虚表与状态布局变化，升级须统一重编译库、宿主与插件。验收与发布状态见 [v0.4.0](docs/versions/v0.4.0.md)。

## v0.3.0 — 2026-10-19（alpha，已发布）

- 新增 async_loader、拥有型进度与 move-only 完成凭证，接入现有共享请求、独立取消和协程；支持宿主延迟 I/O，不引入 libuv 依赖。
- 原同步 loader 源码保持兼容；虚函数表及相关状态布局变化，升级时须同时重编译库、宿主与插件，不能混用旧二进制。
- asset_manager 可初始化默认 worker/completion，asset_group 复用特殊调度；实际操作清理前拒绝 runtime 重配，旧 group 在重配后失效。

- CMake preset 默认动态库；静态库使用 `*-static-debug/release`，旧 `*-shared-*` 名称移除，原无前缀静态调用需迁移。
- 常规 CI 精简为 Linux 5 组、Windows 3 组，补充 Windows workflow；Debug 等组合保留按需验证。

## v0.2.0 — 2026-10-19（候选，尚未发布）

### 修复

- 修复目标线程提前完成调度时，协程可能在调用线程继续执行的竞态，保证成功切换的恢复线程。

### 调度后端

- 删除 Asio 依赖；内置线程池使用标准库线程和条件变量，execution_context 使用调用线程驱动的任务队列。
- 协程入口从 `caors/tasks/asio.h` 改名为 `caors/tasks/coroutine.h`，调用方需更新 include。

### 宿主集成

- 新增外部 executor 拥有型适配；宿主关闭并丢弃队列任务时，关联请求进入明确失败终态。
- 同一资源的独立加载/重载调用共享底层工作，取消订阅互不影响；需要旧的全局取消行为时改用 `cancel_shared()`。
- 增加 runtime 身份、跨模块创建/释放、显式模块 pin 与 allocator/弱引用寿命的验证和契约说明。

### 工程

- 支持 `CAORS_LIBRARY_TYPE=AUTO/STATIC/SHARED`、按需测试/benchmark 依赖及外部依赖版本检查。
- 日志选项改用 `CAORS_` 前缀，旧选项仅作默认值，新值优先；子项目默认关闭自身测试。
- 静态库默认 PIC，动态库隐藏内部符号；SONAME 和安装包兼容匹配按 major.minor 划分。

- 工程版本集中到 `cmake/caors_version.cmake`，生成头、运行时查询和安装包版本继续由同一来源派生。
- 项目私有 CMake 模块与辅助函数统一使用 `caors_` 前缀；直接包含旧脚本的调用需改用新文件名。
- 增加父工程集成测试，检查函数名称隔离、警告选项作用域和版本生成一致性。

### 文档

- 建立文档职责、物理目录、权威来源和命名规范。
- 补充开发、CI、版本准备、发布验收和发布后复核流程及模板。
