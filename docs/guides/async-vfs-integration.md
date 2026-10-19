<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 接入宿主异步 VFS

本指南面向 Gneiss 等已有 loop/executor 的宿主。当前接口由 [IO 参考](../reference/io.md)和
[资源参考](../reference/resources.md#加载来源快照)维护；固定版本、提交及实际验证见
[v0.4.0 记录](../records/2026-10-19-v0.4.0-async-vfs.md)。CAORS 不包含 libuv 后端。

## 接入步骤

1. 将后端作为拥有式 file_system 挂载到现有 resolver/mount；组合模块租约、loop 状态和必要 allocator 所有者作为 lifetime。
   不建立第二套 URI 挂载表，也不将仅有的裸指针包装成无效的 owning 指针。
2. 覆盖 file_system::start_open，保存传入 resolved_file、内存资源和完成凭证到真实打开操作。
   start 返回成功后必须完成或销毁凭证；启动拒绝后不得保留操作继续通知。
3. 打开结果实现 async_file：固定打开时长度，每次按独立 offset 定位，明确结果大小上限和线程约束。
   只将完整 byte_buffer 交给完成凭证；短段继续读取，提前 EOF 返回 short_read。
4. 将 IO 通知投递到宿主 completion executor。保留请求至所需通知结束，并处理请求可查询的拒绝/丢弃失败。
   不依赖“函数返回后才回调”，不在 I/O 线程补调用户通知。
5. loader 使用 async_load_context.sources->resolve(guid)，把拥有式路径交给 request_open。
   需要同步适配时由宿主明确选择 request_open_on_worker，不能在 unsupported 后偷偷阻塞或 read_all 回退。
6. 解析结束或取消后显式 request_close，排空真实 I/O 与关闭，再释放文件、回调和模块/allocator。
   新来源使用 remount 加明确 reload，或创建独立资源上下文。

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
