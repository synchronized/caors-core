<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Library 与 Plugin 管理系统开发计划

## 文档状态

- 当前状态：阶段 0 至阶段 5 已完成，阶段 6 详细设计已冻结并开始实施；
- 第一阶段范围：跨平台动态库封装、稳定插件 C ABI、描述信息、加载与生命周期；
- 后续范围：依赖图、服务扩展、registration scope、registry overlay 和安全卸载；
- 非目标：第一版热重载、跨编译器 C++ ABI、进程外插件和脚本插件。

## 已冻结的首版决策

- 低层 namespace 使用 `platform::shared_library`；
- manifest 后续复用 CAORS 现有 JSON 基础能力，但读取过程不依赖 meta 类型注册；
- 第一版只支持显式动态库路径，不扫描目录；
- plugin manager 只提供显式实例，不提供默认进程级全局对象；
- 第一版不支持静态插件；
- 第一版不承诺安全卸载或热重载；
- 插件错误后续增加独立 `error_domain::plugins`，动态库错误使用
  `error_domain::platform`。

## 当前实施结果

- 已新增 move-only `platform::shared_library`；
- Windows 使用 `LoadLibraryExW`，POSIX 使用 `dlopen`；
- 已支持显式路径、local/global 加载模式、符号查询、幂等关闭和平台错误域；
- 已增加默认状态、无效路径、move 语义的编译期约束测试；
- plugin identity 测试已删除私有加载器，改用公共 `platform::shared_library`；
- 已新增纯 C 兼容 `plugins/abi.h`、`plugin_binary`、descriptor 拷贝、生命周期和 extension 查询；
- 已使用真实 C 编译单元验证 ABI 头可以被 C 编译器包含；
- 已覆盖有效插件、ABI 不兼容、缺少入口和 initialize 失败；
- 已新增独立 `semantic_version`、基础完整上下界 `version_range` 和 JSON manifest；
- manifest 字符串由宿主拥有，解析过程复用 JSON DOM 且不依赖 meta 注册；
- 已实现重复 ID、缺失依赖、版本冲突、optional dependency 和循环检测；
- 依赖解析同时生成 initialize/start 正序与 stop/shutdown 逆序；
- 已新增显式 `plugin_host`，统一拥有 Host ABI、运行时扩展、自定义扩展和服务记录；
- Host Extension 使用稳定字符串 ID 与精确版本，调用方可按已知版本逐级降级查询；
- 服务使用稳定 GUID 和整数版本，不复用 meta capability key，并支持在版本区间内选择最高版本；
- 服务地址采用借用语义，由发布方 owner 标识并在生命周期结束前统一移除；
- 已新增显式 `plugin_registration_scope`，可统一移除服务、断开事件并请求取消任务；
- scope 使用结构化 blocker 区分 meta 注册、未排空任务和插件拥有的擦除值；
- append-only meta 注册和不能证明 callable 已销毁的任务会返回 `errc::unsupported`，不会错误
  宣称可以卸载；
- `plugins::plugin` 只读 handle 需要引用 manager 拥有的稳定节点，因此不在
  `plugin_binary` 阶段制造无所有权裸视图；它随首个 manager 阶段实现；
- Windows Clang Shared Debug 使用 `--parallel 1` 构建通过，阶段 1/2 相关测试 3/3 通过。

## 背景

CAORS 已支持共享 runtime 下的跨 DLL `type_id`、capability key 和 extension key 统一，也有真实的
`host + plugin_a + plugin_b + caors.dll` 测试拓扑，但目前测试直接使用
`LoadLibrary` / `dlopen`，没有正式的 library/plugin 公共 API。

需要补充两层能力：

```text
platform::shared_library
└─ 只负责加载动态库、查询符号和保持句柄生命周期

plugins::plugin_manager
├─ 发现并校验插件
├─ 协商 ABI
├─ 解析依赖
├─ 驱动生命周期
├─ 提供 host service
└─ 管理注册与卸载安全
```

动态库不是插件。任意 `.dll` / `.so` / `.dylib` 都可以由 `shared_library` 加载；只有提供 CAORS
插件入口并通过协议校验的动态库才由 `plugin_manager` 管理。

## 外部方案取舍

### Boost.DLL 与 POCO

借鉴跨平台动态库 RAII、符号查询、平台前后缀、加载选项和错误诊断。不直接依赖 Boost.DLL；
首版功能较小，CAORS 自己封装系统 API 可避免引入 Boost.Filesystem/System。

