<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Error

本文档说明 CAORS 的统一错误模型。当前错误层目标是为 io、meta、object、resources 和
serialization 等模块提供一致的跨模块错误表达。

## 设计目标

第一阶段解决几件事：

- 用 `caors::errc` 表达跨模块通用错误码。
- 用 `caors::error_domain` 保留错误来源模块。
- 用 `caors::error` 表达轻量错误状态，不分配动态消息。
- 用 `caors::result<T>` 表达携带值的轻量返回结果。
- 用 `caors::result<void>` 表达需要统一 `result` 流程但没有返回值的结果。
- 让 `caors::errc` 可以转换为 `std::error_code`，方便接入标准库生态。

当前不会包含：

- 动态错误消息。
- 调用栈或诊断上下文。
- 需要抛异常的返回值封装。

这些能力可以在序列化和跨模块 API 稳定后再评估。

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
caors::error error{caors::error_domain::meta, caors::errc::invalid_field};
```

同一个通用错误码在不同模块可能有不同含义。比如 `duplicate_name` 可能来自 meta 类型注册，也可能
来自对象类型或序列化 schema 注册。`error_domain` 用来保留这层边界。

### error

`error` 是不分配内存的轻量状态对象。布尔上下文中 `true` 表示失败，`false` 表示成功：

```cpp
caors::error status = caors::meta::class_<actor>("game.actor").result();

if (status) {
  std::error_code code = status.code();
}
```

默认构造的 `error` 表示成功。`succeeded()` / `failed()` 仍可用于需要显式表达状态的代码。

### 公共注册 API

公共注册 API 统一返回 `caors::error`：

```cpp
caors::error status = caors::meta::class_<actor>("game.actor").result();

if (status.domain() == caors::error_domain::meta &&
    status.value() == caors::errc::duplicate_name) {
  // 处理 meta 类型重名。
}
```

重复注册同一个稳定名称会返回 `errc::duplicate_name`：

```cpp
caors::meta::class_<actor>("game.actor");
caors::error status = caors::meta::class_<enemy>("game.actor").result();
// status.value() == caors::errc::duplicate_name
```

异步或跨阶段 API 可以用 `errc::pending` 表示结果尚未完成，用 `errc::cancelled`
表示调用方已取消请求：

```cpp
auto result = request.result();
if (result.error().value() == caors::errc::pending) {
  // 稍后再查询。
}
```

依赖图、预加载、打包等需要拓扑顺序的流程可以用 `errc::cycle_detected`
表示发现循环依赖：

```cpp
auto plan = database.plan_preload<texture>(texture_id);
if (plan.error().value() == caors::errc::cycle_detected) {
  // plan.cycle 保存一个闭合依赖路径。
}
```

序列化按 inline value 展开对象、容器、指针和 wrapper 时，如果超过
`serialization::context::limits.max_depth`，会返回 `errc::recursion_limit`：

```cpp
caors::serialization::context context{};
context.config.limits.max_depth = 8;

auto result = caors::to_string(context, node);
if (result.error().value() == caors::errc::recursion_limit) {
  // 检查是否存在自环、父指针或过深嵌套。
}
```

### rethrow_as

跨模块 API 边界可以用 `rethrow_as()` 保留底层错误码，并替换为当前模块的错误来源域：

```cpp
caors::error status = meta.emplace_extension<serialization::type_schema>(id, data);
return caors::rethrow_as(status, caors::error_domain::serialization);
```

如果底层错误码在当前模块有更准确的对外语义，可以只声明这一处差异：

```cpp
return caors::rethrow_as(status,
                         caors::error_domain::serialization,
                         caors::errc::duplicate_component,
                         caors::errc::duplicate_field);
```

### result

需要同时返回错误状态和值的 API 可以使用 `caors::result<T>`：

```cpp
caors::result<caors::string> result = caors::ok(caors::string{"text"});

if (result.has_value()) {
  caors::string text = *result;
}
```

失败结果可以用 `fail<T>()` 构造，避免调用点手写 `{status, value}` 聚合初始化：

```cpp
caors::result<caors::string> result =
    caors::fail<caors::string>(
        caors::error{caors::error_domain::serialization, caors::errc::invalid_type});
```

`result<T>` 提供接近 `expected<T, error>` 的访问方式：

- `has_value()` / `operator bool()`：是否成功并拥有可用值。
- `error()`：失败状态，返回 `caors::error`。
- `operator*` / `operator->`：访问结果值。
- `value_or(fallback)`：失败时返回备用值。

`result<T>` 目前仍保留 `status` 和 `value` 聚合字段，方便内部和历史调用点逐步迁移。新代码优先使用
上面的访问器，避免继续扩散字段名依赖。布尔上下文中 `true` 表示成功且有可用值，
这和 `caors::error` 的布尔语义相反：`error` 的 `true` 表示失败。

### result<void>

`result<void>` 用于没有返回值、但需要和 `result<T>` 保持同一套流程语义的 API：

```cpp
caors::result<void> status = caors::ok();

if (!status) {
  return status.error();
}
```

如果一个 API 只表达“成功或失败”，没有结果值、没有后续组合流程，仍优先直接返回 `caors::error`。
`result<void>` 更适合需要和泛型 `result<T>` 统一处理的场景，例如批处理步骤、异步流程节点或后续可能扩展值结果的 API。

## 后续方向

- 新模块和新的公共注册入口优先返回或暴露统一 `caors::error`。
- `result<T>` 后续可继续评估是否隐藏聚合字段、增加 `and_then` / `transform` 等组合操作。
- 需要详细诊断的工具链 API 可以在 `error` 之外增加诊断对象，避免让底层热路径默认承担分配成本。

## 相关入口

- 公共头文件：`include/caors/error.h`。
- 行为测试：`tests/error/`。
