<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# small_vector 开发计划

## 文档状态

- 当前状态：阶段 2 实施中；
- 目标：提供带对象内联存储、溢出后使用 CAORS `memory_resource` 的连续顺序容器；
- 首批范围：核心容器语义、异常安全、资源分配、meta/serialization 适配、测试和代表性基准；
- 非目标：首批不实现 `stable_vector`，不保证元素地址稳定，不提供用户自定义增长策略、容量整数类型或
  内联区额外对齐选项，也不把容器对象直接放入稳定 C ABI 或插件 ABI。

## 背景

`core::vector<T>` 当前只是 `std::vector<T, memory::allocator<T>>` 的别名。它能够统一动态分配来源，
但即使绝大多数实例只保存少量元素，也需要单独的动态存储。临时工作集、参数列表、小型依赖列表和
解析中间结果适合使用 small-buffer optimization，在未超过常见元素数量时避免分配。

LLVM `SmallVector`、Boost.Container `small_vector` 和 Abseil `InlinedVector` 都采用对象内联存储，
超过内联容量后转入动态存储。LLVM 建议只读接口优先接受 `ArrayRef`，而不是把 inline capacity 写入
参数类型；CAORS 对应使用 `std::span`。Boost 提供不依赖内联容量的 `small_vector_base` 和丰富的增长、
对齐配置，但首版引入这类层次会扩大 API、实现和 ABI 面积，因此暂不采用。

参考资料：

