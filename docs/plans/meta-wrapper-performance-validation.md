<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Meta Wrapper 性能复验计划

## 文档状态

- 当前状态：阶段 0 至阶段 2 已完成，阶段 3 已完成 Windows Clang 正式执行；
- 上游计划：[meta-erased-wrapper-redesign.md](meta-erased-wrapper-redesign.md)；
- 目标：量化 property/method wrapper 迁移后的布局、注册分配和运行时调用成本，并据此决定是否优化；
- 非目标：不为追求形式统一迁移 constructor、type model 或 builtin ops，不把单次微基准直接解释为真实
  应用收益。

## 背景

property 和 method 已从固定 record/ops 与内联模板状态迁移为 move-only opaque wrapper。现有
`caors_benchmarks` 已覆盖 typed、`meta::value`、`any`、按名访问和缓存句柄等运行时路径，但尚未
完整回答：

- `property_record`、`method_record` 和 wrapper 的当前尺寸及对齐；
- member、accessor、global property 和不同 method model 的注册分配成本；
- request dispatch 相对迁移前实现增加了多少成本；
- compiled member accessor 是否已经绕过主要热路径开销；
- 是否存在足以支持小对象优化、无状态 model 或其他结构调整的证据。

## 测量原则

### 固定环境

正式结果使用 Release benchmark preset，同一机器、同一编译器、同一迭代次数至少运行三次并取
中位数。Windows Clang 和 Linux Clang 各保存一份结果；GCC 只做构建与冒烟，不作为首批性能门禁。

每份摘要记录：

- 提交号、操作系统、CPU 和可用内存；
- CMake、编译器、标准库和 linker；
- preset、迭代次数和重复次数；
- 是否启用 Shared、ASan、调试迭代器或其他会改变 ABI/性能的选项。

ASan、Debug 与 Release 结果不得横向比较。机器或工具链变化后只建立新基线，不与旧摘要计算回归比例。

### 防止失真

- 热路径预先缓存 `meta::type`、property、method 和参数容器；
- 按名便利 API 单独测量，不能混入 wrapper dispatch 用例；
- 直接 C++ 调用作为下界，`any` / `value` 转换成本单独列出；
- 注册分配统计使用新建局部 registry 和固定数量记录，排除首次 `string_name` intern、全局静态初始化及
  benchmark 输出分配；
- 每个 timed loop 外完成注册、查找和输入准备；
- 保留结果消费，避免编译器删除被测操作。

## 指标与用例

### 布局样本

在 benchmark 输出独立的 `Meta wrapper layout samples`：

- `sizeof` / `alignof(property_wrapper)`；
- `sizeof` / `alignof(property_record)`；
- `sizeof` / `alignof(method_wrapper)`；
- `sizeof` / `alignof(method_record)`；
- `sizeof(method_wrapper_state)`；
- 公共 `meta::property`、`meta::method`、`property_accessor` 句柄尺寸。

布局样本属于 ABI 与内存密度诊断，不作为耗时 benchmark。内部类型只在 benchmark 实现文件中引用，
不得因此暴露新的公共 API。

### 注册分配样本

分别注册固定数量的：

- member property；
- getter-only 和 getter/setter property；
- global property；
- member、const member 和 global method；
- 小型无状态 callable 与需要拥有状态的 callable；
- method overload set。

输出总分配次数、总字节数、峰值、提交后 live 字节数，以及折算后的每记录平均值。销毁局部 registry 后
live 必须回到样本开始前。

`property_wrapper`、`method_wrapper` 和 registry 当前使用 `memory::library_resource()`。普通
`set_default_memory_resource()` 不能可靠统计这些分配。实施时优先增加仅由 benchmark 构建启用的内部
分配跟踪配置，或复用现有
`CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE=1` 构建变体；不得为了统计而给公共注册 API 增加
测试专用参数。统计构建与正式耗时构建分开执行。

### Property 热路径

在现有用例基础上补齐：

- 直接成员读写；
- 缓存 `property` 的 member typed get/set；
- compiled member accessor typed get/set；
- accessor property typed、`any` 和 `value`；
- global property typed、`any` 和 `value`；
- `instance` 按名便利入口，作为查找加调用的高层对照。

member 与 accessor/global 必须分组，因为 compiled member accessor 可以缓存 offset，而 accessor/global
仍需进入 wrapper。

### Method 热路径

在现有用例基础上补齐：

- 直接 C++ 调用；
- 缓存单个 method 后的 member、const member 和 global 调用；
- `argument` 精确 C++ 路径；
- `meta::value` 路径；
- `any` 路径；
- 已缓存 method set 的重载匹配；
- `instance` 按名调用；
- 默认参数和需要转换的参数，作为高层编排对照。

request dispatch、参数准备、重载匹配和返回值装箱必须能通过用例拆分定位，不能只保留一条完整调用结果。

## 基线与判断标准

迁移前后对比必须在同一机器上分别检出明确提交，使用相同 benchmark 源码、Release preset 和输入运行。
如果旧提交不能直接编译新 benchmark，应将纯测量补丁临时移植到旧提交，不把兼容性改动计入被测实现。

以下阈值用于触发分析，不代表自动判定失败：

- 同口径中位数回归超过 10%，且三轮方向一致；
- compiled member accessor 明显慢于迁移前 member 快路径；
- 单个简单 property/method 出现多个无法由 metadata、容器或 callable 所解释的常驻分配；
- wrapper/record 尺寸增长使 1000 个节点的额外常驻内存达到需要关注的量级；
- request dispatch 在已缓存、精确参数路径中成为主要成本，而不是转换、查找或装箱成本。