### CLAP

借鉴单一 C ABI 入口、host/plugin 双向函数表、固定结构版本和可选 extension 查询。插件协议不在
DLL 边界传递 STL、异常或内部 C++ 类型。

### Vulkan Loader

借鉴加载前 manifest、接口版本协商、结构大小字段和清晰的兼容性拒绝流程。

### Qt Plugin

借鉴无需执行插件代码即可读取 metadata，以及引用仍存在时拒绝卸载的语义。不引入 QObject/moc。

### RTTR

借鉴动态库注册内容追踪和“卸载前不得保留插件 property/method/value”的生命周期约束。
CAORS 不直接复制 RTTR 从全局 registry 删除节点的方式；当前 append-only registry 需要先引入
overlay 才能安全卸载。

## 命名与模块边界

建议模块划分：

```text
include/caors/platform/shared_library.h
src/caors/platform/shared_library.cpp

include/caors/plugins/abi.h
include/caors/plugins/plugin.h
include/caors/plugins/plugin_manager.h
include/caors/plugins/manifest.h

src/caors/plugins/
```

- `platform::shared_library`：平台动态库；
- `plugins::plugin_binary`：已经打开并完成 ABI 查询的插件二进制；
- `plugins::plugin`：面向用户的只读插件句柄；
- `plugins::plugin_manager`：发现、依赖、状态和生命周期所有者；
- `plugins::plugin_id`：稳定 GUID 包装，不复用运行期 `type_id`；
- `plugins::plugin_state`：显式状态机。

不建议使用过于宽泛的 `library_manager`。底层动态库不需要全局 manager；插件才需要集中协调。

## `shared_library` 设计

建议接口：

```cpp
namespace caors::platform {

enum class library_load_mode : std::uint32_t {
  local,
  global,
};

class shared_library final {
public:
  shared_library() noexcept = default;
  shared_library(const shared_library&) = delete;
  shared_library& operator=(const shared_library&) = delete;
  shared_library(shared_library&&) noexcept;
  shared_library& operator=(shared_library&&) noexcept;
  ~shared_library();

  [[nodiscard]] static result<shared_library>
  open(const std::filesystem::path& path,
       library_load_mode mode = library_load_mode::local);

  [[nodiscard]] bool loaded() const noexcept;
  [[nodiscard]] void* find_symbol(std::string_view name) const noexcept;
  [[nodiscard]] const std::filesystem::path& path() const noexcept;
  [[nodiscard]] error close() noexcept;

  template <typename Function>
  [[nodiscard]] Function* find_function(std::string_view name) const noexcept;
};

} // namespace caors::platform
```

约束：

- move-only RAII；
- Windows 使用 `LoadLibraryExW` / `GetProcAddress` / `FreeLibrary`；
- POSIX 使用 `dlopen` / `dlsym` / `dlclose`；
- 默认 POSIX `RTLD_NOW | RTLD_LOCAL`；
- 原生句柄保持私有，首版不公开所有权不清晰的 `native_handle()`；
- 错误保存平台错误码和可读诊断，不仅返回 `false`；
- `close()` 在未加载时幂等成功；
- 加载器不自动猜测任意搜索路径，路径解析由更高层负责；
- 使用中的函数指针和插件对象不能比 `shared_library` 活得更久。

## 稳定插件 ABI

动态库只要求导出一个固定名称：

```cpp
extern "C" CAORS_PLUGIN_EXPORT
const caors_plugin_entry* caors_query_plugin(
    const caors_plugin_host* host,
    std::uint32_t host_abi_version) noexcept;
```

ABI 头只使用：

- 固定宽度整数；
- POD 结构；
- `const char* + size`；
- opaque `void*`；
- C 调用约定函数指针；
- CAORS 固定宽度错误码。

禁止直接传递：

- `std::string`、`std::vector`、`std::span` 和 `std::filesystem::path`；
- C++ 异常；
- `meta::type_record` 等 detail 结构；
- 需要跨模块析构的拥有型 C++ 对象；
- 编译器相关虚表接口。

基础结构必须包含：

