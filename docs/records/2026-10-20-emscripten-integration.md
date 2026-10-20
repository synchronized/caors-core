<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Emscripten 平台实施与验收

## 范围与提交

用户同意继续后，将规划 PR #52 合入 main，从 0d26e3e0 创建 feature/v0.7.0-emscripten。
工程版本仍为 0.6.0；本批是 [v0.7.0](../versions/v0.7.0.md) 开发，不创建发布标签。

## 实际修复与首批结果

共享快照通过 __cpp_lib_atomic_shared_ptr 选择标准实现；无特化时互斥槽保持拥有者与 CAS 语义。
原生强制回退测试已通过：同地址不同拥有者、别名指针、并发 CAS 400 次更新、旧快照保活、
store 和失败 expected 析构重入。原生标准原子分支在完整构建中继续使用。

Emscripten 5.0.6 首批 atomic/shared_ptr、bind_file_system、allocate_shared 错误在替换快照槽后消失。
继续编译复现 GUID 常量截断及 long double from_chars 重载删除；分别固定哈希位宽、增加能力探测回退。
完整静态库首次构建已通过，未关闭严格警告，也未关闭 assets/meta 等模块。

Web 线程池明确不可构造为可运行池；有异常时构造抛出 system_error，无异常时终止；
动态加载返回 unsupported，原生实现保留。安装包仅在原生目标声明 Threads 依赖。
Web 显式 SHARED/pthread 配置均按预期拒绝；同目录先取消再添加 -pthread 也重新探测并拒绝，
没有复用旧能力缓存。日志见 reject-shared.log、reject-pthread-reconfigure.log。

首批日志位于 build/web-assessment：after-snapshot.log、after-guid.log、after-charconv.log、
platform-configure.log、platform-build.log、native-test.log；早期失败保留，不记为通过。

## 本机协议与生命周期结果

Windows Clang Debug 首轮全量 100/100 通过；随后增加 I/O 终态修复，再编译并通过 I/O、asset_manager、
强制快照回退三个专项；GUID 的固定哈希回归也通过。原生专项日志为 native-fixed-build.log、native-fixed-test.log。

扩大 Web 集合后发现分块读取的丢弃通知无法进入终态。原因是 std::function 移动后的源状态未指定，
libc++ 保留了源回调拥有的循环引用。改为与空槽交换，在锁外释放捕获；新增请求存活期间的回调释放回归。
该失败没有被排除，修复后 Node 59 用例、61,167 断言通过（fixed-protocol-test.log）。
Chrome 155.0.8059.85 实际跨事件循环验证延迟完成、取消、队列丢弃、manager/cache 销毁后句柄保活通过。
工具链为 Emscripten 5.0.6，随附 Node 24.19.0，wasm32 单线程，Debug、严格警告即错误。

## 在验收项目

首轮完整库、独立公共头、Node 和安装/迁移 consumer 通过；最后修复后的公共头重编译及安装迁移正在复核。
新增浮点解析与位宽回归；复用异步 loader、I/O 与路由回归，仅按能力排除确实创建线程的用例，
快照并发测试仍由原生执行。浏览器驱动固定 playwright-core 1.62.1，仅测试使用，锁文件保存来源摘要。
最终代码提交和 CI 链接在验收后补齐，不把进行中的构建写为通过。新增工作流首次以限定特性分支 push 验证，
合并前删除该临时入口，正式触发仍为手动/每周定时。

## 上游与发布边界

本批未修改 Gneiss；其更新锁定版本和删除临时开关仍需上游联合验收。
未验证 pthread、动态插件、WASI/wasm64，不产生稳定 ABI 或性能承诺。
