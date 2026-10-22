<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 接入宿主异步 VFS

本指南面向 Gneiss 等已有 loop/executor 的宿主。当前接口由 [IO 参考](../reference/io.md)和
[资源参考](../reference/resources.md#加载来源快照)维护；固定版本、提交及实际验证见
[v0.4.0 记录](../records/2026-10-19-v0.4.0-async-vfs.md)。CAORS 不包含 libuv 后端。

## 新的 backend 接入

新代码可以实现 `io::backend::open(path, open_options, resource, completion)` 并绑定自己的宿主 loop，
再通过 `io::vfs::mount(prefix, shared_backend, target_prefix, lifetime)` 挂载。
调用方使用 `io_context` 的单层 request，普通访问不接触 file_system、resolve_owned 或 start_io。
后端接受后必须完成或销毁凭证，拒绝时不能保留；完成后须清理宿主操作并释放其凭证，不能将后端永久保存在自己的完成状态中；实际文件沿用 async_file 的范围读取、可选 start_write 与关闭协议，实现该协议时显式包含 `caors/io/async_file.h`，普通调用方不需要该头。
完成凭证的 stop_token 用于观察协作取消，不能把请求取消理解成真实 I/O 已清理。

目前 open/read/write/close 和整文件 write/stat/list 可用；旧配置工厂及嵌套路由迁移仍按[VFS 计划](../plans/vfs-api-consolidation.md)推进。
下文为现有 file_system 后端和资源配置兼容接入，已有宿主无需在未完成验收前整体替换。

## 兼容后端接入步骤

1. 将后端作为拥有式 file_system 挂载到现有 resolver/mount；组合模块租约、loop 状态和必要 allocator 所有者作为 lifetime。
   不建立第二套 URI 挂载表，也不将仅有的裸指针包装成无效的 owning 指针。
2. 覆盖 file_system::start_open，保存传入 resolved_file、内存资源和完成凭证到真实打开操作。
   start 返回成功后必须完成或销毁凭证；启动拒绝后不得保留操作继续通知。
3. 打开结果实现 async_file：固定打开时长度，每次按独立 offset 定位，明确结果大小上限和线程约束。
   只将完整 byte_buffer 交给完成凭证；短段继续读取，提前 EOF 返回 short_read。
4. 将 IO 通知投递到宿主 completion executor。保留请求至所需通知结束，并处理请求可查询的拒绝/丢弃失败。
   不依赖“函数返回后才回调”，不在 I/O 线程补调用户通知。
5. loader 从 `context.snapshot_as<resources::provider_snapshot>()` 取得并检查快照，通过 resolve(guid) 固定来源，
   交给 resources::request_bytes 与 file_byte_reader。不要为了使用新入口而从活跃 VFS 再解析一次，改变在途来源。
   新后端构造时绑定 worker；只有旧同步随机读来源才显式选择 file_read_mode::worker_adapter，不作自动回退。
6. file_byte_reader 统一编排打开、限量读取与关闭，宿主须维持真实 I/O 和关闭清理所需循环与凭据。
   普通文件访问通过 io_context.open 得到 file，并显式 file.close；来源切换使用 remount 加明确 reload。

可运行的 manager → loader → 快照 → 读取/关闭示例见
[assets_async_vfs.cpp](../../examples/beta/assets_async_vfs.cpp)。来源可同时提供同步与异步能力，
其读取策略由宿主配置明确选择，既有同步 loader 无需被改成阻塞等待异步结果。

## libuv 等 loop 后端的重点

- CAORS 的 IO 层不依赖 resources。GUID/依赖/缓存继续由资源层处理；libuv 头、句柄与类型留在宿主实现。
- 将停止信号转交 loop 线程。取消不等于 uv_fs 等底层操作已经结束，原生缓冲及请求结构必须存活至其真实回调/清理。
- 完成前校验结果字节数；uv_fs_read 等返回短段时累计，offset 不依赖共享游标。
- 将原始负错误码保存到 io_error.native_code，分配宿主 backend_id；不要保存临时错误文本视图。
- 开始关闭后拒绝新读，已接受的读完成清理后只关闭一次，重复关闭复用结果；析构不能阻塞等待自身 loop。
- 模块 pin 必须覆盖回调退出和原生请求清理，不仅覆盖 complete 调用。宿主 loop 外层须保留模块和 allocator，
  不在插件自己的回调栈内释放最后模块租约并卸载实现代码。
- completion 失败仍须清理原生操作。保留请求并观察 queryable failure，或用拥有式操作析构收口上层完成凭证，避免资源请求永久 pending。

## 来源与资产事务

同一次预加载根及依赖共用来源快照。已有 GUID 在途工作仍沿用首次操作，加入者不替换其来源或调度。
source_id 只表示进程内来源实例，不能当作持久 GUID、内容摘要或磁盘修订。
文件内容、摘要/版本复验、GPU 提交与场景事务仍由宿主负责；remount 本身不使旧 cache 自动失效。

## 联合验证入口

宿主适配可运行后，需要实际验证延迟打开、多个范围并发、短读/截断、取消后迟到回调、
关闭竞争、通知队列拒绝/丢弃，以及模块卸载与 allocator/loop 寿命。
当前仓库的内存、worker、模拟延迟与插件测试只验证 CAORS 协议，不代表 Gneiss/libuv 已联合通过。