- [LLVM Programmer's Manual：SmallVector](https://llvm.org/docs/ProgrammersManual.html#llvm-adt-smallvector-h)；
- [Boost.Container：small_vector](https://www.boost.org/doc/libs/latest/doc/html/container/non_standard_containers.html#container.non_standard_containers.small_vector)；
- [Abseil 容器指南](https://abseil.io/docs/cpp/guides/container)。

## 核心决策

### 1. 公共类型

首版公共类型为：

```cpp
template <typename T, std::size_t InlineCapacity>
class small_vector;
```

类型位于 `caors::core`，并在根命名空间提供高频别名：

```cpp
template <typename T, std::size_t InlineCapacity>
using small_vector = core::small_vector<T, InlineCapacity>;
```

`InlineCapacity` 是类型的一部分。只读公共接口优先接受 `std::span<const T>`，可写但不改变容量的接口
优先接受 `std::span<T>`。首版不提供容量擦除的可变基类，避免公开一套只能作为引用使用的第二容器 API。

### 2. 分配与所有权

- 对象持有一个非空、非拥有的 `memory_resource*`；传入空指针时捕获当前默认资源；
- `size() <= InlineCapacity` 且未发生必要的外置保留时，元素位于对象内联区，不调用上游资源；
- 超过内联容量后，动态存储使用 `memory::allocator<T>`；
- 析构时只销毁已构造元素，并仅释放动态存储；
- 容器不可跨越资源或插件生命周期存活，约束与现有 CAORS allocator 容器一致。

首版固定动态 allocator 类型，不增加第三个 `Allocator` 模板参数。这样能保持 CAORS 资源模型统一，
减少类型组合，并让 meta/serialization 能稳定识别该容器。

### 3. 表示与容量

建议表示包含：

- 当前数据指针；
- 当前 size 与 capacity；
- `memory_resource*`；
- `alignas(T)` 的原始内联存储。

必须支持 `InlineCapacity == 0`，但不能为零容量实例实际构造一个 `T`。容量增长采用摊还常数复杂度的
指数策略，并在乘法、加法和 `sizeof(T) * count` 前检查溢出。首版不承诺精确增长倍率，只承诺新容量
不小于请求值和 `max_size()` 契约。

`shrink_to_fit()` 是非绑定请求：动态元素能够安全迁回内联区时允许迁回，否则保持当前容量。无论是否
迁回，元素引用和迭代器失效规则都按可能重分配处理。

### 4. 元素类型与异常安全

- 支持非平凡析构、只移动类型和 uses-allocator 元素；
- 元素构造统一通过 CAORS allocator 的 `construct()`，使嵌套资源感知类型获得相同资源；
- 原地尾部构造失败时保持原 size；
- 需要新存储的操作先完成新区域构造，再提交 data/size/capacity；
- 当 `T` 可复制但移动可能抛出时，优先复制以维持强异常保证；
- 当 `T` 只可抛出移动且不可复制时，与 `std::vector` 类似，只提供基本保证并在文档中明确；
- 无异常构建沿用项目现有失败策略，不单独引入 `result` 版容器 API。

### 5. 移动、复制与资源语义

- 复制构造默认使用源容器资源，另提供显式资源构造重载；
- 移动构造在源使用动态存储时可接管缓冲区；源使用内联存储时逐元素移动；
- 带不同资源的移动构造不得接管原缓冲区，必须迁移元素；
- 移动赋值遵循 CAORS allocator 的传播语义；接管前必须正确销毁目标原元素和动态存储；
- `swap()` 在双方资源和存储状态不同的情况下不能简单交换数据指针，首版必须覆盖 inline/inline、
  inline/heap、heap/heap 与异资源组合。

### 6. 首版 API

首版目标覆盖常用 vector 语义，而不是一次复制全部标准重载：

- 构造、析构、复制、移动、initializer list、迭代区间和显式资源构造；
- `begin/end`、`data`、`size/capacity/empty/max_size`；
- `operator[]`、`at`、`front/back`；
- `reserve`、`shrink_to_fit`、`resize`；
- `emplace_back`、`push_back`、`pop_back`；
- `emplace/insert/erase` 的单元素与基础区间形式；
- `clear`、`assign`、`swap`；
- 相等比较和三路比较在元素类型支持时可用。

首版不提供非标准的 unchecked API、默认未初始化扩容或 POD 专用接口。平凡类型批量移动优化只作为
实现细节，不能改变生命周期语义。

## Meta 与序列化

新增 `sequence_traits<core::small_vector<T, N>>`：

- `type_name_prefix()` 使用 `caors.small_vector`；
- 类型身份必须包含元素类型和 `InlineCapacity`；
- 暴露 size、element、mutable_element、clear、resize 和 append；
- 带资源 resize 时，容器自身保持原资源，新增资源感知元素使用容器资源，不能临时改用 reader context
  中另一个资源。

序列化格式只表达元素序列，不保存 inline capacity、当前 capacity 或当前存储模式。因此相同元素内容的
`small_vector<T, 4>` 与普通 vector 具有相同的数据形态，但仍是不同的运行时类型。

## 文件与模块组织

计划新增：

- `include/caors/core/small_vector.h`：header-only 模板实现；
- `include/caors/core/containers.h`：聚合、别名和 `make_small_vector`；
- `include/caors/adapters/small_vector_traits.h`：meta 顺序容器适配；
- `tests/core/containers/small_vector_test.cpp`：容器语义与资源测试；
- `tests/serialization/container/small_vector_test.cpp`：反射与序列化测试；
- `benchmarks/core/small_vector_bench.cpp`：小规模构造、追加、溢出和低命中 workload。

适配头是否由现有 STL adapter 聚合头继续导出，在实施阶段依据当前 include boundary 决定；不能让
`core/small_vector.h` 反向依赖 meta 或 serialization。

## 测试矩阵

### 容量与生命周期

- 0、1、`InlineCapacity`、`InlineCapacity + 1` 和多次增长；
- 内联到动态、动态保持、动态迁回内联；
- 平凡类型、非平凡析构类型、只移动类型、过对齐类型；
- 构造、复制、移动和析构计数平衡；
- `InlineCapacity == 0` 与超大容量/字节数溢出。

### 资源与异常

- 内联范围内上游分配次数为零；
- 溢出分配和释放的 size/alignment 匹配；
- 默认资源、显式资源、同资源和异资源移动；
- 分配失败、复制失败和移动失败注入；
- 嵌套资源感知元素获得容器资源；
- ASan 下切换存储模式无 use-after-free、越界或泄漏。

### 迭代器与修改

- 插入、擦除、resize、reserve、swap 的内容和失效边界；
- 自身区间插入、重叠移动和别名参数；
- 空容器访问的前置条件；
- 与 `std::span`、ranges 算法和连续迭代器要求兼容。

### Meta 与序列化

- capability 自动发现、类型名称和不同 inline capacity 的类型身份；
- JSON/MessagePack 往返；
- 空、内联、溢出三种状态产生一致的元素序列格式；
- 反序列化资源传播与失败回滚。

## 基准与采用门槛

至少对比：

- `core::small_vector<T, N>`；
- `core::vector<T>`；
- `std::array<T, N>` 加独立 size（仅固定上限场景）；
- 可用时作为人工参考的 LLVM/Boost 实现，但不把第三方依赖加入正式构建。

workload 覆盖：始终内联、偶尔溢出、频繁溢出、大元素、非平凡元素、批量创建与销毁。记录耗时、
分配次数、对象大小和峰值容量。只有已知元素数量分布且内联命中率足够高时才采用；不能把
`small_vector` 机械替换所有 `core::vector`。

## 风险

1. **对象膨胀**：内联容量直接增加 `sizeof(container)`，大量长生命周期实例可能浪费更多内存；
2. **代码膨胀**：`T` 与 `InlineCapacity` 共同参与模板实例化，完整 STL 重载会放大编译时间和体积；
3. **复杂异常路径**：inline/heap 切换、插入和异资源移动容易造成重复析构或泄漏；
4. **地址误用**：从内联区溢出后元素地址仍会失效，不能把它误认为 stable container；
5. **ABI 风险**：对象布局依赖 `T`、容量和实现版本，不能跨稳定 C ABI 按值传递；
6. **收益不稳定**：低内联命中、大元素或频繁溢出时，额外分支和对象体积可能劣于普通 vector。

## 实施阶段

### 阶段 0：布局与最小骨架（已完成）

- [x] 固定公共类型、资源构造和 inline/heap 状态不变量；
- [x] 实现空容器、析构、访问器和连续迭代器；
- [x] 用静态断言与测试覆盖对齐、容量 0 和对象布局边界。

### 阶段 1：增长与生命周期（已完成）

- [x] 实现 reserve、emplace_back、push/pop、clear 和 resize；
- [x] 实现溢出检查、增长策略和内联/动态迁移；
- [x] 覆盖平凡、非平凡、只移动和资源感知元素。

### 阶段 2：完整值语义与修改操作（已完成）

- [x] 实现复制、移动、赋值与显式异资源构造；
- [x] 实现 swap；
- [x] 实现 insert、erase 和 assign；
- [x] 实现 shrink_to_fit；
- [x] 完成异常注入与别名/自身区间测试。

### 阶段 3：Meta、序列化与文档（已完成）

- [x] 增加 sequence traits 和类型身份；
- [x] 完成 JSON/MessagePack 往返及资源传播测试；
- [x] 更新聚合头、`docs/core.md` 或相关正式容器文档和 roadmap。

### 阶段 4：基准与跨平台验证

- [x] 增加代表性 benchmark 并记录 Windows 结果；
- [ ] 完成 Windows Clang/MSVC Debug、Release、Static/Shared；
- [ ] 使用 GitHub Actions 完成 Linux Clang/GCC 与 ASan；
- [ ] 固定 Linux 环境完成正式性能复验后再选择首批实际接入点。

## 完成标准

- 内联范围无动态分配，溢出与释放完全遵守 CAORS resource 契约；
- vector-like 常用 API、异常保证、迭代器失效和 ABI 限制均有文档与测试；
- meta/serialization 不泄漏存储模式，资源传播可解释；
- Windows 与 Linux 正确性、ASan 验证通过；
- 代表性 small workload 显示明确收益，并记录不适用场景；
- 在真实使用点验证前不批量替换现有 `core::vector`。

## 当前验证记录

- 2026-08-04：完成阶段 0 header-only 骨架；`windows-clang-debug` 下
  `caors.core.containers` 定向测试通过；
- 2026-08-04：新增公共头及聚合头完成 222 项公共头自包含编译，`caors.headers` 测试通过；
- 2026-08-04：完成阶段 1 的增长与生命周期操作；覆盖容量 0、内联命中、动态溢出、过对齐、
  非平凡生命周期、只移动、资源感知元素、别名尾部追加和复制失败回滚；
- 2026-08-04：完成阶段 2 第一批值语义；复制赋值保留目标资源，移动赋值传播源资源，同资源动态
  缓冲区允许接管，异资源逐元素迁移，并覆盖动态存储精确收缩和迁回内联区；
- 2026-08-04：完成阶段 2 修改操作；增加初始化列表和区间构造、assign、insert、erase、swap 及内容
  比较，覆盖自身区间插入/赋值、只移动元素与 inline/heap 异资源交换；
- 2026-08-04：完成阶段 3；类型身份包含内联容量，顺序容器能力及 JSON/MessagePack 往返验证通过，
  反序列化保持容器资源并向嵌套资源感知元素传播；
- 2026-08-04：完成 Windows Clang Release microbenchmark；内联和偶尔溢出工作集收益明确，频繁溢出
  相比精确 reserve 明显更慢，因此继续禁止未经真实 workload 验证的批量替换；
- 2026-08-04：Windows Clang 静态 Debug、Release 全量构建及 83 项测试通过；Windows MSVC 静态
  Debug 全量构建及 83 项测试通过；MSVC Release 与 Shared 组合待后续批次验证；
- `windows-clang-debug` 下 `caors.core.containers`、`caors.meta.sequence` 和
  `caors.serialization.container` 定向测试通过。
