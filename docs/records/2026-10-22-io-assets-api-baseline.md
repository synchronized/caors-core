<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# IO 与资源 API 整理基线

## 范围与环境

2026-10-22，docs/io-assets-api-review 分支，公开 API 基线 main 3b543eb1，方案初稿 60f83fb0。
用户确认按[API 收敛方案](../plans/io-assets-api-consolidation.md)推进；本批补齐迁移清单并验证旧实现基线。
本机 Windows x64、Clang 23.1.1、MSVC ABI，使用 VsDevCmd 初始化开发环境；
复用 build/v030-clang-debug 目录，重新构建所选目标，依赖缓存离线使用。

## 方法与结果

```powershell
cmake --build build/v030-clang-debug --target caors_example_beta_assets_native_async caors_example_beta_assets_async_vfs caors_assets_asset_manager_test -j 2
# 运行示例时 PATH 加入 build/v030-clang-debug/src，以定位共享库。
.\build\v030-clang-debug\examples\beta\caors_example_beta_assets_native_async.exe
.\build\v030-clang-debug\examples\beta\caors_example_beta_assets_async_vfs.exe
ctest --test-dir build/v030-clang-debug -R '^caors.assets.asset_manager$' --output-on-failure
```

三个目标构建成功。宿主示例输出 width=256，验证延迟交付、请求复用和完成线程提交；
这是模拟宿主事件，不是真实 libuv 文件读取。VFS 示例原生异步与 worker 适配均输出 hello VFS。
asset_manager CTest 目标 1/1 通过；其中既有测试覆盖重挂载后的旧来源保留、预加载共享快照等场景。
相关源码：[宿主示例](../../examples/beta/assets_native_async.cpp)、
[VFS 示例](../../examples/beta/assets_async_vfs.cpp)、
[资源请求测试](../../tests/assets/asset_manager/asynchronous_loader_test.cpp)。

文档还修正了 IO 参考开头声称“不支持异步”的旧说明；当前协议与具体后端能力分别表述。
详细 API 去向集中维护于 Plan，不在本记录重复维护第二份清单。

## 限制与下一批

本批没有实现解除 VFS 类型依赖、迁移配置工厂或新的文件 API；既有示例通过不代表新架构通过。
没有运行完整平台矩阵、真实浏览器或 Gneiss 联调；本批仅文档及改造前的原生相关基线。
下一批应在独立 feature 分支原型验证批次来源传递，特别是根/依赖共享、合并请求跨批次与重载来源刷新。
替代机制完成前保留 context.sources 和现有快照，不提前删除同步写入等能力。
