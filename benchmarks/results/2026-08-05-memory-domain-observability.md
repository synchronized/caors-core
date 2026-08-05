<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 内存域统计与可观测性基线

本文统一记录 process、registry layer、scratch 和 result/user 四个内存域可以可靠观测的指标。
数据来自同机 Release benchmark 时才用于性能比较；表中的“不适用”和“不可观测”不是零值。

| 内存域 | 分配次数与峰值 | 碎片 | 释放时机与延迟 |
| --- | --- | --- | --- |
| process | 默认后端不附加 tracking；按需使用 identity/debug snapshot 观察逻辑条目 | system 后端内部碎片不可观测 | runtime/process 结束时释放，不存在插件卸载或单次操作释放延迟 |
| registry layer | 8 类型 system 基线：107 次上游分配，live/peak 14520 字节 | system 后端内部碎片不可观测；pool 候选可报告 requested/capacity | system 卸载中位数 20.4 μs；卸载后 live 为 0 |
| scratch | 1000 次 typed MessagePack：2 次上游分配、65584 字节；逻辑峰值 1344 字节、1 chunk | stack chunk 的保留容量不等同 allocator 内部碎片；本样本峰值后显式 reset 归零 | 每次调用 rewind，1000 次 mark/rewind；最终 reset 后上游 live 为 0 |
| result/user | 1000 次 typed MessagePack：1000 次分配、736000 字节；单次峰值 736 字节 | 由调用方选择的后端决定，普通 tracking 不推导碎片 | 返回对象逐次析构，样本结束后 live 为 0；延迟包含在完整 typed read 路径中 |

registry 策略完整三轮数据见
`2026-08-05-windows-clang-registry-resources-a4f5ba1.md`；MessagePack result/scratch 数据见
`2026-07-29-windows-clang-messagepack-baseline.md`。

## 解释规则

- `tracking_resource` 只报告调用方请求和上游分配，不能看到 system allocator 内部的 size class、页保留
  或外部碎片，因此不计算伪“碎片率”。
- `size_class_pool_resource` 的 `requested/capacity` 差值才可用于观察池内内部碎片；`reserved_bytes`
  表示策略保留量，不等于泄漏。
- scratch 的 rewind 延迟属于完整操作的一部分。只有真实 workload 显示它成为热点时，才新增独立微基准，
  避免用空 scope 得出失真的纳秒结论。
- process 域有意保持到进程结束。长期存活不能作为泄漏判断，诊断重点是其中不得保存插件代码指针或插件
  私有对象。

## 当前决策

- process 与 registry 默认继续使用 system resource；
- registry 保留显式资源注入，不默认采用 monotonic 或 pool；
- scratch 继续使用可回滚 stack resource；
- result/user resource 继续由调用方或长期 owner 决定；
- 后续基准新增内存数据时必须同时标明域、后端、释放阶段和指标是否可观测。

