<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 内存域重构计划

## 文档状态

- 当前状态：方案设计与现状审计完成，等待分阶段实施；
- 目标：用明确的生命周期域统一 CAORS 内部长期元数据、registry layer、同步临时工作区和用户结果
  内存，消除依赖空指针回退与全局资源函数表达所有权的歧义；
- 首批范围：memory domain 契约、registry/type record 资源传播、临时工作区接入、结果资源边界、
  插件卸载与调试验证；
- 非目标：不替换底层 `memory_resource` 抽象，不强制用户采用 arena，不统一 CAORS 与
  `std::pmr` 的默认资源，不在首批更换系统分配器，也不把 C++ allocator 类型放入稳定插件 C ABI。

## 背景

CAORS 已经提供以下基础能力：

- `system_memory_resource` 与可替换的 `default_resource`；
- 内部长期数据使用的 `library_resource()`；
- `monotonic_resource`、`stack_resource`、`stack_scope` 与线程局部
  `library_temporary_resource()`；
- tracking、debug guard、fixed block pool、size class pool 和 PMR 适配；
- manager、container 和公共 API 的显式 `memory_resource*` 注入。

当前主要问题不是缺少 allocator，而是同一个 `memory_resource*` 在不同位置可能表示：

- 进程级内部元数据；
- registry 或插件 layer 的长期状态；
- 子系统拥有的对象；
- 返回给调用方的结果；
- 单次操作的临时工作区。

这些语义主要依赖函数名、空指针回退和调用约定，类型系统不能阻止资源使用错误。

## 当前结构审计

### 内部长期资源

`library_resource()` 默认固定返回 `system_memory_resource`，避免静态初始化期间捕获尚未配置的
`default_resource`。`string_name`、type identity、registry、reflection metadata 和 schema 等内部数据
广泛使用该入口。

这个默认策略本身合理，但 `library_resource()` 同时承担了两种不同语义：

1. 真正存活到进程结束的 identity 与 intern table；
2. 理论上从属于某个 registry/plugin layer、应当随 layer 销毁的数据。

### Registry 资源传播不完整

`registry_state` 和 `registry_layer_state` 已允许传入内部资源，外层 map、vector 和 state 对象会使用该
资源。但 `type_record`、`type_details`、property、method、enum value、metadata 等深层对象仍有多处
直接调用 `library_resource()`。

当前结构近似为：

```text
registry_layer(resource R)
├─ registry_state                    使用 R
├─ types/names/guids/cpp_types       使用 R
└─ type_record
   ├─ type_details                   使用 library_resource
   ├─ property/method/enum metadata  使用 library_resource
   └─ capability data                部分使用 R，部分使用全局资源
```

结果是 layer 的资源注入没有覆盖完整对象图，无法仅通过销毁 layer storage 回收该 layer 的全部数据，
也难以准确统计插件或 registry 的内存归属。

### 临时资源

`library_temporary_resource()` 返回线程局部复用的 `stack_resource`，`stack_scope` 通过 mark/rewind 支持
同步嵌套调用。MessagePack 和部分序列化事务已经区分结果资源与临时资源，但 JSON、meta 方法参数转换、
overload 候选和部分 registry 查询仍使用普通 `vector` 或默认资源。

### 用户与结果资源

用户可设置 `default_resource`，也可为 manager、container 和 API 显式传入资源。当前接口中名为
`resource` 的参数可能代表对象长期资源、返回值资源或内部临时资源，语义不统一。

## 设计原则

1. **生命周期先于分配策略**：先确定数据属于哪个域，再选择 system、pool、monotonic 或其他后端；
2. **所有权域随对象图传播**：父对象在某个域中，其拥有的动态子对象默认使用同一域；
3. **长期与临时分离**：返回值和长期对象不得引用 scratch domain；
4. **插件数据可卸载**：插件注册数据必须从属于可分离的 registry layer，不进入进程永久域；
5. **默认值只用于入口**：内部对象构造后保存解析完成的域或资源，不在析构时重新查询默认资源；
6. **跨域操作显式**：跨域 move、swap、adopt 和 release 必须复制、拒绝或明确声明 unsafe；
7. **资源对象生命周期可证明**：任何保存资源指针的对象都必须由同生命周期或更长生命周期的 owner
   管理；
