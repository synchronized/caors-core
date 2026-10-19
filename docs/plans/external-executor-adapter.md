<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 外部 executor 适配计划

## 状态与范围

状态：已完成，对应 C51-06。版本与依赖见 [v0.2.0](../versions/v0.2.0.md)。
现有 `tasks::executor` 主要由 CAORS context/thread_pool 构造；新增宿主投递适配，
内置调度改用标准库线程，不要求引入 libuv 依赖或新建线程池。

## 设计与实施

- [x] 设计受控外部投递入口，复用 `tasks::task` 与现有错误模型；确定适配状态所有者及 allocator。
  非拥有的宿主上下文也必须通过关闭/保活协议防止回调悬空。
- [x] 明确提交前、接受成功后、拒绝后的任务所有权及恰好一次销毁责任；
  不能让宿主接受后静默丢弃任务而使关联请求永久 pending。
- [x] 定义关闭过程及接受/关闭竞争：停止新提交、完成或取消已接受工作、排空回调，然后释放上下文。
  adapter 句柄晚于宿主销毁时返回明确错误，不访问旧用户指针。
- [x] 规定同步 inline 接受的重入语义与是否允许，禁止持有内部锁调用宿主回调。
  任务异常、拒绝投递和丢弃路径必须映射到现有请求终态。
- [x] 用模拟宿主线程池和所属线程完成队列接入资源加载；后台任务与 cache commit 分别投递，
  不放宽资源 cache 的所属线程约束。复用模块寿命及独立订阅取消规则。
- [x] 补公共头注释、tasks/resources 参考及可运行示例；不泄漏 Asio 类型，不新增平行 request 系统。

## 验收

覆盖正常接受、拒绝、关闭、已接受任务丢弃、inline 重入、提交/关闭竞争和 adapter 句柄晚销毁。
验证 task 销毁恰好一次、请求都有终态、上下文释放后无回调；使用线程 ID 断言 cache commit 发生在所属线程。
集成 A/B 共享加载、A 取消而 B 成功，以及全部取消与宿主关闭场景；模块卸载遵守统一寿命协议。
Windows/Linux 和 sanitizer 验证通过，记录 API 契约、线程模型及实际执行证据。

## 已确定取舍

采用 `external_executor_context` 的拥有型 post/close 协议和 `external_executor` 所有者。上下文封闭队列并排空或销毁工作，适配器句柄可晚存活；接受后丢弃通过请求保护进入失败终态。同步执行可重入；不提供 libuv 实现，不改变 cache 所属线程规则。接口见[任务参考](../reference/tasks.md#外部-executor)。

## 验收收口

实现与契约验收已完成：标准库后端 Windows 八组完整测试、Linux 八组 CI、重复回归及安装迁移通过。
实际源码提交、平台与适用限制统一见[版本验收记录](../records/2026-10-19-v0.2.0-host-integration.md)。
版本元数据与发布收口由[版本页](../versions/v0.2.0.md)维护；本计划完成不表示已公开发布或 Gneiss 已接入。
