<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 插件构建环境兼容性开发计划

## 目标

在插件进入 `initialize` 前比较 host 与 plugin 的编译环境，拒绝已知不兼容的 C++ ABI 组合，并给出
可诊断的错误。普通、不启用动态插件的 CAORS 静态构建继续允许 `/MT`。

第一版检查：

- CAORS runtime 主版本；
- 编译器前端及版本；
- C++ ABI 工具集版本；
- Windows CRT 模式：`/MD`、`/MDd`、`/MT`、`/MTd`；
- MSVC STL `_ITERATOR_DEBUG_LEVEL`；
- 指针宽度和处理器架构。

暂不增加 host allocator、owned buffer 或 byte view API；等出现首个跨 ABI 拥有型缓冲区需求时再设计。

## ABI 结构

新增可复用的 `caors_plugin_build_environment`。host 的
`caors_plugin_runtime_extension` 和 plugin 的 `caors_plugin_build_extension` 各保存一份环境描述。

host runtime extension 由 `plugin_host` 创建；plugin build extension 由插件使用公共编译期初始化宏创建，
并通过 `caors_plugin_entry::get_extension` 返回。所有结构保留 `struct_size` 和 `version`，新字段只追加。

## 加载顺序

```text
加载动态库
  → 查询 plugin entry
  → 校验 entry/descriptor ABI
  → 查询 caors.plugin.build
  → 比较 host runtime 与 plugin build
  → 兼容后才允许返回 loaded 状态和 initialize
```

缺失 build extension 或任一强约束不一致时返回 `plugins/inconsistent_type`。调用方可通过
`plugin_build_compatibility_diagnostic` 获得差异位集及 host/plugin 环境快照，不让异常跨越 C ABI。

## Windows CRT 策略

启用动态插件的官方目标必须统一使用动态 CRT：

- Debug：`MultiThreadedDebugDLL`（`/MDd`）；
- 其他配置：`MultiThreadedDLL`（`/MD`）。

host、plugin、CAORS 和静态链接到这些目标的库必须一致。CAORS 本身不全局禁止 `/MT`；不使用动态插件的
单体程序仍可选择静态 CRT。

## 实施阶段

### 阶段 1：构建环境描述

- [x] 增加枚举、环境结构和编译期检测宏；
- [x] host runtime extension 填充当前构建环境；
- [x] good/failure 测试插件发布 plugin build extension。

### 阶段 2：加载期校验

- [x] `plugin_binary::load` 在 initialize 前查询并比较 build extension；
- [x] 缺失或不兼容时拒绝加载；
- [x] 保留结构大小和扩展版本兼容检查。
- [x] 提供逐字段差异位集及双方构建环境快照。

### 阶段 3：构建约束与测试

- [x] 为官方插件测试目标统一设置动态 CRT；
- [x] 增加伪造不兼容环境插件；
- [x] 增加缺失 build extension 插件；
- [x] 覆盖编译器 ABI、CRT、iterator level、架构和指针宽度；
- [x] 更新正式插件 ABI 文档。

## 完成标准

- 兼容插件在 Windows Shared Debug/Release 正常加载；
- `/MD` 与 `/MDd`、动态与静态 CRT 混用会在 initialize 前被拒绝；
- 不兼容原因可定位；
- C ABI 结构不包含 STL 类型，不跨边界传播异常或转移隐式内存所有权。
