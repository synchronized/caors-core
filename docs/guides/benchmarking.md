<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Benchmarking

CAORS 使用 `ctrack` 作为轻量 benchmark / profiling 工具，按需从锁定源码缓存创建
`ctrack::ctrack` interface 目标；不构建其上游示例、测试和 benchmark。获取、版本复用及离线准备
见[第三方依赖指南](third-party-dependencies.md)。

源码分组和目标入口见[基准目录](../../benchmarks/README.md)，历史数据与记录格式见
[结果索引](../../benchmarks/results/README.md)。本页统一维护构建、用例和统计范围。

## 构建和运行

benchmark 默认关闭，避免影响普通开发构建：

```sh
cmake --preset windows-clang-static-debug -DCAORS_BUILD_BENCHMARKS=ON
cmake --build --preset windows-clang-static-debug --target caors_benchmarks
build/benchmarks/caors_benchmarks 100000
```

命令行参数是每个用例内部循环次数；不传时默认 `100000`。

Debug benchmark 只适合确认用例能运行、观察 API 相对路径和粗略热点，不适合作为真实性能结论。
正式比较提交之间的性能时使用 Release preset，并固定迭代次数：

```sh
cmake --preset windows-clang-benchmark
cmake --build --preset windows-clang-benchmark --target caors_benchmarks
build/windows-clang-benchmark/benchmarks/caors_benchmarks 100000
```

Linux 下使用对应 release preset：

```sh
cmake --preset linux-clang-benchmark
cmake --build --preset linux-clang-benchmark --target caors_benchmarks
build/linux-clang-benchmark/benchmarks/caors_benchmarks 100000
```

建议同一机器、同一构建目录、同一迭代次数至少运行 3 次，取中位数对比。运行前尽量关闭无关后台任务；
如果要比较不同 API 路径，优先看同一次运行内的相对差异。如果要比较不同提交，记录编译器、preset、
迭代次数和提交号。

人工整理后的 release benchmark 历史摘要放在 `benchmarks/results/`。该目录只提交摘要，不提交完整
`ctrack` 原始控制台输出。

当前 registry 查询快路径的 release 摘要见
`benchmarks/results/2026-07-27-windows-clang-benchmark-7015a59.md`。

Meta wrapper 的布局样本会随普通 benchmark 输出。注册分配统计需要让
`memory::library_resource()` 临时跟随默认资源，必须使用独立构建目录，不能复用正式耗时构建：

```sh
cmake --preset windows-clang-benchmark \
  -B build/windows-clang-benchmark-allocations \
  -DCAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE=ON
cmake --build build/windows-clang-benchmark-allocations --target caors_benchmarks
build/windows-clang-benchmark-allocations/benchmarks/caors_benchmarks 1000
```

该构建只用于统计分配次数、字节数和退出后的存活内存，不用于耗时对比。

## 独立调度器目标

```sh
cmake --preset windows-clang-benchmark
cmake --build --preset windows-clang-benchmark --target caors_scheduler_benchmarks
build/windows-clang-benchmark/benchmarks/caors_scheduler_benchmarks
```

独立目标 `caors_scheduler_benchmarks` 用于调度器对比：构建该目标后直接运行
`build/windows-clang-benchmark/benchmarks/caors_scheduler_benchmarks`。每个场景采集 10,000
个样本，输出 p50/p95、吞吐、显式 resource 分配和句柄大小。`context.lifecycle` 包含构造和销毁；
`context.post_drain` 使用常驻 context；`pool.roundtrip` 使用常驻单 worker，并包含第二个屏障任务
及线程同步成本。常驻执行环境自身使用系统 resource，分配统计只覆盖传入的 task resource，
历史 Asio 基线不覆盖其内部 operation、线程、全局分配和屏障任务；新后端需独立重测。源码可在旧调度器版本中独立编译，
用于同机比较；基准不构成性能保证，也不作为测试门禁。


## 当前用例

当前 `caors_benchmarks` 覆盖以下稳定热路径：

- `core::string_name`：热路径 intern 查找和轻量比较。
- `meta::resolve<T>()` / `meta::find<T>()`：运行期类型查询。
- registry 查询快路径：`meta.registry.locked_find_type_id`、`frozen_find_type_id`、
  `layer_find_type_id`、`frozen_contains`、`frozen_property_lookup`、`frozen_method_lookup`
  以及 `locked/frozen/layer_schema_lookup`，用于对比默认加锁 primary、冻结 primary 和已发布
  layer 的只读查询成本。
