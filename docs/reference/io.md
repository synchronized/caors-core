<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# IO

本页维护普通文件访问的当前 API：统一异步操作、文件句柄和虚拟路由，为资源系统、配置及宿主来源接入提供底座。
新文件访问优先使用下方 vfs/io_context；旧 file_system、路由器及同步流保留为迁移兼容与工具能力，使用时显式包含 `caors/io/legacy.h` 或对应单头。宿主接入见[异步 VFS 指南](../guides/async-vfs-integration.md)。

## VFS 与统一文件访问

推荐入口包含四个概念：vfs 配置挂载，io_context 配置完成调度，file 持有打开文件，request 持有操作控制及结果。
公共聚合头 `caors/io.h` 和 `caors/io/context.h` 不包含旧 file_system/resolver 或原始文件协议定义，后端协议见 `caors/io/backend.h`。
完整可运行示例见 [vfs_basic.cpp](../../examples/beta/vfs_basic.cpp)。

```cpp
io::vfs files;
files.mount("assets://game", io::make_native_backend("assets", worker));
io::io_context io{files, {.completion = main_executor, .resource = resource}};
auto reading = io.read("assets://game/hero.png", {.max_bytes = 8 * 1024 * 1024}, on_bytes);
auto opening = io.open("assets://game/base.pack", {.access = io::file_access::read}, on_open);
```

这是接入片段，须检查 mount 返回值，并持有 reading/opening 至所需通知结束；两次操作相互独立。
open 默认 `read + open_existing`，不会创建或截断文件。可选权限为 read/write/read_write；
创建策略为 open_existing/create_always，后者仅允许可写访问，显式创建或截断，不增加 open_write 名称。
后端不支持的权限返回失败；不提供隐式 append、独占创建或原子替换语义。
结果类型为 `io_result<T>`，文件操作直接返回 `request<T>`；即使发起拒绝也通过请求报告，
错误回调经配置的 completion 投递。completion 无效、拒绝或丢弃时不能保证回调执行，应查询结果。
request 的布尔值仅表示有效句柄，不表示成功；done/result 查看终态，cancel 为协作取消，析构不等待清理。

打开成功后，从结果取出 file（值类型、共享实际文件），再使用 `file.read({offset, length}, callback)`
、`file.write({offset}, owned_bytes, callback)` 和 `file.close(callback)`。文件已经绑定 completion、字节 allocator 和单次范围预算，操作不逐次传 executor。
读取必须读满范围，短读失败；只写句柄拒绝读取，只读句柄拒绝写入，包括空范围。
只读 size 为打开时长度；可写 size 包含本句柄已成功写入的扩展，不追踪外部修改。复制共享关闭状态，显式 close 作用于全部副本，析构非阻塞兜底。

io_context.read 组合打开、有界整文件读取、关闭；资源 byte_reader 复用同一内部状态机。
读取的中间 IO 结果只读，交付前复制，结果峰值约为两份预算，另有后端和状态开销，预算不是全局在途限制。
只读阶段取消在步骤边界观察，不承诺强制终止在途读取；open 的协作取消会传递至后端完成凭证。

vfs 只配置挂载，不继承 backend。挂载拥有 backend 和可选 lifetime，已有请求与快照不受 unmount/clear 影响。
make_native_backend 在构造时绑定可阻塞 worker；make_memory_backend 提供内存后端，store 用于配置内存数据。
后端实现只实现 `backend::open` 并绑定自己的 loop/worker，不负责公共路由和用户完成调度。
URI/普通路径匹配复用原路由算法，目前以私有传输适配连接旧路由实现，没有第二套路由算法。

活跃 vfs 构造的 context 借用配置用于后续调用，每次发起捕获稳定路由；如需固定整批路由：

```cpp
auto snapshot = files.snapshot();
// 检查 snapshot 成功后构造批次访问入口。
io::io_context batch{std::move(snapshot.value), {.completion = main_executor}};
```

snapshot 固定路由、后端与 lifetime，不冻结磁盘内容或任意后端可变状态。
allocator 和 executor/宿主循环必须覆盖真实操作及结果清理；vfs 的 allocator 也须覆盖仍持有的快照。

