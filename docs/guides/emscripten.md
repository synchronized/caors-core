<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Emscripten 单线程接入

本指南描述面向 v0.7.0 的开发中接入能力，CAORS 独立平台集成已通过；
v0.7.0 尚未发布，已发布 v0.6.0 不包含这些修复。
范围与未完成项见[版本页](../versions/v0.7.0.md)，实际证据见[集成记录](../records/2026-10-20-emscripten-integration.md)。

## 基线与配置

验证基线是 Emscripten 5.0.6、wasm32、无 pthread、静态库。安装并激活 EMSDK 后在独立目录配置：

```sh
source /path/to/emsdk/emsdk_env.sh
emcmake cmake --preset emscripten-static-release -G Ninja
cmake --build --preset emscripten-static-release
ctest --preset emscripten-static-release --output-on-failure
```

Windows PowerShell 先执行 emsdk_env.ps1，必要时设置 EMSDK_PYTHON 为本机 Python；
Debug 使用 emscripten-static-debug。显式传入 -G Ninja，防止 Windows emcmake 的默认 generator 覆盖 preset。
原生默认动态库 preset 不变；Web 显式 SHARED 或 -pthread 配置被拒绝，后者尚未纳入首版验证。
这些检查不把所有 Emscripten 版本都锁死为 5.0.6。

preset 启用 C++ 异常编译及异常捕获链接选项；手工构建需对库和消费端保持一致。
不声明完整无异常运行矩阵。开发依赖仍遵循[缓存指南](third-party-dependencies.md)。

## 事件循环与能力

config::has_background_threads 和 thread_pool::supported() 在此目标为 false。
thread_pool 默认线程数为 0，构造抛出 operation_not_supported 的 system_error；
禁用异常时构造会终止程序。调用方应先检查能力并选用宿主 executor 或 execution_context。
该类不会悄悄模拟 inline 线程池。

worker 和 completion 表示调度位置，可以绑定同一个协作队列；使用 poll/drain 在浏览器每帧或
事件回调中非阻塞推进。避免在浏览器主线程阻塞等待尚未发生的 I/O、完成回执或未来事件。
库不安装浏览器主循环，不自动启用 Asyncify；fetch/libuv 等加载适配仍由宿主提供。
async_loader 的拥有型完成凭证及取消/关闭语义保持原接口。

config::has_dynamic_libraries 为 false，shared_library::open 对非空路径返回 platform/unsupported；
空路径仍为 invalid_argument。静态类型注册、meta、资源缓存与 VFS 快照不因此关闭。

共享快照依照标准特性宏选择原子 shared_ptr；缺少特化时使用互斥保护的 shared_ptr 槽。
CAS 比较地址和拥有者，旧快照保活；旧值和失败 expected 在解锁后释放，避免析构重入死锁。
GUID 哈希先按 64 位混合再折叠到 32 位，GUID 本身不变；哈希值不可作为跨位宽持久化标识。
部分标准库缺少 long double 的 from_chars，回退使用经典 locale 的流，不修改全局 locale。

## 测试、浏览器与安装

Web 测试入口包括独立公共头和选定异步协议回归；原生线程/DSO 测试没有被宣称可在 Web 运行。
Node 运行不代替浏览器事件循环测试：

```sh
npm ci --prefix tests/web --ignore-scripts
node tests/web/browser_test.cjs build/emscripten-static-release/tests/web
```

浏览器驱动 playwright-core 固定 1.62.1（Apache-2.0），仅为测试开发依赖；npm ci 会下载锁定驱动，
不下载浏览器。默认使用 /usr/bin/google-chrome，其他安装通过 CHROME_PATH 指定可执行文件。
测试用 HTTP 提供 .wasm/JS，验证跨事件循环的完成、取消、队列丢弃和句柄存活。

cmake --install 可安装静态包；独立消费端位于 tests/web/consumer，使用 Emscripten 工具链配置，
通过 caors_DIR 指向安装的 lib/cmake/caors，并保持 -fexceptions 与 -sDISABLE_EXCEPTION_CATCHING=0。
迁移验证须移走原前缀后重新配置新 consumer，不只复用旧构建缓存。
独立 Emscripten 工作流串行执行上述检查，原生 Linux5/Windows3 矩阵数量不变。
