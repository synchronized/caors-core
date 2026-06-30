<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Error

本文档说明 CAORS 的统一错误模型。当前错误层目标是为 meta、object、reflection 和
serialization 等模块提供一致的跨模块错误表达。

## 设计目标

第一阶段解决几件事：

- 用 `caors::errc` 表达跨模块通用错误码。
- 用 `caors::error_domain` 保留错误来源模块。
- 用 `caors::error` 表达轻量错误状态，不分配动态消息。
- 用 `caors::result<T>` 表达携带值的轻量返回结果。
- 让 `caors::errc` 可以转换为 `std::error_code`，方便接入标准库生态。

当前不会包含：

- 动态错误消息。
- 调用栈或诊断上下文。
- 完整 `expected<T, E>` 风格返回值封装。

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

`error` 是不分配内存的轻量状态对象。布尔上下文中 `true` 表示失败，`false` 表示成功：

```cpp
caors::error status = caors::register_type<actor>("game.actor");

if (status) {
  std::error_code code = status.code();
}
```

默认构造的 `error` 表示成功。`succeeded()` / `failed()` 仍可用于需要显式表达状态的代码。

### 公共注册 API

公共注册 API 统一返回 `caors::error`：

```cpp
caors::error status = caors::register_type<actor>("game.actor");

if (status.domain() == caors::error_domain::meta &&
    status.value() == caors::errc::duplicate_name) {
  // 处理基础类型重名。
}
```

### rethrow_as

跨模块 API 边界可以用 `rethrow_as()` 保留底层错误码，并替换为当前模块的错误来源域：

```cpp
caors::error status = meta.emplace<serialization_type_data>(id, data);
return caors::rethrow_as(status, caors::error_domain::serialization);
```

如果底层错误码在当前模块有更准确的对外语义，可以只声明这一处差异：

```cpp
return caors::rethrow_as(status,
                         caors::error_domain::reflection,
                         caors::errc::duplicate_component,
                         caors::errc::duplicate_reflection);
```

### result

需要同时返回错误状态和值的 API 可以使用 `caors::result<T>`：

```cpp
caors::result<caors::string> result = caors::serialize_text(instance);

if (result) {
  caors::string text = result.value;
}
```

`result<T>` 内部保存 `status` 和 `value`。布尔上下文中 `true` 表示成功且有可用值，
这和 `caors::error` 的布尔语义相反：`error` 的 `true` 表示失败。

## 后续方向

- 新模块和新的公共注册入口优先返回或暴露统一 `caors::error`。
- 需要更严格的值访问语义时，可以评估把轻量 `result<T>` 升级为完整
  `expected<T, error>` 风格封装。
- 需要详细诊断的工具链 API 可以在 `error` 之外增加诊断对象，避免让底层热路径默认承担分配成本。

## 相关入口

- 公共头文件：`include/caors/error.h`。
- 行为测试：`tests/error/`。
