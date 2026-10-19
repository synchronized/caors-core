<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# CAORS 文档中心

这里统一索引 CAORS 的指南、当前行为参考、架构说明、计划和验证资料。
项目介绍和快速开始见 [README.md](../README.md)，文档维护规则见
[项目文档规范](../DOCUMENTATION_GUIDE.md)。现有专题路径继续有效，分类表示内容职责。

## 推荐阅读顺序

- 初次接入：构建与安装 → 示例 → 所用模块的行为参考 → API 稳定性与生命周期约束。
- 参与开发：开发规范 → 文档规范 → 路线图 → 相关计划 → 测试与验证资料。

下方提供对应文档入口；计划中的方案不代表已经提供的公共能力。

## 操作与开发指南

- [build.md](build.md)：环境要求、CMake preset、构建、测试、安装和 CMake 选项。
- [benchmarking.md](benchmarking.md)：benchmark 构建入口、当前用例和后续专项拆分建议。
- [development.md](development.md)：目录结构、代码规范、格式化、静态检查和新增模块规则。
- [DOCUMENTATION_GUIDE.md](../DOCUMENTATION_GUIDE.md)：文档职责、事实维护入口、生命周期和内容模板。
- [registration.md](registration.md)：类型、对象、meta 属性/方法和序列化的推荐注册顺序与初始化策略。
- [examples/README.md](../examples/README.md)：可运行示例、构建、运行和新增规则。
- [tests/README.md](../tests/README.md)：测试组织、运行和新增规则。

## 架构与模块边界

- [dll-plugins.md](dll-plugins.md)：共享 runtime、类型身份、ABI、注册和插件卸载约束。
- [module-boundary.md](module-boundary.md)：跨 EXE、DLL、插件和静态库的 runtime 唯一性、视图传递和 ABI 风险边界。

## API、行为与稳定性参考

- [api-stability.md](api-stability.md)：当前阶段、稳定候选与实验性范围的统一维护入口。
- [config.md](config.md)：编译期配置常量、异常检测、内部分配策略和默认 new 对齐。
- [traits.md](traits.md)：底层 traits 扩展点、adapters 边界和用户特化规则。

- [assets.md](assets.md)：工程资源配置、asset manager、source/type factory 和 JSON 资源适配。
- [core.md](core.md)：core 主题，包含 `string_name`、`guid`、`type_guid` 和基础容器规划。
- [error.md](error.md)：统一错误码、错误来源域和 `std::error_code` 适配。
- [events.md](events.md)：轻量 delegate、同步 signal、连接视图、同步 dispatcher、连接句柄和事件系统边界。
- [io.md](io.md)：虚拟路径、同步字节流、文件系统抽象、本地文件系统、内存文件系统和挂载文件系统。
- [math.md](math.md)：基础数学值类型，包含向量、矩阵、四元数、矩形和 AABB。
- [memory.md](memory.md)：内存资源、分配器、策略资源和调试跟踪。
- [meta.md](meta.md)：类型元数据、`type_guid`、`meta::type`、`meta::registry`、metadata 和 capability data。
- [object.md](object.md)：对象基类、对象类型 API 和对象创建销毁入口。
- [meta-properties.md](meta-properties.md)：meta 内建字段/方法能力和手写注册入口。
- [resources.md](resources.md)：资源身份、资源引用、运行期 handle、manager/cache、依赖图、manifest、
  provider、loader/saver、非拥有 database 编排入口、同步 reload 通知、异步加载请求和资源引用 resolver。
- [serialization.md](serialization.md)：序列化元信息、注册表、JSON/文本读写、codec、资源引用和多态读写边界。
- [tasks.md](tasks.md)：move-only task、轻量 executor、Asio execution context、线程池和可选协程等待。
- [value.md](value.md)：`any`、`any_ref`、运行期 view、细粒度头文件边界和 `variant` 数据 DOM。

## 规划与验证证据

- [checklists/beta-readiness.md](checklists/beta-readiness.md)：Beta Gate、边界扫描与记录要求。
- [records/README.md](records/README.md)：实施、验收与历史证据索引。

- [roadmap.md](roadmap.md)：后续方向和阶段状态；详细实施范围见对应计划。
- [plans/README.md](plans/README.md)：进行中、已完成和延期复验的开发计划索引。
- [checklists/linux-validation.md](checklists/linux-validation.md)：Linux 验证步骤和检查清单。
- [benchmarks/results/README.md](../benchmarks/results/README.md)：性能结果的记录规则及复现要求，具体结果保存在同目录。

现有计划保留历史设计和验收依据。新增长篇执行证据按文档规范写入独立记录，性能表继续由结果目录维护。