```cpp
struct caors_plugin_entry {
  std::uint32_t struct_size;
  std::uint32_t abi_version;
  const caors_plugin_descriptor* descriptor;

  caors_error_code (*initialize)(caors_plugin_context*) noexcept;
  caors_error_code (*start)(caors_plugin_context*) noexcept;
  caors_error_code (*stop)(caors_plugin_context*) noexcept;
  caors_error_code (*shutdown)(caors_plugin_context*) noexcept;

  const void* (*get_extension)(const char* id,
                               std::size_t id_size,
                               std::uint32_t version) noexcept;
};
```

`struct_size` 允许旧 host 忽略新尾字段。`abi_version` 用于不兼容基础协议；可选能力通过
`get_extension()` 扩展，避免频繁扩大基础结构。

## 描述信息与 Manifest

插件描述至少包含：

- 稳定 `plugin_id`；
- 名称和显示名称；
- semantic version；
- 插件 ABI 版本；
- 最低/最高 CAORS runtime 兼容版本；
- 依赖列表；
- 可选 capabilities；
- 动态库相对路径。

建议使用伴随 JSON manifest：

```json
{
  "id": "2a6d5992-ff3c-45ae-a167-d861e469aa90",
  "name": "caors.image.png",
  "version": "1.0.0",
  "abi": 1,
  "library": "caors_image_png.dll",
  "dependencies": [
    {
      "id": "1cc73a41-dabb-4545-88e5-28a58dc3f821",
      "version": ">=1.0.0 <2.0.0",
      "optional": false
    }
  ]
}
```

manifest 用于加载前发现、依赖排序和诊断；DLL 内 descriptor 用于加载后的最终校验。两者的 ID、
版本和 ABI 不一致时拒绝插件。

首版不要求把 metadata 嵌入动态库二进制节，避免立即实现 PE/ELF/Mach-O 解析。

## 生命周期状态机

建议状态：

```text
discovered
    ↓
loading → failed
    ↓
loaded
    ↓
initializing → failed
    ↓
initialized
    ↓
starting → failed
    ↓
running
    ↓
stopping → failed
    ↓
stopped
    ↓
shutting_down
    ↓
unloaded
```

语义：

- `load`：打开动态库、查询入口、校验 ABI/descriptor；
- `initialize`：注册类型、服务和功能，不启动业务线程；
- `start`：依赖均已初始化后开始运行；
- `stop`：停止线程、请求和事件生产；
- `shutdown`：释放插件服务与注册作用域；
- `unload`：最后关闭动态库。

失败必须保留具体阶段、插件 ID、路径和原始错误。批量加载失败时，按相反顺序停止并关闭本批次已经
成功的插件；不得回滚加载前已经运行的无关插件。

## 依赖管理

- 依赖使用 `plugin_id + version_range`；
- 不使用文件名、注册名称或 `type_id` 表示依赖；
- 加载前检查缺失依赖和版本不兼容；
- 使用拓扑排序确定 initialize/start 顺序；
- stop/shutdown 使用逆拓扑顺序；
- 循环依赖返回明确错误并附带依赖链；
- optional 依赖缺失不失败，但插件必须通过 context 查询其是否存在；
- 第一版不支持运行中替换依赖版本。

## Host 服务与 Extension

host API 只提供稳定、窄型能力：

```cpp
struct caors_plugin_host {
  std::uint32_t struct_size;
  std::uint32_t abi_version;
  void* context;

  void (*log)(void* context, caors_log_level,
              const char* message, std::size_t size) noexcept;
  const void* (*get_extension)(void* context,
                               const char* id,
                               std::size_t id_size,
                               std::uint32_t version) noexcept;
};
```

第一版 host extension 可只包含日志和 runtime 版本。meta 注册、服务注册和任务系统接入应作为独立
版本化 extension，不直接塞进基础 ABI。

服务身份使用稳定 GUID/字符串协议 ID 和版本，不复用 `meta::capability_key`。capability key 只保证
同一 runtime 内的 C++ 类型查询，不是长期插件协议。

## Registration Scope 与卸载

第一版允许插件在 `initialize` 中使用统一 registration builder。插件 lifetime 应显式区分
`pinned_process` 和 `detachable`：`pinned_process` 插件默认保持加载到 `plugin_manager` 或进程结束，
不支持提前 unload，也不需要为插件 ops 调用承担 in-flight 原子 guard；`detachable` 插件显式选择
热卸载语义，必须通过 registry layer detach、library lease 和 call guard 保护。