- registry layer 资源策略：`meta.registry.resource.<system|monotonic|pool>.<register|query|unload>`
  使用隔离 registry 和同一组类型，对比 system、monotonic 与 size class pool 的注册、查询和卸载。
  同时输出 `live`、`unloaded`、`released` 三阶段的上游分配统计；`released` 阶段
  `bytes_in_use` 应回到 0。monotonic 和 pool 在 `unloaded` 阶段仍保留内存属于策略行为，不能直接
  判定为泄漏。
- `meta::property` / `meta::instance`：成员属性、readonly 属性和 getter/setter 属性的 typed、
  `value`、`any`、compiled accessor 与直接成员访问路径。返回值 getter 使用装箱路径，返回引用的
  accessor 才参与 typed 原地读写。
- `meta::method` / `meta::instance::call()`：直接 C++ 调用、const/non-const member、
  `invoke_result_of()`、`invoke()`、`call()`、`call_any()`、精确重载、默认参数和安全数值转换的
  对比。
- `meta::property` / `meta::method_set` 缓存句柄与 `instance` 按名便利入口的对比。按名入口每次
  调用都会查询属性或方法，热点循环应缓存 property / method 视图。
- Meta wrapper 样本：输出 property/method wrapper、record 和公共句柄的尺寸与对齐；诊断构建还会
  输出 32 个混合 property 或 method 类型注册时的净分配次数、字节数和销毁后存活内存。
- `meta::any_ref` / `meta::any`：运行期引用创建、类型检查和标量装箱。
- `small_vector` 内联/溢出路径，以及系统资源、fixed pool、size class pool 与标准 PMR pool 的混合尺寸分配。
- tasks/resources coroutine：请求已完成、单等待者和四等待者的显式资源分配及 p50/p95。

对于属性热点循环，先缓存 `meta::property`，需要进一步降低通用类型校验与 request dispatch 成本时，
使用 `compile_property_accessor()`。compiled accessor 的有效期不得超过所属 registry record 和注册
模块；它是热点优化入口，不替代具备完整动态语义的通用 property API。2026-07-31 的 Windows Clang
三轮结果中，compiled member accessor 明显快于 cached typed property，因此当前保留两级 API，不调整
wrapper 的所有权与擦除结构。
- `math`：`vec3` 点积/叉积/归一化、`mat4` 乘法、`mat4 * vec4`、四元数旋转、TRS 构造、
  透视投影矩阵构造、frustum 粗略裁剪和 project/unproject。
- math 动态路径：`math.meta.property_vec3_typed/value/any`、`math.meta.property_mat4_typed`、
  `math.meta.method_vec3_direct/value/any`、`math.meta.any_ref_vec3`、`math.meta.any_vec3` 和
  `math.meta.any_quat`。这些用例用于观察数学类型经过 `meta::property`、`meta::method`、
  `meta::value`、`any` 和 `any_ref` 时的额外成本，并与直接 C++ 调用对比。
- `serialization::to_value()` / `write_json_to()` / `to_json()` / `parse_json()` / `read_value_from()` / `read_json_from()`：
  反射对象到 `variant` DOM、复用 buffer JSON 写出、一次性 JSON 字符串、JSON parse、DOM 读回对象和
  JSON 文本 reader 直接读回对象的主路径。
- MessagePack：`parse_messagepack`、`deserialize_from_messagepack_variant` 和
  `read_messagepack_from` 分别测 DOM 解析、预解析 DOM 读回 typed 对象及完整 typed 读取。
- 序列化拆分项：`schema_view_find` 测默认 registry 上的序列化 schema 视图查询，
  `bound_schema_fields` 测提交后已绑定字段 schema 的遍历，`schema_property_lookup` 测按 schema 字段名查 property，`properties_get_ref` 测属性引用读取，
  `field_refs_to_variant` 测字段引用转 DOM，`manual_variant_object_direct` 测不经过反射的手动 DOM 构造，
  `write_value_to_json_reused_buffer` 测预构造 DOM 写入 JSON，`direct_json_writer_object` 测直接驱动 JSON
  writer 写对象。这些用例用于判断完整 JSON 序列化成本主要来自 schema 注册表、字段查找、反射、DOM 构造、
  DOM 写出、reader 字段索引还是 writer 本身。
- `resources::cache<T>` / `resources::provider` / `assets::json::loader<T>` /
  `resources::load_handle_into()`：cache 查找、已缓存加载、fake loader 未缓存加载、memory file system
  provider 读取，以及 provider -> JSON loader -> cache 的资源加载主路径。

