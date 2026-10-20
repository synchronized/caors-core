<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Emscripten 兼容性评估

## 来源与身份

2026-10-20，读取相邻 Gneiss docs/plans/UPSTREAM-051-caors-web-convergence.md；
其报告基于 cbe41b75223f52bec746cc23346b28eba90fa693，不能直接当作最新版本结论。
CAORS 当前已发布 v0.6.0；本次实际复现 main 48dcaa00988950cf453f23a3595f182fb29faf50。
未修改 Gneiss/Granit，也未修改 CAORS 产品实现。

## 当前提交复现

Windows x64 PowerShell；Emscripten 5.0.6（6ea9c28c38cdd40c1032fa04400c9d16230ee180），wasm32，无 -pthread。
设置 EMSDK_PYTHON 为本机 Python，使用 emcmake 配置 Ninja，CAORS_LIBRARY_TYPE=STATIC，
CAORS_BUILD_TESTING/BUILD_TESTING/CAORS_BUILD_EXAMPLES/CAORS_BUILD_BENCHMARKS 全 OFF。
配置成功，Threads 探测成功；cmake --build build/web-assessment/current -j 2 失败。

首批 registry、file_system_resolver、mount_file_system 的 atomic<shared_ptr> 实例化触发
libc++ 的 trivially-copyable 静态断言，随后还有 bind_file_system/allocate_shared 错误。
独立 atomic-probe.cpp 仅包含 atomic/memory 并声明 atomic<shared_ptr<int>>，em++ -std=c++20
-fsyntax-only 同样失败；支持首批根因判断，不证明后续错误全部由此引起。
当前源码还存在 guid_hash 将 64 位常量赋给 size_t；本轮编译提前失败，未把 GUID 警告声称为已复现。

原始证据位于 build/web-assessment/configure.log、build.log、atomic-probe.cpp、atomic-probe.log。
尚未生成可运行库或进行 Node/浏览器协议验收，不推断所有 Emscripten 版本均失败。

## Granit 参考与适用取舍

实际读取相邻项目：

- cmake/granit_platform.cmake：平台能力集中判定，Web 静态约束，5.0.6 工具链检查。
- CMakePresets.json：EMSDK 工具链、独立 Debug/Release 构建目录。
- cmake/granit_web.cmake：编译与最终程序链接参数按 target 声明，页面生命周期单独配置。
- .github/workflows/emscripten.yml、tests/web/CMakeLists.txt、tests/web/platform_smoke_test.cjs：完整构建、CTest、安装 consumer、浏览器驱动和产物核对分层。

采用能力分层与构建/运行/消费端分别验收；不复制 WebGPU、SDL、页面或资源格式实现。
CAORS 优先固定 CI 工具链基线而非对所有用户硬编码版本；库目标不默认启用 pthread 或 Asyncify。
Emscripten [线程说明](https://emscripten.org/docs/porting/pthreads.html)说明 pthread 构建及浏览器主线程阻塞约束，
是单线程/协作调度边界的参考；本次是否可用仍以本机探针和运行结果为准。

## 结论与后续

当前 main 在上述目标组合不支持完整编译，v0.6.0 既有 Windows/Linux 验收不能扩展为 Web 承诺。
建议按 [v0.7.0](../versions/v0.7.0.md) 与[平台接入计划](../plans/emscripten-single-thread-platform.md)推进。
本记录仅完成评估与复现，未修复，也未执行 Gneiss 联合验收。