8. **先保证正确性，再采用 arena**：不能用整池释放掩盖析构、插件 lease 或资源逃逸问题。

## 目标内存域

### Process domain

只保存真正跨 registry、跨插件并存活到 runtime 关闭的数据：

```text
process domain
├─ string intern identity
├─ type/capability identity state
├─ host runtime 基础状态
└─ 不引用插件代码的稳定诊断信息
```

建议入口命名为 `process_memory_resource()`，由 host/runtime 创建并固定。启动完成后不允许运行期替换。
现有 `library_resource()` 在迁移期保留为兼容入口，但新代码不得用它分配可卸载 layer 的成员数据。

Process domain 首批仍可使用 system resource。是否改为独立 monotonic 或 pool 资源应由实际统计决定，
不把“进程生命周期”与“必须使用 arena”绑定。

### Registry layer domain

每个 primary 或 plugin registry layer 拥有独立 storage：

```text
registry layer
└─ layer storage
   ├─ registry_state
   ├─ type_record/type_details
   ├─ property/method/enum/nested type
   ├─ metadata
   └─ dynamic capability data
```

规则：

- `type_record` 构造时必须接收并保存所属 layer resource；
- 所有动态子对象从该资源分配，不再直接调用 `library_resource()`；
- primary layer 使用 `pinned_process` 生命周期，但仍保持独立域，便于测试和统计；
- plugin layer 在停止新调用、等待 in-flight call 归零并析构 record 后释放全部 storage；
- static capability model 可以位于模块静态存储，但 layer 中不得留下已卸载插件的函数指针；
- layer arena 只是允许的实现，不是公共契约。首批可以继续逐项析构和释放，待验证后再切换为批量 storage。

建议内部结构：

```cpp
class registry_layer_storage final {
public:
  explicit registry_layer_storage(memory_resource& upstream);

  [[nodiscard]] memory_resource& resource() noexcept;

private:
  // 具体策略在实现阶段决定，可先使用 tracking/system，后续再评估 monotonic。
};
```

### Scratch domain

用于单次同步操作或严格嵌套子操作的临时工作区：

```text
scratch scope
├─ 参数规范化与转换
├─ overload 候选
├─ JSON/MessagePack frame
├─ 事务式反序列化暂存
└─ registry 查询临时快照
```

默认使用当前线程的 `library_temporary_resource()` 和 `stack_scope`。调用方可传入独立 scratch resource，
但 resource 必须支持本次操作所需的生命周期；异步任务不能默认借用发起线程的 TLS scratch。

规则：

- scratch 分配不得进入返回对象、registry、异步队列或插件持久状态；
- rewind 前先析构所有非平凡临时对象；
- 同步嵌套作用域严格 LIFO；
- 异步操作拥有独立 task/request arena，生命周期覆盖整个异步状态机；
- 跨 DLL 时优先使用 host 显式传入的 scratch 域，不依赖模块各自的 TLS 实例。

### Result/user domain

用于离开当前调用后仍然存活的数据：

```text
result/user domain
├─ decoded object 与 variant
├─ byte buffer/string/list
├─ object/asset/resource instance
└─ manager 或用户容器
```

由调用方或长期 owner 提供；未提供时在 API 入口解析 `default_resource` 并保存解析后的指针。返回对象的
析构始终使用创建时保存的资源，不重新查询当前默认资源。

## API 表达

### 长期 owner

manager、registry layer、filesystem、asset manager 等长期 owner 在构造时解析一次资源：

```cpp
class subsystem {
public:
  explicit subsystem(memory_resource& resource);
  [[nodiscard]] memory_resource& resource() noexcept;
};
```

如果保留可空指针 convenience overload，也只允许它在最外层委托构造中解析默认资源。

