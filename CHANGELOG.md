<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 变更记录

本文件记录面向使用者的变化。未发布内容进入 Unreleased，发布准备时移入带版本和日期的章节。
本次建立变更记录，不追补未经核实的历史版本；工程版本不等同于已经发布。
准备与发布步骤见[发布流程](docs/guides/release.md)。

## Unreleased

- IO：统一 open 支持显式读写权限与创建/截断选项，file.write 提供有预算的范围写入并复用关闭清理。
- 新增 io_context 整文件 write/stat/list，输入与结果拥有数据，元数据路径回映到逻辑目录；原生写入检查关闭错误。io.h 仅聚合新主线，旧路由/同步文件系统与原始文件组合显式使用 io/legacy.h。新增 resources::vfs_source，将新 VFS 批次路由接入默认 JSON loader。

- 新增 vfs/backend/io_context/file 访问主线；open 统一命名、默认只读已有文件，操作返回单层 request。native 后端在构造时绑定 worker；资源 byte_reader 复用 IO 整体读取状态机。旧写入、目录、属性及配置接口保留迁移，见[IO 参考](docs/reference/io.md#vfs-与统一文件访问)。

- 日常请求收敛为 load/reload/preload 三类：request_refresh 合入 request_reload，每次重载独立启动、无缓存也可创建、最新重载决定默认绑定。manager/group 删除逐次 worker/completion 参数和模板 preload，特殊调度使用 group，同类型 preload 在低层 database 使用。

- 删除 asset_manager/database 的同步 load、load_handle、reload、reload_handle、preload 执行入口，统一使用 request_*；find/resolve/plan_preload 查询保留。loader 注册和手动重建绑定收敛到单槽，不同实例重复注册返回 already_exists；低层同步组合函数仅接受 sync_loader_adapter，不再运行时探测原生异步实现。

- 默认 JSON 工厂迁至异步请求快照路径：`json::loader<T>` 现在接受 reader 或文件读取模式；原 provider 构造实现更名为 `json::sync_loader<T>`，短期 `json::byte_loader<T>` 更名为 loader。默认原生异步，阻塞文件后端须显式选择 worker_adapter；工厂调用方改用 request_load/reload/preload，详见[迁移参考](docs/reference/assets.md#json-异步字节接入)。

- 新增统一 `resources::byte_reader` 协议、`request_bytes` 与 VFS 原生/显式 worker 适配；`assets::json::loader<T>` 使用请求快照异步读字节并构造资源。同步读写兼容路径仍保留，迁移与预算限制见[资源参考](docs/reference/resources.md#统一字节读取适配)。

- 加载协议统一为 `resources::loader<T>::start`，删除独立 `async_loader<T>`。原生异步实现改继承 loader；已有同步实现改继承 `sync_loader_adapter<T>` 并保留 load 钩子。旧同步入口仅使用显式适配能力，不等待异步 loader。

- database 移除 provider 设置及清单绑定接口；清单绑定函数迁入 `resources/manifest_binding.h`。工程来源改由 `project_builder::provider()` 访问。空快照捕获器现在关闭捕获，恢复工程来源须显式调用 `use_provider_snapshot()`。

- 文件系统/source 绑定、配置工厂注册、`create_owned` 和工厂扩展类型的公开入口迁至 `project_builder`；工厂通过 `context.project()` 使用装配操作。JSON/通用类型工厂 helper 改为接收 builder，manager 保留手动资源类型注册和请求主线。

- 工程注册与配置对象状态移入私有实现，保持既有生命周期；修复 `create_owned` 在拥有表扩容失败时新对象未释放的问题。

- 工程构建入口迁至 `assets::project_builder{manager}.build(config)` / `build_detailed(config)`，删除 manager 上的同名入口；配置对象仍由 manager 保活。已有配置驱动接入需要迁移调用点。

- 异步加载管线改用通用快照捕获器；低层 database 可配置宿主策略，工程环境提供兼容的 provider 捕获适配。缓存命中与共享请求不重复捕获，预加载闭包共享批次结果。

- 加载上下文的 `sources` 改为通用 `snapshot`；VFS loader 使用 `snapshot_as<resources::provider_snapshot>()` 并检查结果。loader 协议头不再传递包含具体文件来源；自定义快照可按批次保活。此变更需迁移调用代码并统一重编译库及使用方，尚未发布。

## v0.7.0 — 2026-10-21（alpha，已发布）

- Emscripten 静态单线程基线：补充共享快照回退、平台能力查询、32 位 GUID 哈希和浮点解析兼容，接入与限制见 [Web 指南](docs/guides/emscripten.md)。
- 修复异步 I/O 终态回调对 std::function 移后状态的依赖，确保通知被丢弃时释放捕获对象并收口分块读取。
- 公共类型布局发生变化，升级时统一重编译库与使用方。64 位 GUID 哈希保持原算法，32 位采用折叠；哈希值不作为持久化标识。

## v0.6.0 — 2026-10-20（alpha，已发布）

- Catch2 与 ctrack 改为锁定归档源码缓存，新增离线、按项并发锁和可配置缓存目录；首次测试/基准配置需要联网或预填缓存，核心构建和父目标复用不获取依赖。

- 修复空参数包实例化的未使用诊断，以及 identity 测试插件以 C linkage 按值返回 C++ 类型的警告。
- 默认将 CAORS 自有目标已启用的编译警告视为错误，可通过 `CAORS_WARNINGS_AS_ERRORS=OFF` 关闭；修复构造函数导出、名称遮蔽及 JSON 控制字符转义诊断，工具链例外见构建指南。

## v0.5.0 — 2026-10-20（alpha，已发布）

- 新增 request_refresh：独立选择新来源，最新刷新意图控制单资源默认绑定发布，旧请求仍可返回自己的对象。
- 修复资源 cache 发布阶段分配失败时索引可能不一致的问题，替换先准备分配再提交。
- 新增串行 read_all_chunked 与 consume_chunks，区分完整结果内存和逐块消费背压；不隐式关闭共享文件。
- 新增来源快照成本基准，同次捕获内共享 source 冻结根；不跨次复用快照。cache/request 布局改变，升级需统一重编译宿主与插件。测量和优化状态见 [v0.5.0](docs/versions/v0.5.0.md)。

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
