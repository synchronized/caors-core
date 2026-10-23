<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# IO 职责与实现复用

本文说明同步文件能力、异步接入和虚拟路由之间的边界。当前接口与行为分别由
[IO 参考](../reference/io.md)、[后端协议](../reference/io-backends.md)和
[同步与兼容接口](../reference/io-legacy.md)维护；后续工作由[VFS 计划](../plans/vfs-api-consolidation.md)跟踪。

## 判断原则

简化的目标是每项职责只有一个实现入口，普通调用者只需了解所用接口。
删除类名或减少文件数不能单独证明架构更简单；若删除后必须重建同样的文件操作、存储和生命周期逻辑，
应优先保留并复用原实现。同步工具、配置保存和内存序列化有独立用途，不以退出异步加载主线为由一并删除。

## 当前依赖

```text
普通调用方 → io_context / file → backend / async_file
                 ↓                  ↓
           vfs / vfs_snapshot    本地或内存实现、宿主 loop
                                    ↓（内置后端）
                           native_file_system / memory_file_system

兼容工具 / 同步保存 → source / file_system → 同一批本地或内存实现
```

vfs 已使用自己的不可变挂载表，快照固定路由并保活后端；普通访问不经过旧 resolver。
内置 native_backend 绑定 worker，复用 native_file_system 的实际文件操作；memory_backend
复用 memory_file_system 的存储与查询，并串行访问索引。它们不是两套独立的磁盘或内存存储实现。

当前 file_system 仍混合同步访问、异步打开扩展和旧路由快照协议。
旧 file_system_resolver、mount_file_system 仍被兼容配置与来源使用；
resources::vfs_source 已直接捕获新 VFS，provider_snapshot 保存已有 backend_location。
旧来源通过 file_source 内部的单向适配接入 backend；旧配置与路由消费仍是当前限制。

## 已确认的职责边界

| 职责 | 负责层 | 保留与收敛原则 |
| --- | --- | --- |
| 挂载选择与批次路由 | vfs / vfs_snapshot | 新访问只使用这一套路由；旧路由随实际消费迁移退出 |
| 异步后端接入 | backend / async_file | 管理宿主操作的接受、完成、取消观察与关闭；不管理虚拟挂载 |
| 用户调度及组合操作 | io_context / file | 配置通知执行器、预算和组合读取；不复制实际文件读写 |
| 本地文件与内存存储 | 现有具体实现 | 保留文件处理、存储、目录和属性算法，供同步入口与异步适配共用 |
| 同步工具与保存 | 同步文件及流接口 | 允许直接调用同步实现；不通过等待异步回调制造同步入口 |
| GUID、缓存、反序列化 | 资源层 | 文件读取复用 IO 编排；宿主 loader 可直接接入字节服务 |

backend 与同步实现处于不同层次：一个表达异步执行协议，一个执行实际操作。
可以保留两种调用方式，但不能因此复制存储、路径映射和文件提交算法。
宿主已有真正异步 IO 时直接实现 backend，无须经过同步 file_system；
同步后端需要异步使用时，由适配层绑定可阻塞 worker。

## file_system 的处理条件

现有 file_system 暂时保留，不承诺按固定日期删除。先处理其混入的路由和异步兼容扩展，
再判断它是否仍是同步工具与具体实现共享的有效边界。
若收敛后仅剩一套有明确调用者的同步文件协议，可以保留；若所有职责已有唯一归属且调用点已迁完，才删除空壳。
不引入一套方法相同的接口来完成名义上的删除。

资源快照迁移应复用已拥有的后端位置、快照和读取编排。
只有现有类型无法表达实际契约时才增加新公共抽象；不能为搬走一个旧类型，再增加多个功能重叠的 resolver、source 或 snapshot 接口。
每批改动需列出删去的重复职责、保留实现及真实消费证据。