### 单次操作

复杂操作明确区分结果与临时内存：

```cpp
struct operation_memory {
  memory_resource& result;
  scratch_scope& scratch;
};

decode_result decode(input_stream& input, operation_memory memory);
```

简单同步 API 可提供 convenience overload，内部创建 scratch scope，但核心实现只接受显式
`operation_memory`。

### 跨域复制与移动

- 同域 move/swap 允许接管存储；
- 不同 result domain 之间默认逐元素移动或复制；
- scratch 到 result 必须 materialize/copy；
- registry layer 之间不得直接转移 record 指针；
- plugin layer 到 process domain 只允许复制不引用插件代码的稳定 identity/diagnostic 数据。

## 析构与 Arena 规则

批量释放 storage 不等于可以跳过所有析构：

- trivial 或明确声明 destructor-skippable 的对象可以只释放 arena；
- 持有锁、文件、shared ownership、plugin lease 或外部资源的对象必须运行析构；
- registry layer 销毁顺序固定为停止调用、等待 in-flight、析构对象图、释放 storage、卸载模块；
- arena 实现如需 destructor list，应由 layer storage 维护，不让每个调用点自行登记；
- Debug 模式记录域标识、generation 和 owner，检测跨域释放与 scratch 逃逸。

## 插件与 ABI 边界

- stable plugin ABI 继续传递 host allocator/resource extension，不传递 STL/PMR allocator 类型；
- host 创建 plugin registry layer domain，插件注册只能使用该域；
- 插件可请求 result 或 scratch 分配，但必须遵守 host 定义的释放协议；
- 插件卸载后仍存活的用户对象必须由 host-owned ops、复制后的数据或显式 lease 保证安全；
- 不允许插件把模块局部 TLS scratch、CRT heap 指针或插件 allocator 对象泄漏给 host；
- ABI 兼容检查继续要求同一平台、编译器、配置和 runtime 契约，内存域不能替代 ABI 校验。

## 与现有资源的映射

| 现有能力 | 目标归属 | 处理方式 |
|---|---|---|
| `system_memory_resource` | 后端策略 | 保留 |
| `default_resource` | 用户域入口默认值 | 保留，限制为启动/停机点切换 |
| `library_resource()` | process domain 兼容入口 | 逐步收窄语义 |
| `library_temporary_resource()` | thread scratch 默认实现 | 保留 |
| `stack_resource/stack_scope` | 同步 scratch | 保留并扩大接入 |
| `monotonic_resource` | registry/task arena 候选策略 | 不直接等同于某个域 |
| tracking/debug resources | 任意域的诊断装饰器 | 保留 |
| fixed/size class pool | 上游或子系统策略 | 由 workload 决定，不自动接入 |
| PMR views | 互操作边界 | 保留非拥有语义 |

## 实施阶段

### 阶段 0：冻结契约与建立审计

- [x] 为 process、registry layer、scratch、result/user 四个域补正式术语和生命周期表；
- [x] 枚举所有 `library_resource()`、默认资源和 temporary resource 调用点；
- [x] 为每个调用点标注 owner、释放时机、线程和插件边界；
- [x] 增加 Debug 域统计基线，不改变默认分配行为。

### 阶段 1：Registry 资源完整传播

- [x] `type_record` 保存所属 layer resource；
- [x] `type_details`、property、method、enum、metadata、nested type 与 dynamic capability 继承该资源；
- [x] 消除可卸载 record 路径中的直接 `library_resource()`；
- [x] 测试 primary 与 plugin layer 使用不同 tracking resource；
- [x] 验证 layer 销毁后其 tracking resource 无存活分配。

### 阶段 2：Process domain 收敛

- [x] 识别真正永久的 string/type/capability identity 数据；
- [x] 引入明确的 process resource/runtime owner；
- [x] 将 `library_resource()` 限制为兼容入口并阻止新滥用；
- [x] 支持测试创建隔离 runtime/context，避免所有测试依赖进程单例。

### 阶段 3：Scratch 接入