这些 API 默认都是纯内存路径，结果更容易对比不同提交之间的回归。当前资源 JSON loader benchmark
会经过 `memory_file_system`、`file_source`、`provider`、输入流和 JSON parser，但不触碰真实磁盘。

## 分配统计

`caors_benchmarks` 在时间统计之外会额外打印 `Allocation samples`。这些样本使用
`tracking_resource` 记录分配次数、分配字节、峰值和退出样本后的 live 字节数，用来辅助判断：

- `serialization.to_value`：DOM 构造阶段是否产生大量临时分配。
- `serialization.write_json_to_reused_buffer`：复用输出 buffer 后是否仍有内部容器分配。
- `serialization.parse_json`：JSON parser 当前需要通过默认 resource 观察分配；这也提示后续 parser
  应继续收敛到显式 `memory_resource`。
- `serialization.read_messagepack_from.result`：typed 读取内部 DOM 和最终结果资源分配。
- `serialization.read_messagepack_from.temporary_upstream` / `temporary_logical`：线程临时
  `stack_resource` 的上游 chunk 分配及逻辑峰值、marker/rewind 次数。
- `resources.load_handle_into.json_uncached`：provider -> JSON loader -> cache 路径的总分配情况。

分配统计不是耗时 benchmark。它用于发现分配热点和泄漏风险，正式性能对比仍以 Release benchmark
同机重复运行结果为准。

registry 资源策略同样属于相对测量。注册和卸载每种策略只执行一次，查询次数才由命令行迭代参数控制；
因此正式判断应至少运行三轮，并结合分配次数、峰值、卸载后的保留量与真实 registry layer 生命周期，
不能根据一次冒烟结果直接替换默认资源。

## Math Fast Path 判断

math benchmark 分成两类：

- 纯计算微基准：`math.vec3.dot_cross_normalize`、`math.mat4.multiply`、`math.dmat4.multiply`、
  `math.mat4.transform_vec4`、`math.quat.rotate`、`math.trs`、`math.perspective_rh_zo`、
  `math.frustum_cull` 和 `math.project_unproject`。这些用例判断数学实现本身的标量成本，
  例如矩阵乘法、四元数旋转和投影 helper 是否需要手动展开、SIMD 或 affine 快路径。
- 动态访问微基准：`math.meta.*` 用例判断数学类型经过 `meta::value`、`any`、`any_ref`、
  反射属性和反射方法时的额外成本。这些数据用于决定 `vec2`、`vec3`、`vec4`、`quat`
  是否值得加入 `meta::value` fast path。