只有达到触发条件并能由拆分用例复现时，才进入优化设计。若差异处于噪声范围，保留当前结构。

## 候选优化及进入条件

- 无状态 wrapper：只有 member property 注册分配占比显著时，才评估非类型模板参数入口或无状态 model；
- 小对象优化：只有 state 分配和局部性成为明确瓶颈时，才评估 wrapper 内联小状态；
- request 专用快路径：只有 request kind 分发本身成为热点时，才评估缓存函数或缩短 dispatch；
- method callable 存储：只有 `shared_ptr` callable 分配在真实注册模型中占比显著时才调整；
- constructor/type model wrapper：不由本计划自动触发，必须有独立行为或 ABI 问题。

任何优化都必须保持插件卸载、resource 配对销毁、move-only 所有权和 compiled accessor 生命周期语义。

## 实施阶段

### 阶段 0：基线冻结

- [x] 确认 wrapper 迁移前后用于对比的提交；
- [x] 固定 Windows/Linux benchmark preset、迭代次数和重复规则；
- [x] 记录当前已有 property/method 用例和缺口；
- [x] 为结果摘要复用 `benchmarks/results/README.md` 的统一规则。

迁移前代码基线为 `162f74c`，property wrapper 迁移提交为 `43b9116`，method wrapper 迁移提交为
`ecb702d`，method 直接 request dispatch 收敛提交为 `654cc75`。正式迁移后结果以执行 benchmark
时记录的当前提交为准。Windows/Linux 均使用对应 Clang Release benchmark preset、100000 次迭代、
至少三轮并取中位数；布局和分配样本不使用迭代耗时。

现有用例覆盖 member/accessor property 的 typed、`value`、`any`，以及缓存 method、
`argument`、`value`、按名和直接调用。阶段 2 又补充 compiled accessor、readonly property、
const/non-const member、重载匹配、默认参数和安全数值转换拆分。当前公开 builder 不提供 global
property 注册入口，因此将其记录为不可达边界，不为 benchmark 新增 API。

### 阶段 1：布局与分配样本

- [x] 增加 layout samples；
- [x] 增加 benchmark-only 内部分配跟踪构建方式；
- [x] 增加 property 注册分配样本；
- [x] 增加 method 注册分配样本；
- [x] 验证 registry 销毁后无 live allocation。

普通 benchmark 始终输出 wrapper、record 与公共句柄布局。只有独立构建显式启用
`CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE=ON` 时，才统计 32 个类型、每类 3 个节点的混合
property/method 注册分配；输出扣除空 registry 基线，并要求 registry 销毁后 live bytes 为 0。
该构建不用于耗时结论。

### 阶段 2：运行时拆分用例

- [x] 补齐 direct、cached、compiled 和 named property 对照；
- [x] 补齐 member、accessor property 对照，并记录 global property 的公开 API 边界；
- [x] 补齐 method dispatch、参数准备、重载匹配和返回装箱对照；
- [x] 保持现有 benchmark 名称稳定，新增用例使用明确层次命名。

property 用例现已区分直接成员访问、缓存 property、compiled accessor、readonly getter 和按名查询。
返回值 getter 继续通过 `value` / `any` 路径测量；只有返回引用的 accessor 才支持 typed 原地读写，
避免将临时返回值误当成可写引用。method 用例现已区分 const/non-const 调用、精确重载匹配、默认参数
补齐和安全数值转换。1000 次迭代的 Windows Clang Release 冒烟已通过，正式耗时结论仍由阶段 3 的
100000 次、至少三轮结果产生。

### 阶段 3：跨平台执行

- [x] Windows Clang Release 至少运行三次；
- [ ] Linux Clang Release 至少运行三次；
- [ ] Linux GCC 完成构建与冒烟；
- [ ] 保存原始环境信息和人工结果摘要。

Windows Clang 结果见
[`2026-07-31-windows-clang-meta-wrapper-22497cb.md`](../../benchmarks/results/2026-07-31-windows-clang-meta-wrapper-22497cb.md)。
三轮原始输出仅保存在本机构建目录，仓库提交环境信息、中位数和人工摘要；完成 Linux Clang 后再勾选
本阶段的统一结果归档项。

### 阶段 4：结论与收尾

- [ ] 判断是否达到优化触发条件；
- [ ] 若无需优化，记录保留当前结构的依据；
- [ ] 若需要优化，另立聚焦计划并明确预期收益；
- [ ] 回写 `meta-erased-wrapper-redesign.md` 和正式 benchmarking 文档。

完成标准：能够分别解释布局、注册分配、查找、转换、wrapper dispatch 和装箱成本，并以可复现数据决定
是否继续优化。

## 风险

- `ctrack` 微基准容易受 CPU 调频和后台负载影响，必须重复运行并使用中位数；
- 旧提交与新 benchmark 源码不兼容时，测量补丁可能改变被测路径，需要人工复核；
- 内部分配跟踪构建会改变 allocator 路径，只用于分配计数，不用于正式耗时对比；
- registry、`string_name` 和 metadata 的一次性分配可能污染样本，应使用预热和前后差值；
- record 尺寸下降不必然提高速度，不能仅凭 `sizeof` 推动结构重构。
