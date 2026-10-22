<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# IO、VFS 与资源加载 API 收敛方案

## 状态与范围

实施中，首批快照解耦与工程装配入口迁移已完成验收；2026-10-22 基于 main 3b543eb1 的公开接口审计。用户要求先整理 API 对照与调用示例，
首批平台验证和安装迁移证据见[原型记录](../records/2026-10-22-load-snapshot-prototype.md)。
后续接口统一分批实施，不宣布 v0.8.0 排期，不修改已发布 v0.7.0 的标签或制品。
当前行为见 [IO 参考](../reference/io.md)、[资源参考索引](../reference/README.md)及公共头。
本页为后续收敛方案的维护入口，之前 VFS/异步 loader 计划仍保留其已实现设计和历史证据。

目标是减少普通调用者必须理解的概念、等价入口和跨层参数，不以移动目录或新增包装类作为完成标准。
优先处理资源加载主线；文件写入、目录访问等既有能力不得因统一异步而无声丢失。

字节读取批新增统一 reader、VFS 编排适配和显式 JSON 异步接入，当前 API 见[资源参考](../reference/resources.md#统一字节读取适配)。
本批实现与验证见[字节读取记录](../records/2026-10-22-byte-reader-protocol.md)。
默认 JSON 工厂已迁移到异步 loader，旧 JSON 实现显式更名 sync_loader，见[迁移记录](../records/2026-10-22-json-factory-migration.md)；manager/database 旧同步执行入口及双注册槽已移除；低层同步组合仅接受显式 adapter，见[收口记录](../records/2026-10-22-single-load-entry.md)。远端验收和最终范围审计仍待完成，不记为整个重构完成。

日常请求进一步收敛为 load/reload/preload 三类：refresh 合入 reload，manager/group 移除逐次调度参数及模板 preload；当前语义以参考为准，本批证据见[三类请求迁移记录](../records/2026-10-22-three-request-api.md)。

VFS 主体重构按[VFS 收敛计划](vfs-api-consolidation.md)实施，本页保留资源侧与原始审计依据。

## 当前职责与问题

| 当前入口 | 实际职责 | 整理方向 |
| --- | --- | --- |
| asset_manager::request_load/reload/preload | 资源请求、缓存与依赖加载入口 | 保留；已有名称具有区分语义，不为缩短名称全部改为 load |
| asset_manager::create_group | 绑定一组 worker/completion 策略 | 保留；常规调用通过 manager 默认策略或 group |
| asset_manager::register_type | 将 cache/loader 接入资源数据库 | 保留核心能力；与配置工厂的注册入口分开，暂不新增 register_loader 同义入口 |
| asset_manager 文件系统/source 绑定与工厂 | 文件后端注册、配置装配、所有权管理 | 从核心下沉到可选工程装配层；确定实际使用方式后命名 |
| asset_manager::build(project_config) | 重建 manifest/provider/依赖及配置创建对象 | 拆出工程装配职责；核心仍需明确缓存失效与在途请求规则 |
| provider | GUID 到 source/location 索引及同步读写入口 | 分离定位与字节访问；不直接换名后保留全部职责 |
| provider_snapshot::resolve | GUID 解析为 resolved_file，并固定文件来源 | 保留现有语义至适配迁移完成，随后归入可选 VFS 资源适配 |
| async_load_context::sources | 请求管线直接携带 provider_snapshot | 核心移除此 VFS 类型；替代的批次一致性传递机制需先验证再确定 |
| file_system | 同步访问、异步打开、路由冻结、能力查询 | 区分访问协议与路由管理；同步能力迁移后才评估公共入口删除 |
| resolver / mount_file_system | URI 路由与虚拟路径挂载 | 归入 VFS；先核对匹配规则与嵌套语义，不直接合并为一个类 |
| async_file::start_read/start_close | 宿主后端协议 | 与普通调用入口分开；start_* 仅用于后端实现 |
| request_open/read/close | 公共异步操作、请求状态和通知调度 | 以文件操作主线收敛；上下文与文件对象具体承载方式待验证 |
| read_all_bounded/read_all_chunked/consume_chunks | 有预算的整文件或逐块处理 | 保留语义差别；整文件入口考虑合并预算与分块选项，逐块消费独立 |
| path / uri / location | 不同地址语义及拥有/借用形式 | 逐一审查隐式转换；先明确边界，不机械合并为 string |

源码核对入口：[asset_manager.h](../../include/caors/assets/asset_manager.h)、
[database.h](../../include/caors/resources/database.h)、[资源加载协议](../../include/caors/resources/io.h)、
[source.h](../../include/caors/resources/source.h)、[file_system.h](../../include/caors/io/file_system.h)、
[async_file.h](../../include/caors/io/async_file.h)。

database 请求路径会捕获 provider 快照并交给 loader，预加载闭包共享快照；
loader 可以忽略 sources，但当前 manager/配置/请求协议结构上仍依赖 VFS。
provider 的基本定位职责不要求 VFS，当前依赖来自 freeze_routes 和 resolved_file 的具体实现。

## 已确认的 API 迁移清单

2026-10-22 用户确认按详细清单推进；以下是目标方向，不表示候选 API 已实现。

| 当前 API/字段 | 处理方式与前置条件 |
| --- | --- |
| request_load / request_reload / request_refresh | 保留不同请求语义；refresh 与 reload 的区别单独核对，禁止机械合并 |
| request_preload / plan_preload | 保留依赖闭包加载与纯计划查询两种职责 |
| create_group / set_execution_options | 保留默认与分组调度；检查重配置时的活动操作限制 |
| request_* 的逐次 worker/completion 重载 | 调用点迁移到默认策略或 group 后收敛；内部仍可保留必要调度入口 |
| load / load_handle / reload / reload_handle / preload | 迁移到异步请求主线后退出主要公共入口；不得用 loop 内阻塞等待模拟 |
| find / resolve(ref) / is_current | 保留；明确裸指针非拥有及旧引用有效性 |
| register_type(cache, loader, type) | 保留唯一手动类型注册主线；不再增加 register_loader 同义接口 |
| bind/find/find_owned/unbind/clear_file_system(s)、file_system_count | 移至可选工程装配/VFS 注册组件，迁移借用和拥有式绑定 |
| bind/find/unbind/clear_source(s)、source_count | 移至来源适配，避免与核心资源注册混放 |
| register_file_system_factory / register_source_factory 及 typed 版本 | 工程装配层维护 |
| register_type_factory 及 typed 版本 | 配置创建层维护，与手动注册分开 |
| build / build_detailed | 工程装配层维护；迁移时保持失败诊断及手工绑定保留规则 |
| provider() | 随可选来源层迁出 |
| manifest() / dependency_graph() / database() | 审查可变逃逸入口，按实际调用需求收敛，暂不全部删除 |
| loader::load / loader::start / async_loader::start | 统一异步实现协议；同步 loader 经 worker 适配后迁移，最终不长期保留双轨 |
| context.guid / resource / stop / progress | 保留通用加载信息，明确借用和回调寿命 |
| context.worker | 区分解析调度与宿主 I/O 绑定，原型后确定保留范围 |
| context.sources | 核心去除具体 VFS 类型；批次与依赖来源一致性验证通过后才移除 |
| provider.bind/rebind/unbind/clear/find/contains/size/empty | 留在定位职责；source_location 返回类型与所有权一起审查 |
| provider.exists/read/open_read/write/open_write | 移出定位职责，归具体字节访问适配 |
| provider.snapshot / provider_snapshot.resolve | 映射固定与 VFS 路由冻结分清；保留语义至批次适配替代通过 |
| file_system.exists/status/list | 功能保留，异步替代需定义缺失、权限和路径错误区别 |
| file_system.read_all/open_read | 公共读取迁移至异步主线；同步实现作为 worker 适配细节 |
| file_system.write_all/open_write | 异步写入/提交/关闭尚未设计完成，禁止提前删除 |
| file_system.start_open/open_random_read | 移至后端接入协议 |
| snapshot/routes_paths/resolve_owned/freeze_routes | 从普通文件访问中下沉到 VFS 路由与适配 |
| resolver 与 mount_file_system 的 mount/unmount/clear/mount_count | 保留配置职责；URI 与虚拟路径匹配语义不机械合并 |
| async_file.start_read/start_close | 后端协议，不作为普通用户主入口 |
| async_file.size/source_id/max_read_size | 保留必要查询；来源诊断和预算限制与常规操作区分 |
| request_open/request_read/request_close | 围绕打开后的文件收敛；请求句柄统一报告发起、执行与通知失败 |
| request_open_on_worker/make_worker_file/random_access_reader | 下沉到后端适配，worker 在适配构造时绑定 |
| read_all_bounded/read_all_chunked | 候选合为带预算和策略选项的整文件读取；不丢失预算或内存峰值约束 |
| consume_chunks | 保留逐块消费和背压，不与整文件结果混淆 |
| io_request/io_completion | 区分用户控制句柄与后端完成凭证；内部状态下沉 detail/实现文件 |
| io_error 与同步 error | 定义统一转换规则，保留 native_code 和 backend_id |
| path/uri/location | 审查隐式转换和拥有/借用，不直接替换为 string |

### 解耦原型进展

实际本机验证与限制见[原型记录](../records/2026-10-22-load-snapshot-prototype.md)。
第一步以 load_snapshot 多态基类和 context.snapshot_as<T>() 替代上下文的 VFS 具体类型；
现有 provider 捕获时机和批次共享规则不变。此批接口已通过平台验收，集成由 PR #57 跟踪；尚未发布。
后续原型已提供 database::set_snapshot_capture；异步请求管线只接收通用捕获器，不再依赖 provider。
后续批次已从 database 移除 provider 与清单绑定方法，manifest_binding.h 提供可选绑定适配；
工程 provider 移入私有工程状态，公开入口归 project_builder。manager 暂时安装兼容捕获策略，
清空自定义捕获器即关闭捕获，use_provider_snapshot 可显式恢复；
配置装配第一步已将 build/build_detailed 的编排、路由图校验和失败诊断实现迁到 project_builder；
已移除 manager 的对应公共入口并迁移测试/示例。注册表、工厂、拥有对象表和配置标记已收进
私有 project_state，具体容器与对象销毁类型移出公共头；状态仍由 manager 保活。公开绑定、配置工厂注册、create_owned 及工厂扩展类型已经归入
project_builder；工厂通过 context.project() 使用装配入口。各阶段创建函数仍为 manager 私有实现，
后续继续迁移，不宣称整个资源核心已与 VFS 解耦。

第二批实现及本机结果见[来源边界记录](../records/2026-10-22-resource-source-boundary.md)；
远端平台验收尚未执行，不沿用第一批结果作为本批通过。

统一 loader 的实现已进入同分支，证据见[统一协议记录](../records/2026-10-22-unified-loader-protocol.md)：loader 仅提供异步 start，原同步实现迁移到 sync_loader_adapter；
删除独立 async_loader 类型。兼容注册槽和旧同步辅助入口暂保留，不以阻塞等待模拟异步。
provider 的 find/快照查询仍同步，字节访问协议拆分尚未实施。

### 调用点迁移边界

核心变更同时检查 resources/database.h、resources/async.h、resources/detail/type_entry_ops.h；
来源适配检查 resources/source.h/.cpp、manifest_file_system、JSON/序列化 loader 与工程配置解析。
示例检查 beta/assets_native_async、assets_async_vfs、asset_pipeline、config_schema；
测试检查 assets/asset_manager、assets/io、io、Web 协议与真实浏览器 smoke，随后检查安装 consumer。
Gneiss 只记录迁移需求，不未经范围确认修改上游依赖锁定或宣称联合验收。

## 建议边界

资源核心负责身份、缓存、请求合并、依赖协调、取消和发布；loader 负责取得输入并构造资源。
宿主按 GUID 直接加载时不要求配置 provider、文件系统或路由。
可选 VFS 适配负责地址解析、固定来源、文件生命周期及读取；工程装配负责从项目配置创建这些组件。
普通文件用户仍可独立使用 IO，不必经过资源系统。

不立即引入 file_context、通用 reader 或来源会话三个新抽象。先用以下两个完整接入验证边界：
一是 loader 直接调用宿主 GUID 服务；二是 loader 通过现有 VFS 固定来源并读取字节。
如果必须增加批次扩展接口，应明确类型、所有权和复制范围，禁止只用 void 指针掩盖契约。

## 目标调用主线

以下为概念示例，未编译、尚未实现；不代表当前签名或可直接复制的接入代码。

### 普通资源用户

```cpp
assets.register_type<texture>(cache, loader);
auto request = assets.request_load<texture>(guid);
auto reload = assets.request_reload<texture>(guid);
auto group = assets.create_group(worker, completion);
auto preload = group.request_preload(root);
```

保留现有用户已经熟悉的操作；注册结果必须检查，示例仅突出入口。
默认 executor 在初始化绑定，特殊调度使用 group；逐次 executor 重载是否移除需核对实际调用点。
load 命中缓存可复用，reload 显式重建并按既有规则替换，preload 处理依赖闭包，不能合并为同义操作。

### 宿主直接加载

```text
assets.request_load(guid)
  → 已注册 loader.start(context, completion)
  → host 按 GUID 异步加载
  → completion 交付资源或错误
```

核心 context 保留 GUID、内存资源、取消和进度等通用信息，不要求 resolved_file。
loader 的宿主依赖在构造时注入；不得让 manager 解释宿主路径或文件协议。
依赖请求继续由资源管线协调，不为 loader 再建第二套缓存或请求去重。

### 可选 VFS 加载

```text
工程装配配置来源与挂载
  → 创建使用 VFS 的 loader，并注册到 assets
  → 加载批次固定 GUID 映射及来源路由
  → loader 打开文件、读取、解析
  → 清理文件操作并交付资源结果
```

固定来源不能仅在每个依赖启动时分别捕获，否则可能跨版本混用。
实现前需确定根请求、依赖、重复请求合并和来源切换的批次边界；未解决前保留现有快照管线。
宿主服务自行定义其来源一致性保证，文档不能默认宣称所有 loader 都拥有 VFS 快照保证。

### 独立文件访问

```cpp
// 候选形式；文件对象与注册流程在后续原型中确定。
auto opening = files.open_read(path, on_open);
auto reading = file.read_at(offset, length, on_read);
auto closing = file.close(on_close);
```

这里只展示三个阶段，不表示可以在 on_open 前使用 file，或必须立即关闭。
返回请求句柄用于取消、查询和回调投递失败兜底；忽略句柄不取消，文件通过打开结果交付。
对外优先统一异步读取，后端允许 worker 适配阻塞操作；不在公共异步调用中偷偷阻塞文件读取。
完成通知经指定调度入口；拒绝或丢弃时句柄仍可查询终态，不能承诺回调总会执行。
若未来保证通知不内联，须显式迁移当前允许返回前完成的契约，并确认宿主调度器能够满足。
不提供可在任意 loop 线程阻塞的通用 sync_wait。

## 必须保留的行为

- 取消不等于底层停止；关闭须拒绝新操作并等待在途清理，析构不能阻塞宿主 loop。
- 结果缓冲拥有数据；借用内存资源、宿主 loop 和模块凭据的寿命须明确。
- 来源切换、旧资源保活、重载失败保留旧对象以及请求合并语义不得因拆层退化。
- 精确范围读取与允许短读的后端协议必须区分，预算在分配前检查。
- 原生动态插件与 Web 单线程的能力边界继续生效，不引入强制 Asio/libuv 依赖。
- 当前同步 write_all/open_write、枚举和属性查询仍存在；异步替代未验收前不删除。

## 实施批次与验收

1. 本批：源码审计、对照表、目标示例与未决项入库；仅文档验证。
2. 原型批：在 feature 分支实现宿主 GUID 与 VFS 两条完整路径，确定批次一致性和注册生命周期；
   比较使用成本后固定公共签名，再确定版本范围。原型未通过前不移动整棵目录。
3. 核心批：解除加载协议对 provider_snapshot 的直接依赖，将工程装配移出核心；
   保持已有路径可迁移，不长期维护两套等价 API。
4. IO 批：统一用户入口/后端协议，随后整理目录和名称；写入与元数据另有完整迁移清单。
5. 收尾批：迁移内部使用、示例、安装 consumer 和文档；移除确定废弃的入口，发布破坏性迁移说明。

后续代码验收至少覆盖：无 VFS 的 loader、VFS 根/依赖来源一致性、跨组请求合并、重载失败、
取消竞争、队列拒绝与丢弃、关闭及迟到回调、模块/资源保活；覆盖 Windows/Linux 与真实 Web 事件循环。
按受影响机制执行原生矩阵、sanitizer、压力及安装迁移；纯方案文档不重复完整 CI。
现有路径的本机基线已验证，见[整理基线](../records/2026-10-22-io-assets-api-baseline.md)。
首批新增行为与安装迁移已通过平台验收，详见原型记录；文件 API 统一、同步入口退出及 provider 后续收敛仍未完成，不能记为整个重构完成。

## 未决与不纳入本批的事项

- 核心与 VFS 适配之间如何传递批次一致性：先做原型，不能仅靠 loader 构造注入解决依赖共享。
- 资源同步主入口在替代完成后迁移退出；文件异步写入/枚举方案仍未决定，禁止提前删除现有能力。
- 文件操作对象与函数命名：概念示例仅作评审，不提前形成兼容承诺。
- 下一版本号、最终破坏性范围及 Gneiss 迁移：原型和调用点审计完成后确定；本批不修改上游。
