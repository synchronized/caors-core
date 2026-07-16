<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Events

本文档说明 CAORS 事件系统的当前设计边界。事件系统第一阶段提供轻量 `delegate`、同步
`signal`、连接视图、手动连接句柄、RAII 连接句柄和同步 `dispatcher`，用于资源重载通知、
编辑器工具回调和普通模块间同步通知。

## 设计目标

当前已覆盖：

- `events::delegate<R(Args...)>`：非拥有函数委托，支持自由函数、成员函数和函数对象左值。
- `events::signal<void(Args...)>`：按连接顺序同步广播多个监听者。
- `events::sink<void(Args...)>`：signal 的连接视图，只允许连接、断开和查询，不暴露 `emit()`。
- `events::connection`：手动断开一次 signal 订阅。
- `events::scoped_connection`：析构时自动断开订阅。
- `events::dispatcher`：按事件类型入队，在显式 `update()` 时同步批量派发。
- signal 内部监听者列表使用 CAORS allocator，构造 signal 时可传入 `memory_resource`。
- dispatcher 的事件队列、监听者列表和类型索引使用 CAORS allocator，构造 dispatcher 时可传入
  `memory_resource`。

当前不会包含：

- 跨线程同步。
- 自动持有对象生命周期。
- 拥有型 lambda / 函数对象存储。
- 后台异步调度、优先级队列或线程切换。

这些能力后续应在资源重载通知、对象生命周期和线程模型明确后逐步补充。

## 命名空间和头文件

事件模块定义位于 `caors::events` 命名空间：

- `events::delegate<Signature>`
- `events::signal<Signature>`
- `events::sink<Signature>`
- `events::connection`
- `events::scoped_connection`
- `events::dispatcher`

事件模块不再向根命名空间导出别名。事件、信号、连接和派发器统一通过
`caors::events::...` 使用，避免和对象、资源、任务模块的运行期组合件混在一起。

公共头文件：

- `caors/events/delegate.h`
- `caors/events/connection.h`
- `caors/events/signal.h`
- `caors/events/sink.h`
- `caors/events/dispatcher.h`
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
- `connection::release()` 和 `scoped_connection::release()` 只释放句柄所有权，不会主动断开订阅。
- emit 期间断开连接会延迟清理；emit 期间新增的监听者从下一次 emit 开始生效。
- 大对象参数建议使用 `const T&` 作为 signal 签名，避免多监听者广播时发生不必要拷贝。

## sink

`sink<void(Args...)>` 是 signal 的订阅入口视图。它适合把“允许外部订阅”的能力暴露出去，同时避免外部直接
触发事件：

```cpp
caors::events::signal<void(int)> changed;
caors::events::sink<void(int)> listeners{changed};

auto current = listeners.connect(listener);
changed.emit(10);
```

需要注意：

- sink 不拥有 signal，也不延长 signal 生命周期。
- sink 不提供 `emit()`；事件触发权仍保留在 signal 或 dispatcher 的拥有者手中。
- sink 的线程安全和生命周期约束与底层 signal 相同。

## dispatcher

`dispatcher` 是按事件类型存储的同步事件队列。监听者通过 `sink<Event>()` 订阅 `const Event&`：

```cpp
struct resource_reloaded {
  caors::resource_guid guid;
};

caors::events::dispatcher events;

auto connection = events.sink<resource_reloaded>().connect(on_resource_reloaded);
events.enqueue(resource_reloaded{texture_id});

events.update<resource_reloaded>(); // 派发这一类事件。
```

常用入口：

- `sink<Event>()`：获取指定事件类型的订阅视图。
- `enqueue<Event>(event)`：复制或移动事件到对应队列。
- `emplace<Event>(args...)`：在队列中原地构造事件。
- `trigger<Event>(event)`：立即同步触发事件，不经过队列。
- `update<Event>()`：派发指定事件类型当前已经入队的事件。
- `update()`：按事件类型首次使用顺序派发所有当前已经入队的事件。
- `clear<Event>()` / `clear()`：丢弃未派发事件。

需要注意：

- dispatcher 不是异步调度器；`update()` 在调用线程同步执行监听者。
- 第一版不做跨线程同步；跨线程入队、连接或派发时调用方需要自行加锁。
- `update<Event>()` 派发期间新增的同类事件会留到下一次 `update<Event>()`。
- 单个事件类型内部按入队顺序派发；`update()` 的跨类型顺序按事件类型首次使用顺序执行，不表达优先级。
- dispatcher 不拥有监听对象，也不延长对象生命周期。
- 事件类型 key 只用于当前进程内队列索引，不是稳定类型 ID，不能用于序列化或跨模块协议。

## 与资源系统的关系

资源系统已经可以在当前事件层上显式发出重载通知，例如：

```cpp
caors::resources::reload_signal<texture> reloaded;
auto result = caors::resources::reload_and_notify(cache, loader, texture_id, reloaded);
```

第一阶段资源 cache 仍只负责加载、替换和失败保留，不会隐式广播。`reload_and_notify()` 只是显式 helper：
调用方传入 signal，成功或失败都会收到一次同步事件。需要延迟批量处理时，可以把 reload 结果转换为
dispatcher 事件，在主循环或资源系统更新阶段统一 `update()`。是否刷新对象、重新解析引用、级联刷新依赖资源，
仍由资源管理器、资源依赖图或编辑器工具层决定。

相关测试位于 `tests/events/`，测试组织规则见 [tests/README.md](../tests/README.md)。
