<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 文件后端协议

本页面向绑定宿主 loop/worker 的后端实现者；普通调用者使用 [IO 主参考](io.md)。
具体接入步骤与模块清理要求见[异步 VFS 指南](../guides/async-vfs-integration.md)。

## 路由与后端边界

实现 `io::backend::open(path, options, resource, completion)`，只接收后端相对路径，
不管理挂载或用户通知执行器；通过 vfs.mount 挂载后，由 io_context 负责访问和通知。
必须验证访问权限与创建选项；默认只读已有文件，可写创建/截断必须显式选择。
可选 write/stat/list 缺省 unsupported，不用同步回退掩盖未支持能力。

接受的完成凭证必须完成或销毁；拒绝不得保留。宿主操作清理后释放凭证，
不能把完成状态永久存在后端自身而形成引用环。取消仅为停止信号，模块、缓冲和 allocator 必须覆盖实际清理。

## 可写文件扩展

打开成功交付 async_file；可写句柄覆盖 start_write(offset, owned_bytes, completion)，
成功结果必须等于输入长度。只写拒绝读、只读拒绝写；close 拒绝新操作并等待已接受读写清理。
size 对只读保持打开长度，对可写反映本句柄已成功写入的扩展，不追踪外部修改。
默认 start_write 返回 unsupported，原有只读宿主不被静默转换成阻塞写入。

显式 worker 适配的 random_access_reader 可选覆盖 write_at，允许短写，由 worker 累计；
共享游标在适配器内部串行，不宣称原生异步或并行读写。原生与内存提交限制见主参考。

## 异步文件与精确范围

普通访问统一使用 io_context.open 与 file.read/write/close；不再公开独立的打开、读取、关闭请求函数。
后端通过 async_file 的 start_read/start_write/start_close 接收实际操作和完成凭证。
VFS 打开结果在内部保留解析来源的寿命与 source_id；宿主仍须持有 loop、原生句柄和 allocator 至真实清理。
完整加载示例见 [assets_async_vfs.cpp](../../examples/beta/assets_async_vfs.cpp)。

范围为 uint64；分配前检查打开长度、整数边界、size_t/ptrdiff_t、后端单次上限及明确的 `max_bytes`。
零长度可位于 EOF，超出 EOF 仍失败；只读文件增长不扩大打开长度，可写句柄计入自身已成功写入的扩展；提前 EOF 返回 short_read，不返回伪完整缓冲。
结果字节缓冲由操作拥有，使用指定 resource；取消不能提前释放底层仍在使用的缓冲。
预算只限制单次结果字节数；控制块、任务开销、全局在途字节/数量/排队/优先级由宿主管理。

`native_file_system` 提供真实随机输入句柄，必须显式选用 worker 适配器；内存句柄共享打开时的不可变存储，
不为小范围先复制整个文件。内存文件系统配置/写入仍要求外部同步；已打开句柄独立持有存储。
worker 适配器累积短段，内部串行 seek/read，允许多个范围在途但不宣称并行或内核非阻塞。
原生宿主可以实现 `async_file`，其 offset 定位、缓冲和关闭约束由后端协议明确承担。

关闭开始后拒绝新读写，已接受操作真实清理后仅关闭一次，重复关闭复用关闭结果，各自拥有通知请求。
worker 适配器析构投递不等待的清理任务；队列拒绝/丢弃或分配失败时，由 reader 的 RAII 析构兜底。
析构没有错误接收者，观察关闭错误必须显式 file.close；宿主必须维持 loop/worker 至关闭结束。


旧 file_system 只有 start_open 时仅支持默认只读选项；支持可写模式需实现 start_open_options。
新 backend 的选项由 VFS 桥原样转交，不要求宿主再实现旧文件系统类型。
