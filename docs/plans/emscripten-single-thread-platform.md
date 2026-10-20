<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Emscripten 单线程平台接入

## 状态与目标

实施中，关联 [v0.7.0](../versions/v0.7.0.md)。本页维护实施设计，当前行为与复现证据见
[评估记录](../records/2026-10-20-emscripten-assessment.md)；设计以下列批次状态为准，已实现行为见[接入指南](../guides/emscripten.md)，验收见[集成记录](../records/2026-10-20-emscripten-integration.md)。

## 推荐方案与取舍

### 共享快照

优先新增内部拥有式快照槽，收敛 registry、resolver、mount 三处发布/读取/CAS。
原生已有可用 atomic<shared_ptr> 时优先保留原实现；通过标准库特性宏选择后端，并在实际构建中验证，不只根据平台名
或 C++20 标准开关推断可用性。不可用时采用短临界区 mutex + shared_ptr，单线程模式仍保持同一语义。
工具链若连 mutex 都不可用，须显式证明单线程构建约束后采用同 API 的无锁槽，不能污染原生并发实现。
不回写 libc++ 内部宏，也不强制启用 pthread 来规避标准库特化缺失。

抽象至少覆盖 load/store/compare_exchange；CAS 要同时核对存储指针与共享所有权，失败更新 expected，
保留现有循环重试行为，不能只比较 get()。锁内仅复制、替换或比较指针；旧引用析构、回调、文件操作和
分配尽量在锁外，避免重入死锁；保持不可变快照、线性化发布和旧 snapshot 保活。

备选：全平台统一 mutex 实现，分支更少但改变原生读路径成本；仅在性能与回归证据支持时采纳。
不采用裸指针或无条件清空同步的方案；也不建议直接使用已弃用的 shared_ptr 原子自由函数作为长期接口。

### 平台能力与构建

新增 caors_ 前缀平台模块，分别识别后台线程、原生动态加载和工具链目标，不按 Threads_FOUND 推断
后台线程可用。Emscripten 单线程基线的 find_package(Threads) 可成功，但并不代表可以创建线程。
Web 仅支持静态库，显式请求 SHARED 时配置失败并给出指导，不悄悄改变调用者选项。

拟新增 emscripten-static-debug/release 的 configure/build/test presets，读取 EMSDK 工具链并隔离 build。
CI 固定 5.0.6，可编译能力探测用于兼容诊断；暂不把 CAORS 使用方硬锁死在一个版本号。
审查 Threads::Threads、dlopen/dlsym、导出、filesystem、随机源、异常与 64 位原子；
完整库必须编译，不能靠只构建几份源文件掩盖问题。
GUID 哈希采用明确位宽与最终折叠策略，保留原生 64 位结果，新增 wasm32 一致性/分布基本检查；
检查其他 size_t/指针转换。哈希不承诺跨位宽持久化稳定，不把哈希变化当作 GUID 值变化。

### 无后台线程的执行与资源生命周期

复用宿主 executor、execution_context 非阻塞 poll/drain，每帧或事件回调推进；不在浏览器主线程
调用等待未来事件的阻塞 get/wait/run 循环，不引入 Asyncify 作为默认前提。
worker 可理解为执行调度，不要求操作系统工作线程；completion 可使用同一个或独立的协作队列。
原生 thread_pool 保持原行为；无线程目标不创建线程、也不偷偷退化为 inline executor。
实施时按现有失败约定确定 thread_pool 的显式不可用行为和能力查询，补测试与文档，不能留可构造但无法推进的假实现。
浏览器 I/O 由测试宿主提供延迟完成适配，生产 fetch/libuv 适配仍属于上游。

动态库加载能力在本基线显式 unsupported；保持静态注册、反射和已有对象模型，不将插件关闭扩散为
整个 meta 模块关闭。需要平台失败实现时保留公共契约，不能留下最终消费端未定义符号。

### 验证层次与 CI

先完整库编译和公共头，再安装 find_package consumer，最后请求协议与浏览器。
Node 用于确定性队列/快照/请求回归，浏览器验证真实事件循环延迟、完成与关闭；不互相替代。
首次加入一个独立 Emscripten Release 任务，串行完成编译、Node、安装 consumer、浏览器 smoke，
手动/定时触发遵循现有 CI 策略，不扩展 Linux/Windows 笛卡尔矩阵。
工具链、第三方源码与浏览器驱动各自固定身份并按职责缓存；新增浏览器驱动仅用于测试，
实施时锁定版本并说明下载影响，不加入库的运行时依赖。

## 实施批次与验收

- [x] WEB-00：读取上游与 Granit，复现当前 main 并保存最小 atomic 探针。
- [ ] WEB-01：统一快照槽；原生并发发布/CAS 失败与旧引用保活测试；Web 最小编译验证。
- [ ] WEB-02：平台能力、无线程/无动态加载边界、位宽问题；完整静态库零编译警告与明确不可用行为。
- [ ] WEB-03：独立 presets、公共头、Node 链接运行与安装/迁移 consumer；明确不可运行的原生线程/DSO 测试，不假装全量通过。
- [ ] WEB-04：单线程资源请求协议：宿主延迟 loader、完成线程语义、独立取消与迟到回执、executor 拒绝/丢弃、关闭和 manager 销毁后句柄保活；补浏览器跨事件循环 smoke。
- [ ] WEB-05：一个 Emscripten CI 任务，原生快照专项和 Linux5/Windows3 集成回归；当前功能指南与 CHANGELOG 收口。
- [ ] WEB-06：Gneiss 更新锁定版本后联合验收，再由 Gneiss 删除临时资源开关；CAORS 不直接修改上游业务。

按 WEB-01 至 WEB-05 推进，WEB-06 是上游集成门槛。先交付可编译基础，再验证异步生命周期，最后合并平台功能。
若修复首批问题后仍有 allocate_shared/bind_file_system 错误，独立缩小复现后再定性，不提前调整 allocator API。

## 延期与边界

pthread、动态插件、WASI、wasm64 尚未实施；原因是扩大部署与并发验证范围，与首版单线程接入无直接必要性。
恢复条件为上游明确需求且有独立构建与运行测试，不阻塞本版限定范围。
当前开发分支尚未完成集成验收，不将部分构建通过描述为已发布的 Web 支持。
