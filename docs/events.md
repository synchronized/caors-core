<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Events

本文档说明 CAORS 事件系统的当前设计边界。事件系统第一阶段提供轻量 `delegate`、同步
`signal`、手动连接句柄和 RAII 连接句柄，用于资源重载通知、编辑器工具回调和普通模块间同步通知。

## 设计目标

当前已覆盖：

- `events::delegate<R(Args...)>`：非拥有函数委托，支持自由函数、成员函数和函数对象左值。
- `events::signal<void(Args...)>`：按连接顺序同步广播多个监听者。
- `events::connection`：手动断开一次 signal 订阅。
- `events::scoped_connection`：析构时自动断开订阅。
- signal 内部监听者列表使用 CAORS allocator，构造 signal 时可传入 `memory_resource`。

当前不会包含：

- 异步事件队列。
- 跨线程同步。
- 自动持有对象生命周期。
- 拥有型 lambda / 函数对象存储。
- 按事件类型入队和批量派发的 dispatcher。

这些能力后续应在资源重载通知、对象生命周期和线程模型明确后逐步补充。

## 命名空间和头文件

事件模块定义位于 `caors::events` 命名空间：

- `events::delegate<Signature>`
- `events::signal<Signature>`
- `events::connection`
- `events::scoped_connection`

根命名空间保留常用别名：

- `caors::delegate<Signature>`
- `caors::signal<Signature>`
- `caors::event_connection`
- `caors::scoped_event_connection`

公共头文件：

- `caors/events/delegate.h`
- `caors/events/connection.h`
- `caors/events/signal.h`
- `caors/events.h`

## delegate

`delegate<R(Args...)>` 是非拥有函数委托。它只保存一个类型擦除调用入口和一个可选对象指针，不分配内存。

自由函数：

```cpp
int double_value(int value);

caors::events::delegate<int(int)> callback;
callback.connect<&double_value>();

int result = callback(7);
```

成员函数：

```cpp
struct listener {
  void on_value(int value);
};

listener target;
caors::events::delegate<void(int)> callback;
callback.connect<&listener::on_value>(&target);
callback(42);
```

函数对象左值：

```cpp
int total = 0;
auto add = [&](int value) {
  total += value;
};

caors::events::delegate<void(int)> callback;
callback.connect(add);
callback(3);
```

需要注意：

- 绑定成员函数或函数对象时，目标对象必须比 delegate 活得更久。
- 函数对象必须是左值，delegate 不复制、不拥有，也不延长其生命周期。
- 未绑定 delegate 后调用属于调用方错误，调试构建会触发断言。

## signal

`signal<void(Args...)>` 是同步广播信号。第一版只支持 `void` 返回值，因为多个监听者的返回值没有统一语义。

```cpp
caors::events::signal<void(int)> changed;

auto listener = [](int value) {
  // 同步处理事件。
};

caors::events::connection current = changed.connect(listener);
changed.emit(10);
current.disconnect();
```

成员函数也可以直接连接：

```cpp
listener target;
caors::events::connection current =
    changed.connect<&listener::on_value>(&target);
```

`scoped_connection` 用于作用域内自动断开：

```cpp
{
  caors::events::scoped_connection scoped{changed.connect(listener)};
  changed.emit(10);
}

changed.emit(20); // listener 已断开。
```

需要注意：

- signal 按连接顺序同步调用监听者。
- signal 不做线程同步；跨线程连接、断开或 emit 时调用方需要自行加锁。
- signal 不拥有监听对象，也不延长对象生命周期。
- `connection` 和 `scoped_connection` 必须在对应 signal 存活期间使用。
- emit 期间断开连接会延迟清理；emit 期间新增的监听者从下一次 emit 开始生效。
- 大对象参数建议使用 `const T&` 作为 signal 签名，避免多监听者广播时发生不必要拷贝。

## 与资源系统的关系

资源系统后续可以在当前事件层上增加重载通知，例如：

```cpp
caors::events::signal<void(caors::resource_guid)> reloaded;
```

第一阶段资源 cache 仍只负责加载、替换和失败保留，不会隐式广播。是否通知、通知哪些上层对象、是否级联刷新
应由后续资源管理器、资源依赖图或编辑器工具层显式接入。

相关测试位于 `tests/events/`，测试组织规则见 [tests/README.md](../tests/README.md)。