新主线另提供 `io.write(path, byte_buffer, callback)`、`io.stat(path, callback)` 和
`io.list(path, callback)`，同样返回单层 request，不逐次传 executor。
write 消费拥有式缓冲，创建或截断整个目标文件，成功值为写入字节数；不保证原子替换或掉电持久化。
原生后端在成功交付前检查关闭错误；取消只影响请求，不回滚已经发生的写入。
需要依赖前一个操作的结果时，在完成回调/await 后发起下一操作，不依赖异步调用的排列顺序。

stat 区分结果失败与不存在，不再用普通 exists 布尔值掩盖错误。list 仅枚举后端目录的直接子项，
返回路径映射到请求的逻辑目录，保留带冒号的文件名，拒绝后端返回的非直接子项名；不合并虚拟挂载覆盖目录。
元数据类型定义在 metadata.h，底层缺失能力返回 unsupported，不静默同步回退。
内存后端索引串行化且不持锁回调，打开文件保留当时的内存内容；原生后端在构造绑定的 worker 执行操作。
分配器可能跨 worker/完成线程分配及释放，须支持相应线程访问，并覆盖输入缓冲、结果与控制清理。

资源侧可使用 `resources::vfs_source` 将 VFS 接入 provider 的批次快照，默认 JSON loader 无需修改。
该来源只服务异步快照读取，不支持旧 source 同步 read/write/exists，不能用于同步 JSON saver。
file.write 精确写满输入，worker 适配累计短写；预算 max_write_bytes 限制单次输入长度，
不代表文件总长度或进程峰值限制。位置使用显式 offset，不维护隐式共享游标；依赖操作须等待前项完成。
close 立即拒绝新操作，并等待此前已接受读写完成实际清理；写入取消不回滚已经发生的字节。
原生后端通过串行 seek/read/write 适配，open_existing 不截断，不隐式创建；当前 std::fstream 实现
即使逻辑权限为 write 也需要底层可读写权限。close 检查缓冲提交错误，不承诺落盘持久化。
内存可写句柄在打开时复制已有内容，max_read_bytes 也限制这份初始缓冲；create_always 在打开时截断。
后续修改在 close 时整体发布，已有只读句柄仍持有旧内容；并发写者后关闭者覆盖，不提供冲突检测。
未写入的已有文件句柄关闭时不覆盖后来更新。析构非阻塞请求清理，须显式 close 才能观察提交失败。
旧项目配置工厂和同步工具流仍走显式兼容入口；它们的退出有独立迁移与验收要求，不能把新调用方主线等同于旧接口全部删除。

## 路径与元数据

`io_context::stat()` 返回 `file_status`，第一版只定义通用基础字段：

- `kind`：文件或目录。
- `size` / `has_size`：文件字节大小。目录、虚拟目录或无法查询大小的后端可以不提供。
- `modified_time` / `has_modified_time`：最后修改时间。内存后端和部分虚拟后端可以不提供。
- `read_only` / `has_read_only`：是否只读。只有能查询权限语义的后端才需要提供。

完整路径类型约定如下。

## path / path_view

`io::path_view` 是非拥有字符串视图，`io::path` 是拥有式字符串。它们用于表达 IO 和资源层的路径语义，
但不等同于 `std::filesystem::path`：

```cpp
caors::io::path path = caors::io::normalize_path("\\assets//textures\\albedo.png");
// path.string_view() == "assets/textures/albedo.png"
```

当前规则：

- `path_view` 不拥有内容，调用方需要保证底层字符串生命周期。
- `path` 使用 CAORS allocator 保存内容。
- `path` 普通构造不自动规范化，避免破坏本地绝对路径。
- `normalize_path()` 和 `join_path()` 面向虚拟路径：统一为 `/`，去掉开头分隔符，折叠连续分隔符。
- 原生后端把 `path_view` 转为宿主平台路径。

## uri / location

`io::uri_view` 和 `io::uri` 用于解析逻辑资源位置和后端路由边界。它们不是 MIME 或 content-type：
URI 的 scheme 决定“去哪一类后端取”，MIME / content-type 只描述“取到的内容是什么格式”。
`io::location_view` 和 `io::location` 则是资源层对“普通虚拟路径或 URI”的拥有/非拥有表示。