- [x] meta 方法参数准备、默认参数补全和转换接入 scratch scope；
- [ ] JSON reader/writer 与 DOM reader 的 frame/context 接入 scratch；
- [ ] registry 查询临时集合接入 scratch；
- [ ] 保证非平凡临时对象在 rewind 前析构；
- [ ] 增加嵌套调用、异常、提前返回和无异常构建测试。

### 阶段 4：Result 与 Scratch API 分离

- [ ] 统一 serialization、asset、IO 和 meta 操作中的 `result_resource` / `scratch` 术语；
- [ ] 核心实现使用显式 `operation_memory` 或等价结构；
- [ ] convenience API 只在边界解析默认资源和创建 scratch scope；
- [ ] 增加返回对象不引用 scratch 的 Debug 与功能测试。

### 阶段 5：Layer storage 策略评估

- [ ] 在 system、monotonic 和 pool 上测量 registry 注册、查询、卸载和内存占用；
- [ ] 确定 destructor list 或显式析构顺序；
- [ ] 只有在真实 type/plugin workload 获得收益后才采用 arena 批量 storage；
- [ ] 记录不同域的峰值、碎片、分配次数和释放延迟。

### 阶段 6：跨模块与全矩阵验证

- [ ] 覆盖 host、多个 plugin layer 和用户 result resource 并存；
- [ ] 覆盖插件卸载、in-flight call、异步 task 和 TLS 退出；
- [ ] 完成 Windows Clang/MSVC Static/Shared Debug/Release；
- [ ] 完成 Linux Clang/GCC、ASan，并在固定 Linux 环境补 UBSan；
- [ ] 更新正式 memory、meta、plugin 和 serialization 文档。

## 风险

- **迁移范围大**：resource 会贯穿 registry、record、metadata、method invocation 和 serialization；
- **对象尺寸增加**：record 保存 resource/domain 指针可能扩大热结构，应集中在 owner/details 而不是每个
  小对象重复保存；
- **错误批量释放**：arena rewind/free 不调用普通 C++ 析构，必须先定义 destructor 策略；
- **跨域复制成本**：正确的跨域操作可能把原本的浅 move 变成深复制；
- **插件卸载顺序**：record 析构或 callable 销毁若依赖插件代码，必须发生在卸载 DLL 前；
- **TLS 模块边界**：EXE 与 DLL 可能拥有不同 TLS 实例，不能把模块局部 scratch 当成进程共享状态；
- **默认资源兼容性**：现有用户可能依赖空指针回退，迁移期需要保留 convenience overload；
- **过早优化**：全局 arena 或 pool 可能降低可观测性并增加常驻内存，必须以统计和 workload 为依据。

## 完成标准

- 每类动态数据都能明确回答 owner、domain、释放时机和线程约束；
- plugin registry layer 的完整对象图从属于该 layer，卸载后无该域存活分配；
- process domain 不保存可卸载插件代码指针或插件私有对象；
- scratch 数据不会逃逸到返回值、异步状态和长期 registry；
- 结果对象始终保存并使用创建时解析的资源；
- 跨域 move/swap/adopt 语义有测试和正式文档；
- Debug 统计能按域报告分配、峰值和未释放对象；
- Windows、Linux、Static/Shared、插件卸载及 sanitizer 验证通过；
- arena 或 pool 的采用由真实 workload 数据支持，而不是由生命周期推断直接决定。

## 参考设计

- C++ `std::pmr`：提供 resource/allocator 机制，但由上层定义生命周期域；
- LLVM `BumpPtrAllocator`：适合 context/module 等同生命周期对象的批量 storage；
- Protobuf Arena：对象树共享 arena，并明确跨 arena move/swap 可能发生深复制；
- Unreal `FMemStack`/`FMemMark`：通过 mark/pop 管理同步临时工作区；
- CAORS 现有 `scoped-temporary-memory`、`registry-overlay-safe-unload` 与
  `library-plugin-system` 计划是本重构的直接约束来源。
