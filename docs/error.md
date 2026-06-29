<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Error

本文档说明 CAORS 的统一错误模型。当前错误层目标是为 meta、object、reflection 和后续
serialization 等模块提供一致的跨模块错误表达，同时保留现有模块枚举的源码兼容性。

## 设计目标

第一阶段解决几件事：

- 用 `caors::errc` 表达跨模块通用错误码。
- 用 `caors::error_domain` 保留错误来源模块。
- 用 `caors::error` 表达轻量错误状态，不分配动态消息。
- 让 `caors::errc` 可以转换为 `std::error_code`，方便接入标准库生态。
- 为已有注册结果枚举提供 `to_error()` 转换函数。

当前不会包含：

- 动态错误消息。
- 调用栈或诊断上下文。
- `expected<T, E>` 风格返回值封装。
- 对现有 `*_register_result` 枚举的破坏性替换。

这些能力可以在 serialization MVP 和跨模块 API 稳定后再评估。

## 核心概念

### errc

`errc` 是 CAORS 的通用错误码，类似 `std::errc` 的项目内版本：

```cpp
std::error_code code = caors::errc::invalid_type;
```

转换后的 `std::error_code` 使用 `caors::error_category()`。这适合和标准库、日志系统或上层工具链
互操作。

### error_domain

`error_domain` 描述错误来源：

```cpp
caors::error error{caors::error_domain::reflection, caors::errc::invalid_field};
```

同一个通用错误码在不同模块可能有不同含义。比如 `duplicate_name` 可能来自基础类型注册，也可能
来自对象类型或反射类型注册。`error_domain` 用来保留这层边界。

### error

`error` 是不分配内存的轻量状态对象：

```cpp
caors::error status = caors::to_error(result);

if (status.failed()) {
  std::error_code code = status.code();
}
```

默认构造的 `error` 表示成功。推荐使用 `succeeded()` / `failed()` 判断状态，避免把错误状态和
指针、句柄的布尔语义混在一起。

### to_error

现有模块结果枚举继续保留，并提供转换函数：

```cpp
caors::type_register_result result = caors::register_type<actor>("game.actor");
caors::error status = caors::to_error(result);
```

当前已提供转换：

- `to_error(type_register_result)`
- `to_error(meta_component_result)`
- `to_error(object_register_result)`
- `to_error(reflection_register_result)`

## 后续方向

- serialization 等新模块优先返回或暴露统一 `caors::error`。
- 需要携带值的 API 可以评估引入 `result<T>` / `expected<T, error>`。
- 需要详细诊断的工具链 API 可以在 `error` 之外增加诊断对象，避免让底层热路径默认承担分配成本。

## 相关入口

- 公共头文件：`include/caors/error.h`。
- 行为测试：`tests/error/`。