这样命名避免使用 `static plugin`，因为 `static` 容易和静态链接、静态初始化或静态存储期混淆。
实际语义是插件被钉在进程生命周期内，而不是插件以静态库形式链接。

当前 registry record、property/method wrapper、capability、extension 和擦除值可能保存插件代码地址。
只有 `detachable` 插件需要证明这些地址不可达后才允许真正关闭动态库。

当前 scope 和 manager 已追踪：

```text
plugin_registration_scope
├─ registry overlay
├─ service registrations
├─ event connections
├─ task/request cancellation
└─ plugin-owned erased values
```

真正允许 `unload` 前必须满足：

- 插件线程全部停止；
- 异步任务和 completion callback 全部取消或完成；
- event connection 全部断开；
- 服务没有外部引用；
- overlay 已从查询链移除且旧 view 已失效或由 snapshot 保活；
- 不存在需要插件析构函数的擦除值；
- property/method/capability 函数指针不再可达。

`plugin_manager::unload()` 现在按插件 lifetime 策略处理：`pinned_process` 返回
`errc::unsupported` 并保留插件；`detachable` 先 detach registry layer，仍有 scope blocker、layer
lease 或 in-flight 调用时返回 `errc::pending`。`plugin::unload_diagnostic()` 暴露最近一次卸载诊断，
包括 plugin ID、generation、状态、lifetime、blocker、layer lease、in-flight 调用和本地注册类型数量。

## 线程安全

- manager 的发现、加载和状态变更串行化；
- 查询已发布 plugin handle 可以并发；
- 不在持有 manager mutex 时调用插件代码，避免插件回调 host 导致死锁；
- 生命周期转换先在锁内声明过渡状态，解锁后调用插件，再在锁内发布结果；
- 同一插件的并发 load/start/stop 请求必须返回确定结果；
- 插件自行创建的线程必须在 `stop` 返回前终止。

## 公共 Handle 与失效

`plugins::plugin` 应是轻量只读句柄，不直接暴露 `caors_plugin_entry*`。建议内部使用
`plugin_id + generation + manager state`，每次需要访问动态信息时校验 generation。

第一版插件不卸载时，handle 可稳定到 manager 销毁。引入热重载后：

- 同一路径重新加载产生新 generation；
- 旧 handle 返回失效状态；
- 不让旧 handle 静默指向新插件；
- descriptor 的公共视图由 manager 拥有，不引用 DLL 静态字符串。

## 错误与诊断

需要增加或映射的错误语义：

- 动态库不存在或加载失败；
- 缺少插件入口；
- manifest 无效；
- descriptor 与 manifest 不一致；
- ABI 不兼容；
- 重复插件 ID；
- 依赖缺失、版本不兼容或循环；
- 生命周期状态错误；
- initialize/start/stop/shutdown 失败；
- 插件仍被使用，无法卸载；
- 插件抛出异常或违反 `noexcept` 协议时生成致命诊断。

manager 应提供结构化诊断，不只提供字符串：

```cpp
struct plugin_diagnostic {
  plugin_id id{};
  plugin_state stage{};
  error status{};
  std::filesystem::path path{};
  core::string message{};
};
```

## 实施阶段

### 阶段 0：需求冻结与基线

- [x] 固定 namespace、文件布局和错误语义；
- [x] 记录 Windows/POSIX 测试环境；
- [x] 复用现有 plugin identity 测试作为基线；
- [x] 明确第一版不支持安全卸载和热重载。

### 阶段 1：跨平台 `shared_library`

- [x] 实现 move-only RAII；
- [x] 实现 Unicode 路径、符号查询、加载模式和平台错误；
- [x] 覆盖不存在文件、无效动态库、缺少符号、move、重复关闭；
- [x] 将现有 plugin identity 测试的本地 `shared_library` 替换为公共 API。

### 阶段 2：ABI 与单插件加载

- [x] 新增纯 C 兼容 `plugins/abi.h`；
- [x] 实现单一 `caors_query_plugin` 入口；
- [x] 校验 `struct_size`、ABI 和 descriptor；
- [x] 实现 `plugin_binary` 和单插件 load/initialize/start/stop；
- [x] 增加 ABI 不兼容、缺入口和生命周期失败测试。

### 阶段 3：Manifest 与依赖图

