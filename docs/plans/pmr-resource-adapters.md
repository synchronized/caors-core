<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# `std::pmr` 内存资源适配计划

## 文档状态

- 当前状态：已完成；
- 目标：在不替换 CAORS 内存系统的前提下，提供 CAORS `memory_resource` 与
  `std::pmr::memory_resource` 的双向非拥有适配；
- 首批范围：资源分配、释放、等价性、失败语义、标准 PMR 容器和 CAORS 容器验证；
- 非目标：不统一两套默认资源、不引入 Boost、不迁移现有 CAORS 容器、不把 PMR 类型加入插件
  C ABI、不在首批提供拥有型适配器。

## 背景

CAORS 已经提供 `memory_resource`、`allocator<T>`、tracking、monotonic、stack 和 fixed block pool。
这些能力可以服务 CAORS 容器和普通标准容器，但不能直接传给 `std::pmr::vector`、
`std::pmr::string` 等 PMR 类型。反方向上，标准库和第三方组件提供的
`std::pmr::memory_resource` 也不能直接供 CAORS API 使用。

CAORS 资源还具有标准 PMR 没有的 `try_allocate()`、无异常构建和库内部资源策略，因此本计划只增加
互操作层，不用 PMR 替换现有抽象。

## 公共结构

新增独立公共头文件 `caors/memory/pmr.h`，并由 `caors/memory.h` 聚合：

```cpp
namespace caors::memory {

class pmr_resource_view final : public std::pmr::memory_resource {
public:
  explicit pmr_resource_view(memory_resource& resource) noexcept;
  [[nodiscard]] memory_resource& resource() const noexcept;
};

class caors_resource_view final : public memory_resource {
public:
  explicit caors_resource_view(std::pmr::memory_resource& resource) noexcept;
  [[nodiscard]] std::pmr::memory_resource& resource() const noexcept;
};

} // namespace caors::memory
```

两个类型都只保存底层资源指针，不获取所有权、不动态分配。使用 `view` 命名是为了显式提醒调用方：
底层资源和适配器本身必须比所有引用它们的 allocator、容器和对象活得更久。

## 语义约束

### CAORS 到 PMR

`pmr_resource_view`：

- `do_allocate()` 调用 CAORS `memory_resource::allocate()`；
- `do_deallocate()` 原样转发指针、标准化后的大小和对齐；
- `do_is_equal()` 只在另一对象也是 `pmr_resource_view`，且两个底层 CAORS 资源等价时返回
  `true`；
- 不修改 `std::pmr` 默认资源。

### PMR 到 CAORS

`caors_resource_view`：

- `do_try_allocate()` 调用 PMR resource 的 `allocate()`；
- 开启异常时捕获分配异常并返回 `nullptr`，保持 CAORS `try_allocate() noexcept` 契约；
- `do_deallocate()` 原样转发指针、大小和对齐；
- `do_is_equal()` 只在另一对象也是 `caors_resource_view`，且两个底层 PMR 资源等价时返回
  `true`；
- 不修改 CAORS 默认资源。

### 生命周期与模块边界

- view 不接受空资源，构造后始终引用有效资源；
- 容器保存的是资源裸指针，销毁容器前不得销毁 view 或底层资源；
- 插件卸载前必须销毁所有引用插件资源的 view、allocator、容器和分配结果；
- PMR 类型依赖 C++ 标准库 ABI，不得通过 CAORS 稳定插件 C ABI 传递；
- 适配器不增加锁，线程安全性完全继承底层资源。

## 实施阶段

### 阶段 0：冻结设计

- [x] 确认采用适配层而不是替换 CAORS `memory_resource`；
- [x] 确认双向 view 均为非拥有类型；
- [x] 明确默认资源、插件 ABI、线程安全和异常边界；
- [x] 明确不引入新的第三方依赖。

### 阶段 1：CAORS 到 PMR

- [x] 增加 `pmr_resource_view`；
- [x] 验证 `std::pmr::vector` 和 `std::pmr::string`；
- [x] 验证 tracking、大小、对齐和释放配对；
- [x] 验证相同与不同底层资源的等价性。

### 阶段 2：PMR 到 CAORS

- [x] 增加 `caors_resource_view`；
- [x] 验证 CAORS allocator 和容器可以使用标准 PMR resource；
- [x] 验证 PMR 分配失败转换为 `try_allocate()` 返回空；
- [x] 验证相同与不同底层资源的等价性。

### 阶段 3：文档与完整验证

- [x] 更新正式 Memory 文档和 roadmap；
- [x] 验证新增公共头文件可以单独包含；
- [x] 完成 Windows Static 全量构建测试和 Shared PMR 专项验证；
- [x] 记录本批不重复触发 Linux CI，后续由既有 Linux workflow 继续覆盖；
- [x] 将计划状态改为完成。

实施结果：

- `windows-clang-debug` 完整构建成功，82 项 CTest 全部通过；
- `windows-clang-shared-debug` 成功构建 CAORS DLL 和 PMR 测试目标，PMR 专项测试通过；
- 新增公共头文件已由 `caors.headers` 自包含目标单独编译；
- 本批没有触发耗时的完整 Linux Actions；适配器只依赖 C++20 标准 `<memory_resource>`，
  Linux Clang/GCC 继续由既有 CI 工作流覆盖。

## 测试清单

- PMR vector/string 的分配经过 CAORS tracking resource；
- CAORS vector/string 的分配经过 PMR 测试资源；
- 零尺寸请求沿用 CAORS 标准化行为；
- `alignas(64)` 等扩展对齐请求保持正确；
- 分配和释放的 size/alignment 配对；
- 同一底层资源的两个 view 等价；
- 不同 pool、stack 或 PMR resource 不等价；
- PMR `std::bad_alloc` 及其他异常不穿过 CAORS `try_allocate() noexcept`；
- 默认资源设置互不影响；
- Static/Shared 构建均无新增未解析符号或导出问题。

## 风险

- **生命周期悬空**：PMR allocator 和 CAORS allocator 都只保存资源指针。通过 `view` 命名、文档和
  测试明确销毁顺序，不提供返回临时 view 的便利函数。
- **异常语义不对称**：PMR 没有不抛异常分配入口。反向适配必须在开启异常时捕获异常并返回空。
- **资源等价性误判**：错误返回 `true` 会允许跨资源释放。实现采用保守的“相同适配类型且底层资源
  等价”规则。
- **跨 DLL/CRT**：适配器属于原生 C++ ABI，只允许在兼容工具链和仍处于加载状态的模块间使用，
  不进入稳定插件 ABI。
- **线程安全误解**：适配器不增加同步；`unsynchronized_pool_resource`、stack resource 等仍由调用方
  保证线程约束。
- **额外虚调用**：双层 resource 抽象会增加一次转发。互操作路径接受该成本，CAORS 内部热点不迁移到
  PMR。

## 完成标准

- 双向适配器均有清晰、可单独包含的公共 API；
- 标准 PMR 容器和 CAORS 容器均完成双向实际分配验证；
- 失败、对齐、释放配对和等价性测试通过；
- 正式文档说明所有权、线程安全、异常和 DLL 边界；
- 可用 Windows preset 的相关构建与测试通过，无法执行的平台验证明确记录。
