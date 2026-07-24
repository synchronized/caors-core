<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta wrapper/model 行为内聚计划

## 背景

当前 `property_data`、`method_data` 已采用“运行时节点 + ops table + 模板 model”的类型擦除结构，
本质上与 RTTR 的 wrapper/model 分层一致，但部分运行时行为仍散落在公共 handle 实现和
`detail` 自由函数中，导致阅读时需要跨多个文件追踪一次属性访问或方法调用。

本计划不改为每个绑定对象单独分配虚基类实例，也不合并成单个巨型 `detail` 头文件。CAORS
继续使用普通内部对象和函数表，以保留静态存储、布局控制和较低分配成本。

## 目标结构

```text
公共只读 handle
property / method / method_set
        │
        ▼
运行时 wrapper 节点
property_data / method_data / method_set_data
        │
        ▼
类型擦除函数表
property_ops / method_ops
        │
        ▼
模板绑定 model
property_model.h / method_model.h
```

各层职责：

- 公共 handle：检查空值和公共对象状态，然后转发到内部 wrapper；
- wrapper 节点：保存名称、metadata、schema、参数和类型擦除状态，并完成运行时行为编排；
- ops table：描述 wrapper 与具体 C++ 绑定之间的最小调用协议；
- 模板 model：生成成员指针、访问器、全局变量、成员函数和 callable 对应的 ops；
- access 类：只负责从内部节点创建公共只读 handle，不暴露内部可变状态。

## 阶段

### 阶段 1：属性行为内聚（已完成）

- 将 `meta::value` 读取、赋值和精确类型回退移入 `property_data`；
- `property.cpp` 只保留公共对象有效性检查和转发；
- 保持 `property_model.h` 只负责生成具体 C++ 绑定；
- 保持 `property_accessor` 的提交期快速访问路径不变。

完成标准：属性运行时转换不再由 `property.cpp` 的匿名 namespace helper 编排。

实施结果：

- `property_data` 新增统一运行期值读取和写入成员函数；
- `meta::value` 快速转换、已知标量转换和精确类型回退已迁移到
  `src/caors/meta/detail/property_data.cpp`；
- `property.cpp` 仅检查公共 handle 与 instance 状态，然后转发给内部 wrapper；
- Clang Debug 的头文件自包含、属性、value 和序列化核心专项测试全部通过。

### 阶段 2：方法行为内聚

- 盘点 `method.cpp` 中可下沉到 `method_data`、`method_set_data` 的结果转换和重载调用逻辑；
- 将单方法调用编排收敛到 `method_data`；
- 将重载匹配后的调用编排收敛到 `method_set_data`；
- `method_model.h` 继续只实现具体 callable 调用。

完成标准：公共 method handle 只负责对象状态检查、参数入口适配和 wrapper 转发。

### 阶段 3：constructor 边界确认

- `constructor_ops` 当前是类型 capability，不单独创建重复的注册节点；
- 将参数匹配、构造和 owned value 生命周期继续内聚在 capability/ops；
- 只有出现多个独立 constructor 元数据节点需求时，才引入 `constructor_data`。

完成标准：明确 constructor 保持 capability 的理由，不为了目录对称增加无用抽象。

### 阶段 4：清理与验证

- 删除仅为旧式编排保留的无调用 helper；
- 运行 Clang、MSVC Debug 单线程构建与测试；
- 复测属性、方法、meta 查询和序列化 Release benchmark；
- 更新 meta 架构文档。

## 非目标

- 不复制 RTTR 的继承层次和虚函数 ABI；
- 不把所有内部实现放入同一个物理头文件；
- 不改变公共 API、注册语法或序列化格式；
- 不在本轮改变 capability 与 extension 的既定边界。