当前数学值的存储契约见[math 参考](../reference/math.md#与-metavalue-fast-path-的关系)，
候选范围与启动条件统一见[路线图](../roadmap.md#05-math-基础数学类型)。


## 后续适合拆分的专项

- 资源系统：provider、resolver、asset_manager 构建、跨类型 preload 和 async request。
- IO 系统：本地文件系统、挂载路径解析、字节流读写和大文件/小文件混合负载。
- 对象系统：`object_id` 生成、对象注册、handle/ref 查询和对象工厂。
- 序列化文本后端：JSON 文本读写、调用方提供 buffer、不同 field policy。
- 内存系统：`memory::resource`、分配器、策略资源和 debug tracking。

资源、IO 和异步任务受外部环境、线程调度、缓存状态影响更大，建议保持独立目标和独立输入数据。

## 约定

- benchmark 源码放在 `benchmarks/`，目标名使用 `caors_<topic>_benchmarks` 或
  当前聚合目标 `caors_benchmarks`。
- 聚合 benchmark 的源码按主题拆分到 `benchmarks/core`、`benchmarks/meta`、
  `benchmarks/math`、`benchmarks/serialization`、`benchmarks/resources`、`benchmarks/memory` 等目录；
  跨主题 fixture、测试数据和公共辅助函数放在 `benchmarks/support`。
- benchmark 目标只在 `CAORS_BUILD_BENCHMARKS=ON` 时加入构建。
- 用例应优先测公开 API。普通动态调用优先覆盖 `meta::value` / capability facade；
  只有在比较精确 C++ 语义或高级扩展成本时才单独测 `any`、`call_any()` 或 raw ops。
- 每个用例应使用固定输入，避免把日志、文件系统、网络或随机数据成本混入基础 API 测量。

## 来源快照成本基准

启用 CAORS_BUILD_BENCHMARKS 后，目标 caors_provider_snapshot_benchmarks 独立于聚合基准运行。
参数为样本数（默认 31，最少 3）、可选绑定规模（0 表示全部，或 1000/10000/100000）、
可选逐次样本 CSV 文件路径；每个暖态用例预热三次。
依次覆盖单/八来源、短/长路径、嵌套路由、重复捕获、rebind、remount、保留四份旧快照，
另测单资源 cache 命中、在途共享以及 typed/dispatched preload 的全命中、在途和依赖路径。

CSV 输出耗时 p50/p95/p99/max、更新最大耗时、显式 allocator 分配次数/字节、
新增保留量及包含 fixture 的峰值，以及 source::freeze_routes 调用次数。
分位数基于逐次样本，较少样本的尾部分位数分辨率有限；provider 首次捕获单列 first_capture（单样本，没有分位数统计意义），fixture 构造不计入捕获计时。
当前计时捕获本身，正常请求对象析构和预加载结果处理的成本包含在相应请求用例中。
显式计数不覆盖 STL 默认分配、所有共享指针控制块、后端 allocator 或进程总内存；不能据此报告全局分配总量。

Windows 普通 PowerShell 先用 scripts/msvc.ps1 初始化同进程 VS 环境，再配置 Release benchmark 构建目录。
至少独立运行三次，优化前后固定提交、机器、编译器与参数；摘要与限制进入结果目录，原始 CSV 保留于执行日志。
共享 CI 可用于可运行性核对，不替代固定 Windows/Linux 环境的正式性能复验。

### 固定 Linux 主机复验

以下是待执行流程，不是测量结果。需要固定 Linux x64 主机、CMake 3.23 以上、Ninja、
Clang 及 Git；机器在六轮测量期间保持相同配置，停止并行构建及其他显著负载。
先在同一主机完成两个提交的构建，再运行测量，不能把编译和测量交错。
本轮比较提交由[开发记录](../records/2026-10-20-v0.5.0-vfs-followups.md#性能证据)维护；
后续比较时应替换下列两个完整 SHA，并使用新的空输出目录，保留历史数据。

在仓库根目录执行（Bash）：

```bash
set -euo pipefail
snapshot_repo=$(git rev-parse --show-toplevel)
snapshot_output="$snapshot_repo/build/provider-snapshot-linux"
# 拒绝覆盖已有测量；命令失败后排查原因，使用新目录重试。
mkdir -p "$snapshot_repo/build"
mkdir "$snapshot_output"
git worktree add --detach "$snapshot_output/baseline" d59cbb6e83698103594082d6dc30523ab1a25301
git worktree add --detach "$snapshot_output/optimized" 602183206f2c28bd7b3fb294456a81f7ca5c8d78
for variant in baseline optimized; do
    (
        cd "$snapshot_output/$variant"
        cmake --preset linux-clang-release \
            -DCAORS_BUILD_TESTING=OFF -DCAORS_BUILD_EXAMPLES=OFF \
            -DCAORS_BUILD_BENCHMARKS=ON
        cmake --build --preset linux-clang-release \
            --target caors_provider_snapshot_benchmarks -j 2
    ) > "$snapshot_output/$variant-build.log" 2>&1
done

{
    date -u
    uname -a
    cat /etc/os-release
    lscpu
    clang++ --version
    cmake --version
    ninja --version
    ldd --version
} > "$snapshot_output/environment.txt" 2>&1
for variant in baseline optimized; do
    git -C "$snapshot_output/$variant" rev-parse HEAD \
        > "$snapshot_output/$variant-commit.txt"
    cp "$snapshot_output/$variant/build/linux-clang-release/CMakeCache.txt" \
        "$snapshot_output/$variant-CMakeCache.txt"
    for run in 1 2 3; do
        "$snapshot_output/$variant/build/linux-clang-release/benchmarks/caors_provider_snapshot_benchmarks" \
            31 0 "$snapshot_output/$variant-$run-raw.csv" \
            > "$snapshot_output/$variant-$run.csv" \
            2> "$snapshot_output/$variant-$run.stderr.log"
    done
done
(cd "$snapshot_output" && sha256sum ./*.csv > SHA256SUMS)
```

执行顺序为三轮基线后再三轮优化，与现有 Windows 记录一致；未消除环境随时间漂移的影响。
另记主机类型、CPU 调频/电源策略、后台负载和执行期间异常；不要筛除慢样本。
核对六份逐次 CSV 与汇总的样本数、分位数和冻结次数，将三轮对比、长尾与分配统计的适用范围
写入结果目录，并由执行记录链接。原始文件和环境日志保留于输出目录；命令成功本身不代表性能改善。
