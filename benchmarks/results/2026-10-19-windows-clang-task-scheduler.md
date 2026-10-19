<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Windows Clang 新旧任务调度器对比

## 环境与方法

- 日期：2026-10-19；Windows 10，10.0.19045；Intel Core i5-8600K @ 3.60 GHz。
- 编译器：Clang 23.1.1；C++20，Release，`-O3 -DNDEBUG`，关闭日志和调试输出。
- 新实现：`73c0290`，`windows-clang-benchmark`，目标 `caors_scheduler_benchmarks`。
- 旧实现：`a43762e`，独立 worktree，使用同一编译器、构建选项和同一份
  `benchmarks/tasks/scheduler_bench.cpp`；临时追加独立 CMake 目标，不修改旧提交。
- 停止本机构建后，新旧实现交替运行三次；每个场景 10,000 个延迟样本。
- `context.lifecycle` 包含构造、一次投递、drain 和析构；`context.post_drain` 使用常驻 context。
- `pool.roundtrip` 使用常驻单 worker，每轮投递一个空任务及一个完成屏障任务，等待屏障后进入下一轮；
  包含线程同步和两次提交，不代表纯提交延迟、饱和吞吐或真实业务负载。
- 吞吐来自采样循环，包含计时和样本收集开销；100 ns 左右的结果受时钟粒度影响。

## 延迟与吞吐

延迟单位为 ns；每格为 p50 / p95。吞吐为三次采样循环吞吐的中位数，单位为场景操作/秒。

| 场景 | 实现 | 第一次 | 第二次 | 第三次 | 吞吐中位数 |
| --- | --- | ---: | ---: | ---: | ---: |
| inline | 旧 | 100 / 100 | 100 / 100 | 100 / 100 | 10,728,500 |
| inline | Asio | 100 / 100 | 100 / 100 | 100 / 100 | 9,819,320 |
| request.inline | 旧 | 300 / 400 | 300 / 400 | 300 / 400 | 2,948,110 |
| request.inline | Asio | 300 / 400 | 300 / 400 | 300 / 400 | 2,787,920 |
| context.lifecycle | 旧 | 400 / 400 | 400 / 400 | 400 / 400 | 2,550,890 |
| context.lifecycle | Asio | 4,500 / 4,800 | 4,500 / 4,900 | 4,600 / 4,800 | 213,313 |
| context.post_drain | 旧 | 300 / 400 | 300 / 400 | 300 / 400 | 2,955,000 |
| context.post_drain | Asio | 2,700 / 2,800 | 2,700 / 2,800 | 2,700 / 2,800 | 363,223 |
| pool.roundtrip | 旧 | 900 / 15,500 | 900 / 1,800 | 1,000 / 1,800 | 686,714 |
| pool.roundtrip | Asio | 3,600 / 21,600 | 3,600 / 18,800 | 3,600 / 17,100 | 142,188 |

## 显式资源分配和布局

分配统计在延迟采样前单独执行一次场景。下表为“次数 / 累计字节”；所有场景结束后，该 tracking
resource 的存活字节均为 0。常驻 context 和 pool 自身使用系统资源；统计只覆盖传入的 callable
或 request 资源，不能用于推断全局分配次数。Asio 内部 operation、线程、采样数组和屏障任务不在统计内。

| 场景 | 旧 | Asio |
| --- | ---: | ---: |
| inline | 1 / 8 | 1 / 8 |
| request.inline | 3 / 177 | 3 / 209 |
| context.lifecycle | 5 / 136 | 3 / 192 |
| context.post_drain | 1 / 8 | 1 / 8 |
| pool.roundtrip | 1 / 1 | 1 / 1 |

| 类型 | 旧 sizeof | Asio sizeof |
| --- | ---: | ---: |
| task | 32 | 32 |
| request | 16 | 16 |
| completion_queue / execution_context | 136 | 8 |

## 阶段性结论

- inline 和同步 request 的 p50/p95 接近旧实现；request 的显式分配字节增加 32 B。
- 当前 Windows 空任务场景下，Asio context 构造、常驻 post/drain 和线程池往返均明显慢于旧实现。
  本次迁移提供统一调度、可安全失效句柄和协程等待能力，不能描述为调度性能优化。
- 应复用长期 context/pool，避免在每个任务中构造执行环境；高频微任务需基于实际负载评估批量投递。
  本基准未单独剖析锁、内核唤醒和 Asio operation 的成本，不能据此断定具体瓶颈。
- 当前不引入 task SBO；这组数据只证明存在 callable 分配，没有独立对照实验支持具体 SBO 容量和收益。
  SBO、operation allocator 和批量调度优化保留为后续独立工作。
- 已有任务与资源协程等待基准分别见 `2026-08-07-windows-clang-task-coroutines.md` 和
  `2026-08-07-windows-clang-resource-coroutines.md`；固定 Linux 环境的正式性能复验与真实业务负载
  评估仍延期，不使用共享 CI runner 的运行时间替代正式性能数据。
