<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 标准库调度后端

## 状态与范围

状态：实施中，纳入 [v0.2.0](../versions/v0.2.0.md)。按宿主集成需求删除 Asio，保留现有 executor 与外部适配协议。
不引入 I/O、timer、work stealing 或任务图；旧 Asio 性能证据不能作为新后端性能结论。

## 设计与实施

- 固定数量的 `std::thread`、条件变量和受互斥锁保护的 FIFO 队列实现 thread_pool。
- execution_context 不创建线程，由调用线程运行；poll/drain 保留快照批次，stop/restart 保留停止规则。
- 排队节点与 worker 数组使用指定长期内存资源；线程及同步原语内部系统资源除外。
- 执行和丢弃任务均在队列锁外；关闭停止接受任务，drain 执行队列，cancel 丢弃队列并等待运行任务退出。
- 构造线程失败须关闭并回收已创建的 worker。关闭不能从本池任务内部调用；所有者析构不得与其他成员调用并发。
- 删除 vendored Asio、配置 target 和专用依赖测试；协程头改名 coroutine.h，安装包显式查找 Threads。

## 验收

- context 快照、重入投递、耗尽/停止/重启、清空回调；pool drain/cancel、并发提交与关闭、异常隔离。
- 请求丢弃终态、协程提前完成恢复、外部 executor 和资源异步组合回归。
- Windows MSVC/Clang 静态/共享 Debug/Release、Linux 完整 CI 与 sanitizer、安装和迁移 consumer。
- 真实结果写入[版本验收记录](../records/2026-10-19-v0.2.0-host-integration.md)，通过前不标记完成。
