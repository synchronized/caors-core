<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 文件服务直接绑定与配置消费验收

## 范围与环境

2026-10-31，feature/resource-source-boundary，PR #58，本记录与实现同批提交。
本机为 Windows Clang Debug 共享库与 Emscripten 静态库，Chrome 156.0.8078.48。
范围由[VFS 计划](../plans/vfs-api-consolidation.md)维护，当前用法见[文件服务来源](../reference/assets.md#文件服务来源)。

project_builder.bind_source(name, file_service) 复用现有来源表，按绑定持有适配对象，不进入通用长期对象列表。
名称别名共享所有权；最后解绑或清空前移除 provider 中的引用，避免悬空 GUID，已有快照与请求不受影响。
外部借用来源保留旧解绑语义。服务仍由调用方拥有，临时服务禁止绑定。
provider.unbind_source(source) 是按来源移除 GUID 的通用操作，不依赖 assets 模块。

异步资源示例、默认 JSON 原生配置测试、安装及浏览器消费直接绑定既有服务。
异步配置不再复制旧路由图；同步配置、保存、旧工厂与配置 schema 保留。

## 本机结果

- build/vfs-final-native.ps1：16/16 资源、配置及序列化相关回归，IO 1/1，相关示例通过。
- build/service-binding-target.ps1：最终服务绑定目标增量构建与回归 1/1，通过。
- build/io-removal-special.ps1：插件实际卸载 1/1，分块示例通过。
- build/io-removal-installed.ps1：原前缀及迁移安装各 6/6，通过新重载消费。
- build/service-binding-headers.ps1：原 8 个相关头及 project_builder 独立头编译通过。
- Emscripten 构建 Node 协议及浏览器目标，Node 1/1。
- tests/web/browser_test.cjs：真实浏览器通过；服务和适配对象释放后，已捕获 JSON 请求跨 tick 完成。
- 文档本地链接、clang-format 与 git diff --check 通过。

专项覆盖空名/重名拒绝、临时服务编译拒绝、别名保活、最终解绑清理 GUID、清空来源、
借用来源保持、配置重建保留手工绑定、在途请求独立寿命、反复绑定无持续保留、
冷表和已有容量下的分配失败及完整释放。
新增冷表测试曾因测试断言提前截获预期 bad_alloc 失败，改为先调用、再断言返回值后重跑通过。
未通过吞掉库分配失败或减少故障注入修补测试。

日志为 build/service-binding-{native-final,target,special,installed,headers,web-final,browser}.log。

## 平台与 PR 收尾

实现提交 1fafbad0c4b52d856d32865b4db8c315ac0bde31 的平台检查全部成功：

- [Linux](https://github.com/synchronized/caors-core/actions/runs/41089923761)：Clang 动态/静态、GCC、ASan、UBSan 五组成功。
- [Windows](https://github.com/synchronized/caors-core/actions/runs/41089936110)：Clang 动态/静态与 MSVC 三组成功。
- [Emscripten](https://github.com/synchronized/caors-core/actions/runs/41089953168)：公共头、Node 协议及真实浏览器成功。

后续文档收尾不改实现，未绕过失败检查。
已复核最终 PR 范围、用户迁移、示例及同步能力保留条件，满足转为可评审的条件；
[PR #58](https://github.com/synchronized/caors-core/pull/58) 的评审状态由 GitHub 维护，不在文档复制状态。
不自动合并或发布。
此批完成已确认的服务绑定和异步配置消费，不声称同步工具路由已经删除。