```cpp
caors::io::uri_view location{"caors://assets/textures/albedo.png"};

// scheme == "caors"
// authority == "assets"
// path == "textures/albedo.png"
```

第一版 URI 解析规则：

- 支持 `scheme://authority/path`、`scheme:path` 和普通相对路径。
- `path()` 会去掉查询参数和片段文本，但不会做百分号解码。
- `caors://assets/a.png` 的 scheme 是 `caors`，authority 是 `assets`，path 是 `a.png`。
- 普通路径 `assets/a.png` 没有 scheme 和 authority，path 是完整输入路径。
- `io::normalize_location()` 返回 `io::location`，用于资源位置规范化：普通路径按
  `normalize_path()` 处理；URI 会保留 scheme / authority，只规范化 path 部分。
- `io::join_location()` 返回 `io::location`，用于拼接资源位置：普通路径等价于
  `join_path()`；URI 会在 path 部分拼接，并保留原来的 scheme / authority。
- 文件访问的入口接收 path_view；资源 manifest/provider/source 等面向资源位置的 API 使用 location_view。

## 请求、取消与错误

`request<T>`（底层 io_request<T>）为 move-only；必须持有至需要的结果交付完毕，最后句柄析构只协作请求停止，不等待。
`result()` 在 pending 时为空，终态返回拥有寿命的只读 `io_result<T>`；结果可独立持有。
这与资源层普通 load_request 析构不自动取消的规则不同。取消不撤回已安排的通知，回调不得借用短寿命对象。

后端接收 move-only `io_completion<T>`，完成或销毁凭证，销毁未完成凭证按取消收尾；重复完成返回 false。
同一凭证允许并发 complete，但禁止与其移动/析构并发。后端启动拒绝直接返回诊断且不通知用户；
提前完成与拒绝同时发生时以启动拒绝为准，拒绝后不得保留凭证继续操作。

用户通知始终经过指定 completion executor，可能在发起函数返回前重入，调用前须准备回调状态。
准备好的成功结果在通知任务开始时才与停止信号作终态裁决；取消不会撤销已确定的成功。
停止是协作式：真实操作须完成或销毁凭证才会收尾，不能只停止 loop 后留下永久在途凭证。
通知任务被拒绝/丢弃时，请求进入可查询的 completion_rejected，不在 I/O 线程补调用户代码。
回调应不抛异常；任务执行异常约束沿用 tasks，已经裁决的终态不会因观察者异常改写。

`io_error` 保留统一分类、进程内 backend_id 和原始 native_code，不借用临时错误字符串。
范围/预算在后端启动前校验；普通入口仍用请求报告失败，底层执行失败通过请求结果返回。后端诊断不被转换为临时文本。
控制块或任务构造仍可能因分配失败抛出 bad_alloc；已接受操作的通知分配失败以 out_of_memory 收尾。
不承诺稳定 C++ ABI，升级须统一重编译库、宿主和插件。

## 后端与兼容入口

宿主实现见[文件后端协议](io-backends.md)和[异步 VFS 指南](../guides/async-vfs-integration.md)。
旧 file_system、resolver、同步流和组合函数见[IO 兼容接口](io-legacy.md)，不属于普通文件访问入口。

## 已移除入口的迁移

以下五个旧公共函数已经删除，不提供兼容别名：

| 旧入口 | 当前替代 |
| --- | --- |
| request_open | io_context.open |
| request_open_on_worker | 构造 backend 时绑定 worker，再通过 io_context.open 打开 |
| request_read | file.read；预算与完成调度在打开时绑定 |
| read_all_bounded | 路径整体读取用 io_context.read；已有句柄用 file.read({0, file.size()}) |
| request_close | file.close |

普通操作直接返回单层 request，发起失败也通过请求查询或完成通知报告，不再检查外层 io_result<request>。
如果要保持旧批次来源，使用拥有式 VFS 快照构造 io_context；资源 loader 使用捕获的来源与 request_bytes，
不把已经解析的来源重新投给活跃路由。详细字节协议见[资源参考](resources.md#统一字节读取适配)。
库内部仍复用有界读取与关闭编排，但对应头仅位于 src，不安装为调用方接口。

本批属于破坏性接口删除，库与调用方须统一重编译；安装到干净前缀，避免保留已删除的内部头。