- [x] 定义 manifest schema；
- [x] 实现显式 manifest 路径读取，且无需加载 DLL；
- [x] 实现 semantic version 与基础完整上下界 version range；
- [x] 实现拓扑排序、optional dependency 和循环诊断；
- [x] 覆盖多插件顺序及逆序停止。

目录扫描需要同时冻结文件命名约定、递归与搜索路径安全策略，移入首个
`plugin_manager` 发现阶段；底层 manifest API 保持显式路径，不自行猜测目录。

### 阶段 4：Host Extension 与服务

- [x] 增加版本化 host/plugin extension 查询；
- [x] 提供 Host 日志转发和 runtime extension；
- [x] 设计稳定 service ID、版本与借用所有权；
- [x] 不把 meta capability key 作为服务协议 ID；
- [x] 覆盖未知 extension、版本降级和 optional service。

### 阶段 5：Registration Scope

- [x] 追踪插件注册的 meta 类型、服务、事件和任务；
- [x] 明确 append-only registry 下可执行的清理范围；
- [x] 在不能安全卸载时返回结构化原因；
- [x] 文档化“stop 成功不等于可以卸载”。

当前 scope 可以直接清理服务和事件。请求取消只阻止任务逻辑开始或状态继续提交，不能证明
executor 队列中的擦除 callable 已经析构，因此清理前尚未完成的请求仍保留 `pending_task`
blocker。meta 注册在阶段 6 overlay 完成前始终保留 `meta_registration` blocker。插件生命周期
调用者必须先 `stop/shutdown`，再执行 scope 清理；任一步成功都不能单独作为关闭 DLL 的依据。

### 阶段 6：Registry Overlay 与安全卸载

- 详细实施计划见 `registry-overlay-safe-unload.md`；
- 独立设计并实现 plugin registry overlay 或 snapshot；
- 为 view 增加 generation/所有权策略；
- 完成异步任务、事件和擦除值清理；
- 满足卸载前置条件后再启用真正 `unload`；
- 覆盖反复加载、卸载、错误回滚和并发查询。

### 阶段 7：热重载

- 明确状态迁移和对象迁移协议；
- 只通过稳定 GUID 和版本化序列化格式迁移数据；
- 旧 generation 全部失效后加载新版本；
- 热重载失败时保留或恢复旧版本；
- 此阶段需另立详细开发计划。

## 测试拓扑

至少提供：

```text
caors.dll
plugin_host.exe
plugin_good.dll
plugin_dependency.dll
plugin_missing_entry.dll
plugin_bad_abi.dll
plugin_initialize_failure.dll
plugin_model_conflict.dll
```

覆盖：

- Windows 和至少一个 POSIX preset；
- Debug/Release 基础 ABI；
- Unicode 路径；
- host/plugin 共同链接同一 CAORS runtime；
- 并发查询和串行生命周期；
- 依赖顺序、循环、缺失与版本冲突；
- 初始化失败的逆序回滚；
- unload 被在用对象阻止；
- AddressSanitizer 下无 use-after-unload。

构建继续使用 `--parallel 1` 作为低内存环境的默认验证方式。

## 验收标准

- `shared_library` 可独立使用且不依赖 plugin/meta；
- 插件只有一个稳定 C ABI 入口；
- DLL 边界不传递 STL 拥有型对象或 detail 结构；
- manifest 可在不执行插件代码时完成发现和依赖校验；
- 生命周期状态转换可诊断且失败可回滚；
- 依赖按拓扑顺序启动、逆序停止；
- plugin descriptor 和字符串由 manager 拷贝并拥有；
- manager 不在持锁状态调用插件代码；
- 第一版不会错误宣称可安全卸载；
- registry overlay 完成前，不关闭仍被 registry 引用的 DLL；
- Windows Clang Debug/Shared 使用 `--parallel 1` 完成相关构建和测试；
- 每个实施阶段独立提交并推送。

## 后续待确认问题

1. registry overlay 采用查询链 overlay 还是不可变 snapshot；
2. 热重载的数据迁移协议需在阶段 7 前另立计划。

## 非目标

- 不保证混用不同 C++ ABI 的插件能够共享 C++ 类型；
- 不用 `type_id` 作为持久插件或服务身份；
- 不让插件直接访问 registry/detail 内部状态；
- 不在第一版实现任意 DLL 热卸载；
- 不依赖静态初始化完成插件注册；
- 不自动从不受信任目录加载动态库；
- 不提供安全沙箱；进程内原生插件拥有宿主进程权限。
